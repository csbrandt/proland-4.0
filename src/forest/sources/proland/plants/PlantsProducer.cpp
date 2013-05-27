/*
 * Proland: a procedural landscape rendering library.
 * Copyright (c) 2008-2011 INRIA
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Proland is distributed under a dual-license scheme.
 * You can obtain a specific license from Inria: proland-licensing@inria.fr.
 */

/*
 * Authors: Eric Bruneton, Antoine Begault, Guillaume Piolat.
 */

#include "proland/plants/PlantsProducer.h"

#include "ork/core/Factory.h"

#include "proland/math/seg2.h"

namespace proland
{

const int VERTEX_SIZE = 24;

class PlantsTileFilter : public TileSampler::TileFilter
{
public:
    Plants* plants;

    PlantsTileFilter(Plants* plants) : plants(plants)
    {
    }

    virtual ~PlantsTileFilter()
    {
    }

    virtual bool storeTile(ptr<TerrainQuad> q) {
        return q->level == plants->getMaxLevel() || (q->level >= plants->getMinLevel() && q->isLeaf());
    }
};

class GPUBufferTileStorage : public TileStorage
{
public:
    class GPUBufferSlot : public Slot
    {
    public:
        ptr<GPUBuffer> buffer;

        const int offset;

        int size;

        unsigned int date;

        ptr<Query> query;

    private:
        GPUBufferSlot(TileStorage *owner, ptr<GPUBuffer> buffer, int offset) :
            Slot(owner), buffer(buffer), offset(offset), size(-1), date(0)
        {
        }

        friend class GPUBufferTileStorage;
    };

    ptr<GPUBuffer> buffer;

    ptr<MeshBuffers> mesh;

    GPUBufferTileStorage(int tileSize, int nTiles) : TileStorage(tileSize, nTiles)
    {
        buffer = new GPUBuffer();
        buffer->setData(tileSize * nTiles, NULL, DYNAMIC_COPY); // TODO best mode?
        for (int i = 0; i < nTiles; ++i) {
            freeSlots.push_back(new GPUBufferSlot(this, buffer, i * tileSize));
        }
        mesh = new MeshBuffers();
        mesh->addAttributeBuffer(0, 3, VERTEX_SIZE, A32F, false);
        mesh->getAttributeBuffer(0)->setBuffer(buffer);
        mesh->addAttributeBuffer(1, 3, VERTEX_SIZE, A32F, false);
        mesh->getAttributeBuffer(1)->setBuffer(buffer);
    }

