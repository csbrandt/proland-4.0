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

#include "proland/terrain/TileSamplerZ.h"

#include "ork/resource/ResourceTemplate.h"
#include "ork/scenegraph/SetTargetTask.h"
#include "proland/producer/GPUTileStorage.h"
#include "proland/terrain/TerrainNode.h"

using namespace std;
using namespace ork;

namespace proland
{

#define MAX_MIPMAP_PER_FRAME 16

const char *minmaxShader = "\
uniform vec4 viewport; // size in pixels and one over size in pixels\n\
#ifdef _VERTEX_\n\
layout(location=0) in vec4 vertex;\n\
void main() {\n\
    gl_Position = vec4((vertex.xy + vec2(1.0)) * viewport.xy - vec2(1.0), 0.0, 1.0);\n\
}\n\
#endif\n\
#ifdef _FRAGMENT_\n\
uniform vec3 sizes; // size of parent and current tiles in pixels, pass\n\
uniform ivec4 tiles[32];\n\
uniform sampler2DArray inputs[8];\n\
uniform sampler2D input_;\n\
layout(location=0) out vec4 data;\n\
void main() {\n\
    vec2 r[16];\n\
    vec2 ij = floor(gl_FragCoord.xy);\n\
    if (sizes.z == 0.0) {\n\
        ivec4 tile = tiles[int(floor(ij.x / sizes.y))];\n\
        vec4 uv = (tile.z == 0 && tile.w == 0) ? vec4(vec2(2.5) + 4.0 * mod(ij, sizes.yy), vec2(sizes.x - 2.5)) : tile.zwzw + vec4(0.5);\n\
        vec4 u = min(vec4(uv.x, uv.x + 1.0, uv.x + 2.0, uv.x + 3.0), uv.zzzz) / sizes.x;\n\
        vec4 v = min(vec4(uv.y, uv.y + 1.0, uv.y + 2.0, uv.y + 3.0), uv.wwww) / sizes.x;\n\
        switch (tile.x) {\n\
        case 0:\n\
            for (int i = 0; i < 16; ++i) {\n\
                r[i] = textureLod(inputs[0], vec3(u[i/4], v[i%4], tile.y), 0.0).zz;\n\
            }\n\
            break;\n\
        case 1:\n\
            for (int i = 0; i < 16; ++i) {\n\
                r[i] = textureLod(inputs[1], vec3(u[i/4], v[i%4], tile.y), 0.0).zz;\n\
            }\n\
            break;\n\
        case 2:\n\
            for (int i = 0; i < 16; ++i) {\n\
                r[i] = textureLod(inputs[2], vec3(u[i/4], v[i%4], tile.y), 0.0).zz;\n\
            }\n\
            break;\n\
        case 3:\n\
            for (int i = 0; i < 16; ++i) {\n\
                r[i] = textureLod(inputs[3], vec3(u[i/4], v[i%4], tile.y), 0.0).zz;\n\
            }\n\
            break;\n\
        case 4:\n\
            for (int i = 0; i < 16; ++i) {\n\
                r[i] = textureLod(inputs[4], vec3(u[i/4], v[i%4], tile.y), 0.0).zz;\n\
            }\n\
            break;\n\
        case 5:\n\
            for (int i = 0; i < 16; ++i) {\n\
                r[i] = textureLod(inputs[5], vec3(u[i/4], v[i%4], tile.y), 0.0).zz;\n\
            }\n\
            break;\n\
        case 6:\n\
            for (int i = 0; i < 16; ++i) {\n\
                r[i] = textureLod(inputs[6], vec3(u[i/4], v[i%4], tile.y), 0.0).zz;\n\
            }\n\
            break;\n\
        case 7:\n\
            for (int i = 0; i < 16; ++i) {\n\
                r[i] = textureLod(inputs[7], vec3(u[i/4], v[i%4], tile.y), 0.0).zz;\n\
            }\n\
            break;\n\
        }\n\
    } else {\n\
        vec2 tile = floor(ij / sizes.y);\n\
        vec2 uvmax = vec2(tile * sizes.x + vec2(sizes.x - 0.5));\n\
        vec2 uv = vec2(0.5) + tile * sizes.x + 4.0 * (ij - tile * sizes.y);\n\
        vec4 u = min(vec4(uv.x, uv.x + 1.0, uv.x + 2.0, uv.x + 3.0), uvmax.xxxx) * viewport.z;\n\
        vec4 v = min(vec4(uv.y, uv.y + 1.0, uv.y + 2.0, uv.y + 3.0), uvmax.yyyy) * viewport.w;\n\
        for (int i = 0; i < 16; ++i) {\n\
            r[i] = textureLod(input_, vec2(u[i/4], v[i%4]), 0.0).xy;\n\
        }\n\
    }\n\
    vec2 s0 = vec2(min(r[0].x, r[1].x), max(r[0].y, r[1].y));\n\
    vec2 s1 = vec2(min(r[2].x, r[3].x), max(r[2].y, r[3].y));\n\
    vec2 s2 = vec2(min(r[4].x, r[5].x), max(r[4].y, r[5].y));\n\
    vec2 s3 = vec2(min(r[6].x, r[7].x), max(r[6].y, r[7].y));\n\
    vec2 s4 = vec2(min(r[8].x, r[9].x), max(r[8].y, r[9].y));\n\
    vec2 s5 = vec2(min(r[10].x, r[11].x), max(r[10].y, r[11].y));\n\
    vec2 s6 = vec2(min(r[12].x, r[13].x), max(r[12].y, r[13].y));\n\
    vec2 s7 = vec2(min(r[14].x, r[15].x), max(r[14].y, r[15].y));\n\
    vec2 t0 = vec2(min(s0.x, s1.x), max(s0.y, s1.y));\n\
    vec2 t1 = vec2(min(s2.x, s3.x), max(s2.y, s3.y));\n\
    vec2 t2 = vec2(min(s4.x, s5.x), max(s4.y, s5.y));\n\
    vec2 t3 = vec2(min(s6.x, s7.x), max(s6.y, s7.y));\n\
    vec2 u0 = vec2(min(t0.x, t1.x), max(t0.y, t1.y));\n\
    vec2 u1 = vec2(min(t2.x, t3.x), max(t2.y, t3.y));\n\
    data = vec4(min(u0.x, u1.x), max(u0.y, u1.y), 0.0, 0.0);\n\
}\n\
#endif\n";

TileSamplerZ::TreeZ::TreeZ(Tree *parent, ptr<TerrainQuad> q) :
    Tree(parent), q(q), readback(false), readbackDate(0)
{
}

void TileSamplerZ::TreeZ::recursiveDelete(TileSampler *owner)
{
    dynamic_cast<TileSamplerZ*>(owner)->state->needReadback.erase(this);
    Tree::recursiveDelete(owner);
}

bool TileSamplerZ::TreeZSort::operator()(const TreeZ *x, const TreeZ *y) const
{
    unsigned int xLevel = x->q->level;
    unsigned int yLevel = y->q->level;
    if (xLevel == yLevel) {
        return x < y;
    } else {
        return xLevel < yLevel;
    }
}

TileSamplerZ::TileCallback::TileCallback(vector< ptr<TerrainQuad> > &targets, bool camera) :
    Callback(), targets(targets), camera(camera)
{
}

void TileSamplerZ::TileCallback::dataRead(volatile void *data)
{
    float *values = (float*) data;
    unsigned int i = 0;
    if (camera) {
        TerrainNode::groundHeightAtCamera = TerrainNode::nextGroundHeightAtCamera;
        TerrainNode::nextGroundHeightAtCamera = values[0];
        i = 1;
    }
    for (; i < targets.size(); ++i) {
        targets[i]->zmin = values[2*i];
        targets[i]->zmax = values[2*i+1];
    }
}

ptr<TileSamplerZ::State> TileSamplerZ::newState(ptr<GPUTileStorage> storage)
{
    return new TileSamplerZ::State(storage);
}

static_ptr< Factory< ptr<GPUTileStorage>, ptr<TileSamplerZ::State> > >
    TileSamplerZ::stateFactory(new Factory< ptr<GPUTileStorage>, ptr<TileSamplerZ::State> >(TileSamplerZ::newState));

TileSamplerZ::State::State(ptr<GPUTileStorage> storage) :
    Object("TileSamplerZ::State"), storage(storage), cameraSlot(NULL), lastFrame(0)
{
    assert(storage->getTextureCount() < 8);
    int tileSize = storage->getTileSize();
    int h = (tileSize - 4) / 4 + (tileSize % 4 == 0 ? 0 : 1);
    int w = MAX_MIPMAP_PER_FRAME * h;
    fbo = new FrameBuffer();
    fbo->setViewport(vec4i(0, 0, w, h));
    fbo->setTextureBuffer(COLOR0, new Texture2D(w, h, RG32F, RG, FLOAT,
            Texture::Parameters().min(NEAREST).mag(NEAREST), Buffer::Parameters(), CPUBuffer()), 0);
    fbo->setTextureBuffer(COLOR1, new Texture2D(w, h, RG32F, RG, FLOAT,
            Texture::Parameters().min(NEAREST).mag(NEAREST), Buffer::Parameters(), CPUBuffer()), 0);
    int pass = 0;
    while (h != 1) {
        h = h / 4 + (h % 4 == 0 ? 0 : 1);
        pass += 1;
    }
    readBuffer = pass % 2 == 0 ? COLOR0 : COLOR1;
    fbo->setReadBuffer(readBuffer);

    minmaxProg = new Program(new Module(330, minmaxShader));
    viewportU = minmaxProg->getUniform4f("viewport");
    sizesU = minmaxProg->getUniform3f("sizes");
    inputU = minmaxProg->getUniformSampler("input_");
    ptr<Sampler> s = new Sampler(Sampler::Parameters().min(NEAREST).mag(NEAREST));
    char buf[256];
    for (int i = 0; i < storage->getTextureCount(); ++i) {
        sprintf(buf, "inputs[%d]", i);
        minmaxProg->getUniformSampler(string(buf))->set(storage->getTexture(i));
        minmaxProg->getUniformSampler(string(buf))->setSampler(s);
    }
    for (int i = 0; i < MAX_MIPMAP_PER_FRAME; ++i) {
        sprintf(buf, "tiles[%d]", i);
        tileU.push_back(minmaxProg->getUniform4i(string(buf)));
    }

    tileReadback = new ReadbackManager(1, 3, MAX_MIPMAP_PER_FRAME * 2 * sizeof(float));
}

TileSamplerZ::TileSamplerZ(const string &name, ptr<TileProducer> producer) :
    TileSampler(name, producer)
{
}

TileSamplerZ::TileSamplerZ() : TileSampler()
{
}

void TileSamplerZ::init(const string &name, ptr<TileProducer> producer)
{
    TileSampler::init(name, producer);
    factory = stateFactory;
    state = factory->get(get()->getCache()->getStorage().cast<GPUTileStorage>());
    cameraQuad = NULL;
    oldLocalCamera = vec3d::ZERO;
}

TileSamplerZ::~TileSamplerZ()
{
    if (root != NULL) {
        root->recursiveDelete(this);
        root = NULL;
    }
    factory->put(get()->getCache()->getStorage().cast<GPUTileStorage>());
    factory = NULL;
}

ptr<Task> TileSamplerZ::update(ptr<SceneManager> scene, ptr<TerrainQuad> root)
{
    ptr<Task> result = TileSampler::update(scene, root);
    unsigned int frameNumber = scene->getFrameNumber();

    if ((root->getOwner()->getLocalCamera() - oldLocalCamera).length() > 0.1 && cameraQuad != NULL && cameraQuad->t != NULL) {
        GPUTileStorage::GPUSlot *gpuTile = dynamic_cast<GPUTileStorage::GPUSlot*>(cameraQuad->t->getData(false));
        if (gpuTile != NULL && state->cameraSlot == NULL) {
            int border = get()->getBorder();
            int tileSize = get()->getCache()->getStorage()->getTileSize() - 2 * border;
            int dx = min((int)(floor(cameraQuadCoords.x * tileSize)), tileSize - 1);
            int dy = min((int)(floor(cameraQuadCoords.y * tileSize)), tileSize - 1);
            assert(border == 2);
            state->cameraSlot = gpuTile;
            state->cameraOffset = vec2i(dx + border, dy + border);
            oldLocalCamera = root->getOwner()->getLocalCamera();
        }
    }
    cameraQuad = NULL;

    if (frameNumber == state->lastFrame) {
        return result;
    }
    state->tileReadback->newFrame();
    state->lastFrame = frameNumber;

    vector<GPUTileStorage::GPUSlot*> gpuTiles;
    vector< ptr<TerrainQuad> > targets;
    vec2i camera(0, 0);

    if (state->cameraSlot != NULL) {
        gpuTiles.push_back(state->cameraSlot);
        targets.push_back(NULL);
        camera = state->cameraOffset;
        state->cameraSlot = NULL;
    }

    std::set<TreeZ*, TreeZSort>::iterator i = state->needReadback.begin();
    while (i != state->needReadback.end() && gpuTiles.size() < MAX_MIPMAP_PER_FRAME) {
        TreeZ *t = *i;
        TileCache::Tile *tile = (*i)->t;
        state->needReadback.erase(i++);

        if (tile != NULL) {
            GPUTileStorage::GPUSlot *gpuTile = dynamic_cast<GPUTileStorage::GPUSlot*>(tile->getData(false));
            if (gpuTile != NULL) {
                gpuTiles.push_back(gpuTile);
                targets.push_back(t->q);
            } else {
                t->readback = false;
            }
        }
    }

    if (gpuTiles.empty()) {
        return result;
    }

    int pass = 0;
    int count = int(gpuTiles.size());
    int parentSize = state->storage->getTileSize();
    int currentSize = (parentSize - 4) / 4 + (parentSize % 4 == 0 ? 0 : 1);
    vec2f viewportSize = vec2f(1.0f / (MAX_MIPMAP_PER_FRAME * currentSize), 1.0f / currentSize);

    if (camera != vec2i::ZERO && count == 1) {
        state->viewportU->set(vec4f(viewportSize.x, viewportSize.y, viewportSize.x, viewportSize.y));
        state->sizesU->set(vec3f(parentSize, currentSize, pass));
        state->tileU[0]->set(vec4i(gpuTiles[0]->getIndex(), gpuTiles[0]->l, camera.x, camera.y));
        state->fbo->setDrawBuffer(state->readBuffer);
        state->fbo->drawQuad(state->minmaxProg);
    } else {
        state->viewportU->set(vec4f(count * currentSize * viewportSize.x, currentSize * viewportSize.y, viewportSize.x, viewportSize.y));
        state->sizesU->set(vec3f(parentSize, currentSize, pass));
        for (int i = 0; i < count; ++i) {
            if (i == 0) {
                state->tileU[i]->set(vec4i(gpuTiles[i]->getIndex(), gpuTiles[i]->l, camera.x, camera.y));
            } else {
                state->tileU[i]->set(vec4i(gpuTiles[i]->getIndex(), gpuTiles[i]->l, 0, 0));
            }
        }
        state->fbo->setDrawBuffer(COLOR0);
        state->fbo->drawQuad(state->minmaxProg);
        while (currentSize != 1) {
            parentSize = currentSize;
            currentSize = currentSize / 4 + (currentSize % 4 == 0 ? 0 : 1);
            pass += 1;
            state->viewportU->set(vec4f(count * currentSize * viewportSize.x, currentSize * viewportSize.y, viewportSize.x, viewportSize.y));
            state->sizesU->set(vec3f(parentSize, currentSize, pass));
            state->inputU->set(state->fbo->getTextureBuffer(pass % 2 == 0 ? COLOR1 : COLOR0));
            state->fbo->setDrawBuffer(pass % 2 == 0 ? COLOR0 : COLOR1);
            state->fbo->drawQuad(state->minmaxProg);
        }
    }

    assert(state->tileReadback->canReadback());
    state->tileReadback->readback(state->fbo, 0, 0, MAX_MIPMAP_PER_FRAME, 1, RG, FLOAT, new TileCallback(targets, camera != vec2i::ZERO));

    return result;
}

bool TileSamplerZ::needTile(ptr<TerrainQuad> q)
{
    vec3d c = q->getOwner()->getLocalCamera();
    if (c.x >= q->ox && c.x < q->ox + q->l && c.y >= q->oy && c.y < q->oy + q->l) {
        return true;
    }
    return TileSampler::needTile(q);
}

void TileSamplerZ::getTiles(Tree *parent, Tree **t, ptr<TerrainQuad> q, ptr<TaskGraph> result)
{
    if (*t == NULL) {
        *t = new TreeZ(parent, q);
        (*t)->needTile = needTile(q);
        if (q->level == 0 && get()->getRootQuadSize() == 0.0f) {
            get()->setRootQuadSize((float) q->l);
        }
    }
    if ((*t)->t != NULL && (*t)->t->task->isDone() && (!((TreeZ*)(*t))->readback || ((TreeZ*) (*t))->readbackDate < (*t)->t->task->getCompletionDate())) {
        state->needReadback.insert((TreeZ*) (*t));
        ((TreeZ*) (*t))->readback = true;
        ((TreeZ*) (*t))->readbackDate = (*t)->t->task->getCompletionDate();
    }

    TileSampler::getTiles(parent, t, q, result);

    if (cameraQuad == NULL && (*t)->t != NULL && (*t)->t->task->isDone()) {
        vec3d c = q->getOwner()->getLocalCamera();
        if (c.x >= q->ox && c.x < q->ox + q->l && c.y >= q->oy && c.y < q->oy + q->l) {
            cameraQuadCoords = vec2f(float((c.x - q->ox) / q->l), float((c.y - q->oy) / q->l));
            cameraQuad = (TreeZ*) (*t);
        }
    }
}

class TileSamplerZResource : public ResourceTemplate<10, TileSamplerZ>
{
public:
    TileSamplerZResource(ptr<ResourceManager> manager, const string &name, ptr<ResourceDescriptor> desc, const TiXmlElement *e = NULL) :
        ResourceTemplate<10, TileSamplerZ>(manager, name, desc)
    {
        e = e == NULL ? desc->descriptor : e;
        checkParameters(desc, e, "id,name,sampler,producer,terrains,storeLeaf,storeParent,storeInvisible,async,");
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
    }
};

extern const char tileSamplerZ[] = "tileSamplerZ";

static ResourceFactory::Type<tileSamplerZ, TileSamplerZResource> TileSamplerZType;

}
