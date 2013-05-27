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

#include "proland/terrain/TileSampler.h"

#include "ork/resource/ResourceTemplate.h"
#include "proland/producer/GPUTileStorage.h"
#include "proland/terrain/TerrainNode.h"

using namespace std;
using namespace ork;

namespace proland
{

class UpdateTileMapTask : public Task
{
public:
    ptr<TileProducer> producer;

    float splitDistance;

    vec2f camera;

    int depth;

    UpdateTileMapTask(ptr<TileProducer> producer, float splitDistance, vec2f camera, int depth) :
        Task("UpdateTileMapTask", true, 0), producer(producer), splitDistance(splitDistance), camera(camera), depth(depth)
    {
    }

    virtual bool run() {
        producer->updateTileMap(splitDistance, camera, depth);
        return true;
    }
};

TileSampler::TileSampler(const string &name, ptr<TileProducer> producer) :
    Object("TileSampler")
{
    init(name, producer);
}

TileSampler::TileSampler() : Object("TileSampler")
{
}

void TileSampler::init(const string &name, ptr<TileProducer> producer)
{
    this->name = name;
    this->producer = producer;
    this->root = NULL;
    this->storeLeaf = true;
    this->storeParent = true;
    this->storeInvisible = true;
    this->async = false;
    this->mipmap = false;
    ptr<GPUTileStorage> storage = producer->getCache()->getStorage().cast<GPUTileStorage>();
    assert(storage != NULL);
    lastProgram = NULL;
}

TileSampler::~TileSampler()
{
    if (root != NULL) {
        root->recursiveDelete(this);
    }
    for (unsigned int i = 0; i < storeFilters.size(); ++i) {
        delete storeFilters[i];
    }
}

ptr<TileProducer> TileSampler::get()
{
    return producer;
}

ptr<TerrainNode> TileSampler::getTerrain(int i)
{
    return i < int(terrains.size()) ? terrains[i] : NULL;
}

bool TileSampler::getStoreLeaf()
{
    return storeLeaf;
}

bool TileSampler::getAsync()
{
    return async;
}

bool TileSampler::getMipMap()
{
    return mipmap;
}

void TileSampler::set(ptr<TileProducer> producer)
{
    this->producer = producer;
}

void TileSampler::addTerrain(ptr<TerrainNode> terrain)
{
    terrains.push_back(terrain);
}

void TileSampler::setStoreLeaf(bool storeLeaf)
{
    this->storeLeaf = storeLeaf;
}

void TileSampler::setStoreParent(bool storeParent)
{
    this->storeParent = storeParent;
}

void TileSampler::setStoreInvisible(bool storeInvisible)
{
    this->storeInvisible = storeInvisible;
}

void TileSampler::setStoreFilter(TileFilter *filter)
{
    storeFilters.push_back(filter);
}

void TileSampler::setAsynchronous(bool async)
{
    this->async = async;
    assert(!async || storeParent);
}

void TileSampler::setMipMap(bool mipmap)
{
    this->mipmap = mipmap;
}

void TileSampler::checkUniforms()
{
    ptr<Program> p = SceneManager::getCurrentProgram();
    if (p != lastProgram) {
        samplerU = p->getUniformSampler(name + ".tilePool");
        coordsU = p->getUniform3f(name + ".tileCoords");
        sizeU = p->getUniform3f(name + ".tileSize");
        tileMapU = p->getUniformSampler(name + ".tileMap");
        quadInfoU = p->getUniform4f(name + ".quadInfo");
        poolInfoU = p->getUniform4f(name + ".poolInfo");

        cameraU.clear();
        for (unsigned int i = 0; i < terrains.size(); ++i) {
            ostringstream oss;
            oss << name << ".camera[" << i << "]";
            cameraU.push_back(p->getUniform4f(oss.str()));
        }
        lastProgram = p;
    }
}

void TileSampler::setTile(int level, int tx, int ty)
{
    checkUniforms();
    if (samplerU == NULL) {
        return;
    }
    TileCache::Tile *t = NULL;
    int b = producer->getBorder();
    int s = producer->getCache()->getStorage()->getTileSize();

    float dx = 0;
    float dy = 0;
    float dd = 1;
    float ds0 = (s / 2) * 2.0f - 2.0f * b;
    float ds = ds0;
    while (!producer->hasTile(level, tx, ty)) {
        dx += (tx % 2) * dd;
        dy += (ty % 2) * dd;
        dd *= 2;
        ds /= 2;
        level -= 1;
        tx /= 2;
        ty /= 2;
        assert(level >= 0);
    }

    Tree *tt = root;
    Tree *tc;
    int tl = 0;
    while (tl != level && (tc = tt->children[((tx >> (level - tl - 1)) & 1) | ((ty >> (level - tl - 1)) & 1) << 1]) != NULL) {
        tl += 1;
        tt = tc;
    }
    while (level > tl) {
        dx += (tx % 2) * dd;
        dy += (ty % 2) * dd;
        dd *= 2;
        ds /= 2;
        level -= 1;
        tx /= 2;
        ty /= 2;
    }
    t = tt->t;

    while (t == NULL) {
        dx += (tx % 2) * dd;
        dy += (ty % 2) * dd;
        dd *= 2;
        ds /= 2;
        level -= 1;
        tx /= 2;
        ty /= 2;
        tt = tt->parent;
        assert(tt != NULL);
        t = tt->t;
    }

    dx = dx * ((s / 2) * 2 - 2 * b) / dd;
    dy = dy * ((s / 2) * 2 - 2 * b) / dd;

    producer->getCache()->getStorage().cast<GPUTileStorage>()->generateMipMap();
    GPUTileStorage::GPUSlot *gput = dynamic_cast<GPUTileStorage::GPUSlot*>(t->getData());
    assert(gput != NULL);

    float w = gput->getWidth();
    float h = gput->getHeight();
    assert(w == h);

    vec4f coords;
    if (s%2 == 0) {
        coords = vec4f((dx + b) / w, (dy + b) / h, float(gput->l), ds / w);
    } else {
        coords = vec4f((dx + b + 0.5f) / w, (dy + b + 0.5f) / h, float(gput->l), ds / w);
    }

    samplerU->set(gput->t);
    coordsU->set(vec3f(coords.x, coords.y, coords.z));
    sizeU->set(vec3f(coords.w, coords.w, (s / 2) * 2.0f - 2.0f * b));
}

void TileSampler::setTileMap()
{
    if (terrains.size() == 0) {
        return;
    }
    checkUniforms();
    if (samplerU == NULL) {
        return;
    }
    ptr<GPUTileStorage> storage = producer->getCache()->getStorage().cast<GPUTileStorage>();
    if (storage->getTileMap() != NULL) {
        storage->generateMipMap();
        ptr<Texture> tilePool = storage->getTexture(0);
        ptr<TerrainNode> n = terrains[0];
        int maxLevel = n->maxLevel;
        while (!producer->hasTile(maxLevel, 0, 0)) {
            --maxLevel;
        }

        float k = ceil(n->getSplitDistance());
        samplerU->set(storage->getTexture(0));
        tileMapU->set(storage->getTileMap());
        quadInfoU->set(vec4f((float) n->root->l, n->getSplitDistance(), 2.0f * k, 4.0f * k + 2.0f));
        float w;
        float h;
        if (tilePool.cast<Texture2D>() != NULL) {
            w = tilePool.cast<Texture2D>()->getWidth();
            h = tilePool.cast<Texture2D>()->getHeight();
        } else {
            w = tilePool.cast<Texture2DArray>()->getWidth();
            h = tilePool.cast<Texture2DArray>()->getHeight();
        }
        poolInfoU->set(vec4f(float(storage->getTileSize()), float(producer->getBorder()), 1.0f / w, 1.0f / h));
        for (unsigned int i = 0; i < terrains.size(); ++i) {
            vec3d camera = terrains[i]->getLocalCamera();
            cameraU[i]->set(vec4d(camera.x - n->root->ox, camera.y - n->root->oy, (camera.z - TerrainNode::groundHeightAtCamera) / n->getDistFactor(), maxLevel).cast<float>());
        }
    }
}

ptr<Task> TileSampler::update(ptr<SceneManager> scene, ptr<TerrainQuad> root)
{
    ptr<TaskGraph> result = new TaskGraph();
    if (terrains.size() == 0) {
        producer->update(scene);
        if (storeInvisible) {
            root->getOwner()->splitInvisibleQuads = true;
        }
        if (!async && storeLeaf && this->root != NULL) {
            int prefetchCount = producer->getCache()->getUnusedTiles() + producer->getCache()->getStorage()->getFreeSlots();
            prefetch(this->root, root, prefetchCount);
        }
        putTiles(&(this->root), root);
        getTiles(NULL, &(this->root), root, result);

        ptr<GPUTileStorage> storage = producer->getCache()->getStorage().cast<GPUTileStorage>();
        if (storage->getTileMap() != NULL) {
            ptr<TerrainNode> n = root->getOwner();
            vec3d camera = n->getLocalCamera();
            ptr<Task> t = new UpdateTileMapTask(producer, n->getSplitDistance(), vec2f((float) camera.x, (float) camera.y), root->getDepth());
            if (result->isEmpty()) {
                t->run();
            } else {
                ptr<TaskGraph> graph = new TaskGraph();
                graph->addTask(result);
                graph->addTask(t);
                graph->addDependency(t, result);
                result = graph;
            }
        }
    }
    return result;
}

TileSampler::Tree::Tree(Tree *parent) : newTree(true), needTile(false), parent(parent), t(NULL)
{
    children[0] = NULL;
    children[1] = NULL;
    children[2] = NULL;
    children[3] = NULL;
}

TileSampler::Tree::~Tree()
{
}

void TileSampler::Tree::recursiveDelete(TileSampler *owner)
{
    if (t != NULL) {
        owner->producer->putTile(t);
        t = NULL;
    }
    if (children[0] != NULL) {
        children[0]->recursiveDelete(owner);
        children[1]->recursiveDelete(owner);
        children[2]->recursiveDelete(owner);
        children[3]->recursiveDelete(owner);
    }
    delete this;
}

bool TileSampler::needTile(ptr<TerrainQuad> q)
{
    bool needTile = storeLeaf;
    if (!storeParent && (q->children[0] != NULL) && (producer->hasChildren(q->level, q->tx, q->ty))) {
        needTile = false;
    }
    if (!needTile) {
        for (unsigned int i = 0; i < storeFilters.size(); ++i) {
            if (storeFilters[i]->storeTile(q)) {
                needTile = true;
                break;
            }
        }
    }
    if (!storeInvisible && q->visible == SceneManager::INVISIBLE) {
        needTile = false;
    }
    return needTile;
}

void TileSampler::putTiles(Tree **t, ptr<TerrainQuad> q)
{
    if (*t == NULL) {
        return;
    }

    assert(producer->hasTile(q->level, q->tx, q->ty));

    (*t)->needTile = needTile(q);

    if (!(*t)->needTile) {
        if ((*t)->t != NULL) {
            producer->putTile((*t)->t);
            (*t)->t = NULL;
        }
    }

    if (q->children[0] == NULL) {
        if ((*t)->children[0] != NULL) {
            for (int i = 0; i < 4; ++i) {
                (*t)->children[i]->recursiveDelete(this);
                (*t)->children[i] = NULL;
            }
        }
    } else if (producer->hasChildren(q->level, q->tx, q->ty)) {
        for (int i = 0; i < 4; ++i) {
            putTiles(&((*t)->children[i]), q->children[i]);
        }
    }
}

void TileSampler::getTiles(Tree *parent, Tree **t, ptr<TerrainQuad> q, ptr<TaskGraph> result)
{
    if (*t == NULL) {
        *t = new Tree(parent);
        (*t)->needTile = needTile(q);
        if (q->level == 0 && producer->getRootQuadSize() == 0.0f) {
            producer->setRootQuadSize((float) q->l);
        }
    }

    assert(producer->hasTile(q->level, q->tx, q->ty));

    if ((*t)->needTile) {
        if ((*t)->t == NULL) {
            if (async && q->level > 0) {
                (*t)->t = producer->findTile(q->level, q->tx, q->ty, true);
                if ((*t)->t == NULL) {
                    if (q->isLeaf()) {
                        producer->prefetchTile(q->level, q->tx, q->ty);
                    }
                } else {
                    (*t)->t = producer->getTile(q->level, q->tx, q->ty, 0);
                    assert((*t)->t != NULL);
                }
            } else {
                (*t)->t = producer->getTile(q->level, q->tx, q->ty, 0);
                if ((*t)->t == NULL && Logger::ERROR_LOGGER != NULL) {
                    Logger::ERROR_LOGGER->log("TERRAIN", "Insufficient tile cache size for '" + name + "' uniform");
                }
                assert((*t)->t != NULL);
            }
        }
        if ((*t)->t != NULL) {
            ptr<Task> tt = (*t)->t->task;
            if (!(*t)->t->task->isDone()) {
                result->addTask((*t)->t->task);
            }
        }
    }

    if (q->children[0] != NULL && producer->hasChildren(q->level, q->tx, q->ty)) {
        for (int i = 0; i < 4; ++i) {
            getTiles(*t, &((*t)->children[i]), q->children[i], result);
        }
    }
}

void TileSampler::prefetch(Tree *t, ptr<TerrainQuad> q, int &prefetchCount)
{
    if (t->children[0] == NULL) {
        if (t->newTree && q != NULL) {
            if ((storeInvisible || q->visible != SceneManager::INVISIBLE) && producer->hasChildren(q->level, q->tx, q->ty)) {
                if (prefetchCount > 0) {
                    if (producer->prefetchTile(q->level + 1, 2 * q->tx, 2 * q->ty)) {
                        --prefetchCount;
                    }
                }
                if (prefetchCount > 0) {
                    if (producer->prefetchTile(q->level + 1, 2 * q->tx + 1, 2 * q->ty)) {
                        --prefetchCount;
                    }
                }
                if (prefetchCount > 0) {
                    if (producer->prefetchTile(q->level + 1, 2 * q->tx, 2 * q->ty + 1)) {
                        --prefetchCount;
                    }
                }
                if (prefetchCount > 0) {
                    if (producer->prefetchTile(q->level + 1, 2 * q->tx + 1, 2 * q->ty + 1)) {
                        --prefetchCount;
                    }
                }
            }
        }
    } else {
        for (int i = 0; i < 4; ++i) {
            prefetch(t->children[i], q == NULL ? NULL : q->children[i], prefetchCount);
        }
    }
    t->newTree = false;
}

void TileSampler::swap(ptr<TileSampler> p)
{
    std::swap(name, p->name);
    std::swap(producer, p->producer);
    std::swap(terrains, p->terrains);
//    std::swap(block, p->block);
    std::swap(samplerU, p->samplerU);
    std::swap(coordsU, p->coordsU);
    std::swap(sizeU, p->sizeU);
    std::swap(tileMapU, p->tileMapU);
    std::swap(quadInfoU, p->quadInfoU);
    std::swap(poolInfoU, p->poolInfoU);
    std::swap(cameraU, p->cameraU);
    std::swap(storeLeaf, p->storeLeaf);
    std::swap(storeParent, p->storeParent);
    std::swap(storeInvisible, p->storeInvisible);
    std::swap(storeFilters, p->storeFilters);
    std::swap(async, p->async);
    std::swap(mipmap, p->mipmap);
}

class TileSamplerResource : public ResourceTemplate<10, TileSampler>
{
public:
    TileSamplerResource(ptr<ResourceManager> manager, const string &name, ptr<ResourceDescriptor> desc, const TiXmlElement *e = NULL) :
        ResourceTemplate<10, TileSampler>(manager, name, desc)
    {
        e = e == NULL ? desc->descriptor : e;
        checkParameters(desc, e, "id,name,sampler,producer,terrains,storeLeaf,storeParent,storeInvisible,async,mipmap,");
        string uname;
        ptr<TileProducer> producer;
        uname = getParameter(desc, e, "sampler");
        producer = manager->loadResource(getParameter(desc, e, "producer")).cast<TileProducer>();
        init(uname, producer);
        if (e->Attribute("terrains") != NULL) {
            string nodes = string(e->Attribute("terrains"));
            string::size_type start = 0;
            string::size_type index;
            while ((index = nodes.find(',', start)) != string::npos) {
                string node = nodes.substr(start, index - start);
                addTerrain(manager->loadResource(node).cast<TerrainNode>());
                start = index + 1;
            }
        }
        if (e->Attribute("storeLeaf") != NULL && strcmp(e->Attribute("storeLeaf"), "false") == 0) {
            setStoreLeaf(false);
        }
        if (e->Attribute("storeParent") != NULL && strcmp(e->Attribute("storeParent"), "false") == 0) {
            setStoreParent(false);
        }
        if (e->Attribute("storeInvisible") != NULL && strcmp(e->Attribute("storeInvisible"), "false") == 0) {
            setStoreInvisible(false);
        }
        if (e->Attribute("async") != NULL && strcmp(e->Attribute("async"), "true") == 0) {
            setAsynchronous(true);
        }
        if (e->Attribute("mipmap") != NULL && strcmp(e->Attribute("mipmap"), "true") == 0) {
            setMipMap(true);
        }
    }
};

extern const char tileSampler[] = "tileSampler";

static ResourceFactory::Type<tileSampler, TileSamplerResource> TileSamplerType;

}