    virtual ~GPUBufferTileStorage()
    {
    }
};

ptr<TileCache> createPlantsCache(Plants *plants) {
    ptr<GPUBufferTileStorage> storage = new GPUBufferTileStorage(VERTEX_SIZE * plants->getMaxDensity(), plants->getTileCacheSize());
    return new TileCache(storage, "PlantsCache");
}

static_ptr< Factory<Plants*, ptr<TileCache> > > plantsCacheFactory(new Factory< Plants*, ptr<TileCache> >(createPlantsCache));

map<pair<SceneNode*, Plants*>, PlantsProducer*> PlantsProducer::producers;

PlantsProducer::PlantsProducer(ptr<SceneNode> node, ptr<TerrainNode> terrain, ptr<Plants> plants,
    ptr<TileSampler> lcc, ptr<TileSampler> z, ptr<TileSampler> n, ptr<TileSampler> occ, ptr<TileCache> cache) :
        TileProducer("PlantsProducer", "CreatePlants", cache, false),
        node(node), terrain(terrain), master(NULL), plants(plants), lcc(lcc), z(z), n(n), occ(occ)
{
    offsets = new int[cache->getStorage()->getCapacity()];
    sizes = new int[cache->getStorage()->getCapacity()];
    tileOffsetU = plants->selectProg->getUniform3f("tileOffset");
    tileDeformU = plants->selectProg->getUniformMatrix2f("tileDeform");
    usedTiles = NULL;
    for (int i = plants->getMinLevel(); i < plants->getMaxLevel(); ++i) {
        slaves.push_back(new PlantsProducer(this));
    }
    count = 0;
    total = 0;
    cameraRefPos = vec3d::ZERO;
}

PlantsProducer::PlantsProducer(PlantsProducer *master) :
    TileProducer("PlantsProducer", "CreatePlants", master->getCache(), false), master(master)
{
}

PlantsProducer::~PlantsProducer()
{
    if (master == NULL) {
        if (usedTiles != NULL) {
            usedTiles->recursiveDelete(this);
        }
        producers.erase(make_pair(node.get(), plants.get()));
        plantsCacheFactory->put(plants.get());
    }
}

bool PlantsProducer::hasTile(int level, int tx, int ty)
{
    if (master != NULL) {
        return master->hasTile(level, tx, ty);
    }
    return level >= plants->getMinLevel() && level <= plants->getMaxLevel();
}

void PlantsProducer::produceTiles()
{
    assert(master == NULL);
    localCameraPos = terrain->getLocalCamera();
    vec3d worldCamera = node->getOwner()->getCameraNode()->getWorldPos();
    vec3d deformedCamera = terrain->deform->localToDeformed(localCameraPos);
    mat4d A = terrain->deform->localToDeformedDifferential(localCameraPos);
    mat4d B = terrain->deform->deformedToTangentFrame(worldCamera);
    mat4d ltow = node->getLocalToWorld();
    localToTangentFrame = B * ltow * A;
    tangentFrameToWorld = B.inverse();
    tangentSpaceToWorld = ltow.mat3x3() * terrain->deform->deformedToTangentFrame(deformedCamera).mat3x3().transpose();
    tangentFrameToScreen = node->getOwner()->getWorldToScreen() * tangentFrameToWorld;
    cameraToTangentFrame = B * node->getOwner()->getCameraNode()->getLocalToWorld();
    localToScreen = node->getOwner()->getWorldToScreen() * ltow * A;
    screenToLocal = localToScreen.inverse();
    tangentCameraPos = cameraToTangentFrame * vec3d::ZERO;

    frustumV[0] = (screenToLocal * vec4d(-1.0, -1.0, -1.0, 1.0)).xyzw();
    frustumV[1] = (screenToLocal * vec4d(+1.0, -1.0, -1.0, 1.0)).xyzw();
    frustumV[2] = (screenToLocal * vec4d(-1.0, +1.0, -1.0, 1.0)).xyzw();
    frustumV[3] = (screenToLocal * vec4d(+1.0, +1.0, -1.0, 1.0)).xyzw();
    frustumV[4] = (screenToLocal * vec4d(-1.0, -1.0, +1.0, 1.0)).xyzw() - frustumV[0];
    frustumV[5] = (screenToLocal * vec4d(+1.0, -1.0, +1.0, 1.0)).xyzw() - frustumV[1];
    frustumV[6] = (screenToLocal * vec4d(-1.0, +1.0, +1.0, 1.0)).xyzw() - frustumV[2];
    frustumV[7] = (screenToLocal * vec4d(+1.0, +1.0, +1.0, 1.0)).xyzw() - frustumV[3];
    SceneManager::getFrustumPlanes(localToScreen, frustumP);

    frustumZ = vec4d(localToScreen[3][0], localToScreen[3][1], localToScreen[3][2], localToScreen[3][3]);

    zNear = frustumZ.dotproduct(frustumV[0]);
    zRange = frustumZ.dotproduct(frustumV[4]);

    if (count > 0 && (cameraRefPos.xy() - localCameraPos.xy()).length() > 100000.0) {
        cameraRefPos = localCameraPos;
        invalidateTiles();
        for (unsigned int i = 0; i < slaves.size(); ++i) {
            slaves[i]->invalidateTiles();
        }
    }

    count = 0;
    total = 0;
    plantBounds.clear();
    plantBox = box3d();
    putTiles(&usedTiles, terrain->root);
    getTiles(&usedTiles, terrain->root);
}

ptr<MeshBuffers> PlantsProducer::getPlantsMesh()
{
    return getCache()->getStorage().cast<GPUBufferTileStorage>()->mesh;
}

ptr<PlantsProducer> PlantsProducer::getPlantsProducer(ptr<SceneNode> tn, ptr<Plants> plants)
{
    map<pair<SceneNode*, Plants*>, PlantsProducer*>::iterator i = producers.find(make_pair(tn.get(), plants.get()));
    if (i != producers.end()) {
        return i->second;
    }
    ptr<TerrainNode> t = tn->getField("terrain").cast<TerrainNode>();
    ptr<TileSampler> lcc = tn->getField("lcc").cast<TileSampler>();
    ptr<TileSampler> z = tn->getField("elevation").cast<TileSampler>();
    ptr<TileSampler> n = tn->getField("fnormal").cast<TileSampler>();
    ptr<TileSampler> occ = tn->getField("aperture").cast<TileSampler>();
    if (lcc != NULL) {
        lcc->setStoreFilter(new PlantsTileFilter(plants.get()));
    }
    if (z != NULL) {
        z->setStoreFilter(new PlantsTileFilter(plants.get()));
    }
    if (n != NULL) {
        n->setStoreFilter(new PlantsTileFilter(plants.get()));
    }
    if (occ != NULL) {
        occ->setStoreFilter(new PlantsTileFilter(plants.get()));
    }
    ptr<TileCache> cache = plantsCacheFactory->get(plants.get());
    ptr<PlantsProducer> p = new PlantsProducer(tn, t, plants, lcc, z, n, occ, cache);
    producers.insert(make_pair(make_pair(tn.get(), plants.get()), p.get()));
    return p;
}

bool PlantsProducer::doCreateTile(int level, int tx, int ty, TileStorage::Slot *data)
{
    if (master != NULL) {
        return master->doCreateTile(level, tx, ty, data);
    }
    GPUBufferTileStorage::GPUBufferSlot *slot = dynamic_cast<GPUBufferTileStorage::GPUBufferSlot*>(data);
    assert(level == plants->getMaxLevel());
    assert(slot != NULL);

    if (lcc != NULL) {
        lcc->setTile(level, tx, ty);
    }
    if (z != NULL) {
        z->setTile(level, tx, ty);
    }
    if (n != NULL) {
        n->setTile(level, tx, ty);
    }
    if (occ != NULL) {
        occ->setTile(level, tx, ty);
    }

    if (cameraRefPos == vec3d::ZERO) {
        cameraRefPos = localCameraPos;
    }

    double rootQuadSize = terrain->root->l;
    double ox = rootQuadSize * (float(tx) / (1 << level) - 0.5f);
    double oy = rootQuadSize * (float(ty) / (1 << level) - 0.5f);
    double l = rootQuadSize / (1 << level);
    tileOffsetU->set(vec3f(ox - cameraRefPos.x, oy - cameraRefPos.y, l));

    if (tileDeformU != NULL) {
        mat4d l2d = terrain->deform->localToDeformedDifferential(vec3d(ox + l / 2.0f, oy + l / 2.0f, 0.0f));
        mat4d d2t = terrain->deform->deformedToTangentFrame(l2d * vec3d::ZERO);
        mat4d t2l = l2d.inverse() * d2t.inverse();
        tileDeformU->set(mat2f(t2l[0][0], t2l[0][1], t2l[1][0], t2l[1][1]).coefficients());
    }

    int patternId = int(881 * abs(cos(ox * oy))) % plants->getPatternCount(); // TODO improve this
    ptr<MeshBuffers> pattern = plants->getPattern(patternId);
    int nSeeds = int(pattern->nvertices);

    ptr<TransformFeedback> tfb = TransformFeedback::getDefault();
    tfb->setVertexBuffer(0, slot->buffer, slot->offset, slot->getOwner()->getTileSize());
    slot->size = -1;
    slot->query = new Query(PRIMITIVES_GENERATED);
    slot->query->begin();
    TransformFeedback::begin(SceneManager::getCurrentFrameBuffer(), plants->selectProg, POINTS, tfb, false);
    TransformFeedback::transform(*pattern, 0, nSeeds);
    TransformFeedback::end();
    slot->query->end();

    return true;
}

PlantsProducer::Tree::Tree(int tileCount) : tileCount(tileCount), needTile(false)
{
    needTiles = tileCount == 0 ? NULL : new bool[tileCount];
    tiles = tileCount == 0 ? NULL : new TileCache::Tile*[tileCount];
    for (int i = 0; i < tileCount; ++i) {
        needTiles[i] = false;
        tiles[i] = NULL;
    }
    children[0] = NULL;
    children[1] = NULL;
    children[2] = NULL;
    children[3] = NULL;
}

PlantsProducer::Tree::~Tree()
{
    if (tileCount > 0) {
        delete[] needTiles;
        delete[] tiles;
    }
}

void PlantsProducer::Tree::recursiveDelete(TileProducer *owner)
{
    if (children[0] != NULL) {
        children[0]->recursiveDelete(owner);
        children[1]->recursiveDelete(owner);
        children[2]->recursiveDelete(owner);
        children[3]->recursiveDelete(owner);
    }
    for (int i = 0; i < tileCount; ++i) {
        if (tiles[i] != NULL) {
            owner->putTile(tiles[i]);
        }
    }
    delete this;
}

bool PlantsProducer::mustAmplifyTile(double ox, double oy, double l)
{
    float d = plants->getMaxDistance() * plants->getMaxDistance();
    vec2f c = tangentCameraPos.xy().cast<float>();
    vec2f p1 = (localToTangentFrame * vec3d(ox - localCameraPos.x, oy - localCameraPos.y, 0.0)).xy().cast<float>();
    vec2f p2 = (localToTangentFrame * vec3d(ox + l - localCameraPos.x, oy - localCameraPos.y, 0.0)).xy().cast<float>();
    vec2f p3 = (localToTangentFrame * vec3d(ox - localCameraPos.x, oy + l - localCameraPos.y, 0.0)).xy().cast<float>();
    vec2f p4 = (localToTangentFrame * vec3d(ox + l - localCameraPos.x, oy + l - localCameraPos.y, 0.0)).xy().cast<float>();
    return seg2f(p1, p2).segmentDistSq(c) < d ||
        seg2f(p2, p3).segmentDistSq(c) < d ||
        seg2f(p3, p4).segmentDistSq(c) < d ||
        seg2f(p4, p1).segmentDistSq(c) < d;
}

void PlantsProducer::putTiles(Tree **t, ptr<TerrainQuad> q)
{
    assert(q->level <= plants->getMaxLevel());
    if (*t == NULL) {
        return;
    }

    bool needTile = q->level == plants->getMaxLevel() || (q->level >= plants->getMinLevel() && q->isLeaf());
    needTile &= q->visible != SceneManager::INVISIBLE;
    needTile &= mustAmplifyTile(q->ox, q->oy, q->l);
    (*t)->needTile = needTile;

    if (needTile) {
        int n = 1 << (plants->getMaxLevel() - q->level);
        for (int y = 0; y < n; ++y) {
            for (int x = 0; x < n; ++x) {
                int i = x + y * n;
                double ox = q->ox + x * q->l / n;
                double oy = q->oy + y * q->l / n;
                double l = q->l / n;
                (*t)->needTiles[i] = n == 1 || mustAmplifyTile(ox, oy, l);
                if (!(*t)->needTiles[i] && (*t)->tiles[i] != NULL) {
                    putTile((*t)->tiles[i]);
                    (*t)->tiles[i] = NULL;
                }
            }
        }
    } else {
        for (int i = 0; i < (*t)->tileCount; ++i) {
            if ((*t)->tiles[i] != NULL) {
                putTile((*t)->tiles[i]);
                (*t)->tiles[i] = NULL;
            }
        }
    }

    if (q->children[0] == NULL) {
        if ((*t)->children[0] != NULL) {
            for (int i = 0; i < 4; ++i) {
                (*t)->children[i]->recursiveDelete(this);
                (*t)->children[i] = NULL;
            }
        }
    } else if (q->level < plants->getMaxLevel()) {
        for (int i = 0; i < 4; ++i) {
            putTiles(&((*t)->children[i]), q->children[i]);
        }
    }
}

void PlantsProducer::getTiles(Tree **t, ptr<TerrainQuad> q)
{
    assert(q->level <= plants->getMaxLevel());
    if (*t == NULL) {
        *t = new Tree(q->level < plants->getMinLevel() ? 0 : 1 << 2 * (plants->getMaxLevel() - q->level));

        bool needTile = q->level == plants->getMaxLevel() || (q->level >= plants->getMinLevel() && q->isLeaf());
        needTile &= q->visible != SceneManager::INVISIBLE;
        needTile &= mustAmplifyTile(q->ox, q->oy, q->l);
        (*t)->needTile = needTile;

        if (needTile) {
            int n = 1 << (plants->getMaxLevel() - q->level);
            for (int y = 0; y < n; ++y) {
                for (int x = 0; x < n; ++x) {
                    int i = x + y * n;
                    double ox = q->ox + x * q->l / n;
                    double oy = q->oy + y * q->l / n;
                    double l = q->l / n;
                    (*t)->needTiles[i] = n == 1 || mustAmplifyTile(ox, oy, l);
                }
            }
        }
    }

    if ((*t)->needTile) {
        int n = 1 << (plants->getMaxLevel() - q->level);
        for (int y = 0; y < n; ++y) {
            int ty = q->ty * n + y;
            for (int x = 0; x < n; ++x) {
                int i = x + y * n;
                int tx = q->tx * n + x;
                if ((*t)->needTiles[i]) {
                    if ((*t)->tiles[i] == NULL) {
                        if (q->level == plants->getMaxLevel()) {
                            (*t)->tiles[i] = getTile(plants->getMaxLevel(), tx, ty, 0);
                        } else {
                            (*t)->tiles[i] = slaves[plants->getMaxLevel() - q->level - 1]->getTile(plants->getMaxLevel(), tx, ty, 0);
                        }
                        if ((*t)->tiles[i] == NULL && Logger::ERROR_LOGGER != NULL) {
                            Logger::ERROR_LOGGER->log("TERRAIN", "Insufficient tile cache size for plants");
                        }
                    }
                    assert((*t)->tiles[i] != NULL);
                    ptr<Task> task = (*t)->tiles[i]->task;

                    unsigned int completionDate = 0;
                    if (lcc != NULL) {
                        TileCache::Tile *u = NULL;
                        lcc->get()->getGpuTileCoords(plants->getMaxLevel(), tx, ty, &u);
                        if (u != NULL && u->task != NULL) {
                            completionDate = u->task->getCompletionDate();
                        }
                    }
                    if (task->isDone() && dynamic_cast<GPUBufferTileStorage::GPUBufferSlot*>((*t)->tiles[i]->getData(false))->date < completionDate) {
                        task->setIsDone(false, 0, Task::DATA_CHANGED);
                    }

                    if (!task->isDone()) {
                        task->run();
                        task->setIsDone(true, 0);
                        dynamic_cast<GPUBufferTileStorage::GPUBufferSlot*>((*t)->tiles[i]->getData(false))->date = completionDate;
                    }
                    GPUBufferTileStorage::GPUBufferSlot *s = dynamic_cast<GPUBufferTileStorage::GPUBufferSlot*>((*t)->tiles[i]->getData(false));
                    if (s->size < 0 /*&& s->query->available()*/) { // uncomment for fully asynchronous mode
                        s->size = s->query->getResult();
                        s->query = NULL;
                    }
                    if (s->size > 0) {
                        offsets[count] = s->offset / VERTEX_SIZE;
                        sizes[count] = s->size;
                        count += 1;
                        total += s->size;
                        updateTerrainHeights(q);
                    }
                }
            }
        }
    }

    if (q->children[0] != NULL && q->level < plants->getMaxLevel()) {
        for (int i = 0; i < 4; ++i) {
            getTiles(&((*t)->children[i]), q->children[i]);
        }
    }
}

void PlantsProducer::updateTerrainHeights(ptr<TerrainQuad> q)
{
    double xmin = q->ox - localCameraPos.x;
    double xmax = q->ox - localCameraPos.x + q->l;
    double ymin = q->oy - localCameraPos.y;
    double ymax = q->oy - localCameraPos.y + q->l;
    double zmin = q->zmin;
    double zmax = q->zmax + 15.0; // maxTreeHeight (TODO remove this hardcoded constant)

    plantBox = plantBox.enlarge(box3d(xmin, xmax, ymin, ymax, zmin, zmax));

    double zMin = INFINITY;
    double zMax = 0.0;

    vec3d V[8];
    double Z[8];
    for (int i = 0; i < 8; ++i) {
        double x = i % 2 == 0 ? xmin : xmax;
        double y = (i / 2) % 2 == 0 ? ymin : ymax;
        double z = (i / 4) % 2 == 0 ? zmin : zmax;
        V[i] = vec3d(x, y, z);
        Z[i] = frustumZ.dotproduct(V[i]);
    }

    double prods[8][5];
    int I[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

    for (int j = 0; j < 5; ++j) {
        int J = 0;
        for (int i = 0; i < 8; ++i) {
            double p = frustumP[j].dotproduct(V[i]);
            int in = p >= 0.0 ? 1.0 : 0.0;
            prods[i][j] = p;
            I[i] += in;
            J += in;
        }
        if (J == 0) {
            // bbox fully outside frustum
            return;
        }
    }

    int N = 0;
    for (int i = 0; i < 8; ++i) {
        if (I[i] == 5) {
            // bbox vertice inside frustum
            double z = Z[i];
            zMin = std::min(zMin, z);
            zMax = std::max(zMax, z);
            N += 1;
        }
    }

    if (N == 8) {
        // bbox fully inside frustum
        plantBounds.push_back(vec4d(zMin, zMax, zmin, zmax));
        return;
    }

    const int segments[24] = { 0, 1, 1, 3, 3, 2, 2, 0, 4, 5, 5, 7, 7, 6, 6, 4, 0, 4, 1, 5, 3, 7, 2, 6 };
    for (int i = 0; i < 24; i += 2) {
        int a = segments[i];
        int b = segments[i + 1];
        if (I[a] < 5 || I[b] < 5) {
            double tIn = 0.0;
            double tOut = 1.0;
            for (int j = 0; j < 5; ++j) {
                double p = prods[a][j] - prods[b][j];
                if (p < 0.0) {
                    tIn = std::max(tIn, prods[a][j] / p);
                } else if (p > 0.0) {
                    tOut = std::min(tOut, prods[a][j] / p);
                }
            }
            if (tIn <= tOut && tIn < 1.0 && tOut > 0.0) {
                double zIn = Z[a] * (1.0 - tIn) + Z[b] * tIn;
                double zOut = Z[a] * (1.0 - tOut) + Z[b] * tOut;
                zMin = std::min(zMin, std::min(zIn, zOut));
                zMax = std::max(zMax, std::max(zIn, zOut));
            }
        }
    }

    for (int i = 0; i < 4; ++i) {
        int j = i + 4;
        double tIn = 0.0;
        double tOut = 1.0;
        tIn = std::max(tIn, ((frustumV[j].x < 0.0 ? xmax : xmin) - frustumV[i].x) / frustumV[j].x);
        tIn = std::max(tIn, ((frustumV[j].y < 0.0 ? ymax : ymin) - frustumV[i].y) / frustumV[j].y);
        tIn = std::max(tIn, ((frustumV[j].z < 0.0 ? zmax : zmin) - frustumV[i].z) / frustumV[j].z);
        tOut = std::min(tOut, ((frustumV[j].x < 0.0 ? xmin : xmax) - frustumV[i].x) / frustumV[j].x);
        tOut = std::min(tOut, ((frustumV[j].y < 0.0 ? ymin : ymax) - frustumV[i].y) / frustumV[j].y);
        tOut = std::min(tOut, ((frustumV[j].z < 0.0 ? zmin : zmax) - frustumV[i].z) / frustumV[j].z);
        if (tIn <= tOut && tIn < 1.0 && tOut > 0.0) {
            double zIn = zNear + zRange * tIn;
            double zOut = zNear + zRange * tOut;
            zMin = std::min(zMin, std::min(zIn, zOut));
            zMax = std::max(zMax, std::max(zIn, zOut));
        }
    }

    if (zMin < zMax) {
        plantBounds.push_back(vec4d(zMin, zMax, zmin, zmax));
    }
}

}
