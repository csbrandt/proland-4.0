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

#include "proland/dem/ElevationProducer.h"

#include <sstream>

#include "ork/core/Logger.h"
#include "ork/resource/ResourceTemplate.h"
#include "ork/render/Program.h"
#include "ork/render/CPUBuffer.h"
#include "ork/taskgraph/TaskGraph.h"
#include "ork/scenegraph/SceneManager.h"
#include "ork/scenegraph/SetTargetTask.h"

#include "proland/math/noise.h"
#include "proland/producer/CPUTileStorage.h"
#include "proland/producer/GPUTileStorage.h"

using namespace std;
using namespace ork;

namespace proland
{

ptr<Texture2DArray> createDemNoise(int tileWidth)
{
    #define SETNOISE(x,y,v) n[(x) + (y) * tileWidth] = v
    const int layers[6] = {0, 1, 3, 5, 7, 15};
    float *noiseArray = new float[6 * tileWidth * tileWidth];
    long rand = 1234567;
    for (int nl = 0; nl < 6; ++nl) {
        float *n = noiseArray + nl * tileWidth * tileWidth;
        int l = layers[nl];
        // corners
        for (int j = 0; j < tileWidth; ++j) {
            for (int i = 0; i < tileWidth; ++i) {
                SETNOISE(i, j, 0.0f);
            }
        }
        long brand;
        // bottom border
        brand = (l & 1) == 0 ? 7654321 : 5647381;
        for (int h = 5; h <= tileWidth / 2; ++h) {
            float N = frandom(&brand) * 2.0f - 1.0f;
            SETNOISE(h, 2, N);
            SETNOISE(tileWidth-1-h, 2, N);
        }
        for (int v = 3; v < 5; ++v) {
            for (int h = 5; h < tileWidth - 5; ++h) {
                float N = frandom(&brand) * 2.0f - 1.0f;
                SETNOISE(h, v, N);
                SETNOISE(tileWidth-1-h, 4-v, N);
            }
        }
        // right border
        brand = (l & 2) == 0 ? 7654321 : 5647381;
        for (int v = 5; v <= tileWidth / 2; ++v) {
            float N = frandom(&brand) * 2.0f - 1.0f;
            SETNOISE(tileWidth-3, v, N);
            SETNOISE(tileWidth-3, tileWidth-1-v, N);
        }
        for (int h = tileWidth - 4; h >= tileWidth - 5; --h) {
            for (int v = 5; v < tileWidth - 5; ++v) {
                float N = frandom(&brand) * 2.0f - 1.0f;
                SETNOISE(h, v, N);
                SETNOISE(2*tileWidth-6-h, tileWidth-1-v, N);
            }
        }
        // top border
        brand = (l & 4) == 0 ? 7654321 : 5647381;
        for (int h = 5; h <= tileWidth / 2; ++h) {
            float N = frandom(&brand) * 2.0f - 1.0f;
            SETNOISE(h, tileWidth-3, N);
            SETNOISE(tileWidth-1-h, tileWidth-3, N);
        }
        for (int v = tileWidth - 2; v < tileWidth; ++v) {
            for (int h = 5; h < tileWidth - 5; ++h) {
                float N = frandom(&brand) * 2.0f - 1.0f;
                SETNOISE(h, v, N);
                SETNOISE(tileWidth-1-h, 2*tileWidth-6-v, N);
            }
        }
        // left border
        brand = (l & 8) == 0 ? 7654321 : 5647381;
        for (int v = 5; v <= tileWidth / 2; ++v) {
            float N = frandom(&brand) * 2.0f - 1.0f;
            SETNOISE(2, v, N);
            SETNOISE(2, tileWidth-1-v, N);
        }
        for (int h = 1; h >= 0; --h) {
            for (int v = 5; v < tileWidth - 5; ++v) {
                float N = frandom(&brand) * 2.0f - 1.0f;
                SETNOISE(h, v, N);
                SETNOISE(4-h, tileWidth-1-v, N);
            }
        }
        // center
        for (int v = 5; v < tileWidth - 5; ++v) {
            for (int h = 5; h < tileWidth - 5; ++h) {
                SETNOISE(h, v, frandom(&rand) * 2.0f - 1.0f);
            }
        }
    }
    ptr<Texture2DArray> noiseTexture = new Texture2DArray(tileWidth, tileWidth, 6, R16F,
        RED, FLOAT, Texture::Parameters().wrapS(REPEAT).wrapT(REPEAT).min(NEAREST).mag(NEAREST), Buffer::Parameters(), CPUBuffer(noiseArray));
    delete[] noiseArray;
    return noiseTexture;
}

static_ptr< Factory< int, ptr<Texture2DArray> > > demNoiseFactory(new Factory< int, ptr<Texture2DArray> >(createDemNoise));

ptr<FrameBuffer> createDemFramebuffer(pair< ptr<Texture2D> , ptr<Texture2D> > textures)
{
    int tileWidth = textures.first->getWidth();
    ptr<FrameBuffer> frameBuffer(new FrameBuffer());
    frameBuffer->setReadBuffer(COLOR0);
    frameBuffer->setDrawBuffer(COLOR0);
    frameBuffer->setViewport(vec4<GLint>(0, 0, tileWidth, tileWidth));
    frameBuffer->setTextureBuffer(COLOR0, textures.first, 0);
    if (textures.second != NULL) {
        ptr<RenderBuffer> depthBuffer = new RenderBuffer(RenderBuffer::DEPTH_COMPONENT32, tileWidth, tileWidth);
        frameBuffer->setTextureBuffer(COLOR1, textures.second, 0);
        frameBuffer->setRenderBuffer(DEPTH, depthBuffer);
        frameBuffer->setDepthTest(true, ALWAYS);
    } else {
        frameBuffer->setDepthTest(false);
    }
    frameBuffer->setPolygonMode(FILL, FILL);
    return frameBuffer;
}

static_ptr< Factory< pair< ptr<Texture2D> , ptr<Texture2D> >, ptr<FrameBuffer> > > demFramebufferFactory(
    new Factory< pair< ptr<Texture2D> , ptr<Texture2D> >, ptr<FrameBuffer> >(createDemFramebuffer));

static_ptr<FrameBuffer> ElevationProducer::old;

ElevationProducer::ElevationProducer(ptr<TileCache> cache, ptr<TileProducer> residualTiles,
    ptr<Texture2D> demTexture, ptr<Texture2D> layerTexture, ptr<Texture2D> residualTexture,
    ptr<Program> upsample, ptr<Program> blend, int gridMeshSize,
    vector<float> &noiseAmp, bool flipDiagonals) : TileProducer("ElevationProducer", "CreateElevationTile")
{
    init(cache, residualTiles, demTexture, layerTexture, residualTexture, upsample, blend, gridMeshSize, noiseAmp, flipDiagonals);
}

ElevationProducer::ElevationProducer() : TileProducer("ElevationProducer", "CreateElevationTile")
{
}

void ElevationProducer::init(ptr<TileCache> cache, ptr<TileProducer> residualTiles,
    ptr<Texture2D> demTexture, ptr<Texture2D> layerTexture, ptr<Texture2D> residualTexture,
    ptr<Program> upsample, ptr<Program> blend, int gridMeshSize,
    vector<float> &noiseAmp, bool flipDiagonals)
{
    int tileWidth = cache->getStorage()->getTileSize();
    TileProducer::init(cache, true);
    this->frameBuffer = demFramebufferFactory->get(make_pair(demTexture, layerTexture));
    this->residualTiles = residualTiles;
    this->demTexture = demTexture;
    this->layerTexture = layerTexture;
    this->residualTexture = residualTexture;
    this->upsample = upsample;
    this->blend = blend;
    this->noiseTexture = demNoiseFactory->get(tileWidth);
    this->noiseAmp = noiseAmp;
    this->gridMeshSize = gridMeshSize;
    this->flipDiagonals = flipDiagonals;

    this->tileWSDFU = upsample->getUniform4f("tileWSDF");
    this->coarseLevelSamplerU = upsample->getUniformSampler("coarseLevelSampler");
    this->coarseLevelOSLU = upsample->getUniform4f("coarseLevelOSL");
    this->residualSamplerU = upsample->getUniformSampler("residualSampler");
    this->residualOSHU = upsample->getUniform4f("residualOSH");
    this->noiseSamplerU = upsample->getUniformSampler("noiseSampler");
    this->noiseUVLHU = upsample->getUniform4f("noiseUVLH");

    if (this->blend != NULL) {
        this->blendCoarseLevelSamplerU = blend->getUniformSampler("coarseLevelSampler");
        this->blendScaleU = blend->getUniform1f("scale");
    }

    if (residualTiles != NULL) {
        this->residualTile = new float[tileWidth * tileWidth];
    } else {
        this->residualTile = NULL;
    }
}

ElevationProducer::~ElevationProducer()
{
    if (residualTile != NULL) {
        delete[] residualTile;
    }
}

void ElevationProducer::getReferencedProducers(vector< ptr<TileProducer> > &producers) const
{
    if (residualTiles != NULL) {
        producers.push_back(residualTiles);
    }
}

void ElevationProducer::setRootQuadSize(float size)
{
    TileProducer::setRootQuadSize(size);
    if (residualTiles != NULL) {
        residualTiles->setRootQuadSize(size);
    }
}

int ElevationProducer::getBorder()
{
    assert(residualTiles == NULL || residualTiles->getBorder() == 2);
    return 2;
}

void *ElevationProducer::getContext() const
{
    return layerTexture == NULL ? demTexture.get() : layerTexture.get();
}

ptr<Task> ElevationProducer::startCreateTile(int level, int tx, int ty, unsigned int deadline, ptr<Task> task, ptr<TaskGraph> owner)
{
    ptr<TaskGraph> result = owner == NULL ? createTaskGraph(task) : owner;

    if (level > 0) {
        TileCache::Tile *t = getTile(level - 1, tx / 2, ty / 2, deadline);
        assert(t != NULL);
        result->addTask(t->task);
        result->addDependency(task, t->task);
    }

    if (residualTiles != NULL) {
        int tileSize = getCache()->getStorage()->getTileSize() - 5;
        int residualTileSize = residualTiles->getCache()->getStorage()->getTileSize() - 5;
        int mod = residualTileSize / tileSize;
        if (residualTiles->hasTile(level, tx / mod, ty / mod)) {
            TileCache::Tile *t = residualTiles->getTile(level, tx / mod, ty / mod, deadline);
            assert(t != NULL);
            result->addTask(t->task);
            result->addDependency(task, t->task);
        }
    }
    TileProducer::startCreateTile(level, tx, ty, deadline, task, result);

    return result;
}

void ElevationProducer::beginCreateTile()
{
    old = SceneManager::getCurrentFrameBuffer();
    SceneManager::setCurrentFrameBuffer(frameBuffer);
    TileProducer::beginCreateTile();
}

bool ElevationProducer::doCreateTile(int level, int tx, int ty, TileStorage::Slot *data)
{
    if (Logger::DEBUG_LOGGER != NULL) {
        ostringstream oss;
        oss << "Elevation tile " << getId() << " " << level << " " << tx << " " << ty;
        Logger::DEBUG_LOGGER->log("DEM", oss.str());
    }

    GPUTileStorage::GPUSlot *gpuData = dynamic_cast<GPUTileStorage::GPUSlot*>(data);
    assert(gpuData != NULL);

    getCache()->getStorage().cast<GPUTileStorage>()->notifyChange(gpuData);

    int tileWidth = data->getOwner()->getTileSize();
    int tileSize = tileWidth - 5;

    GPUTileStorage::GPUSlot *parentGpuData = NULL;
    if (level > 0) {

        TileCache::Tile *t = findTile(level - 1, tx / 2, ty / 2);
        assert(t != NULL);
        parentGpuData = dynamic_cast<GPUTileStorage::GPUSlot*>(t->getData());
        assert(parentGpuData != NULL);
    }

    tileWSDFU->set(vec4f((float) tileWidth, getRootQuadSize() / (1 << level) / tileSize, float((tileWidth - 5) / gridMeshSize), flipDiagonals ? 1.0f : 0.0f));

    if (level > 0) {
        ptr<Texture2DArray> t = parentGpuData->t;
        float dx = float((tx % 2) * (tileSize / 2));
        float dy = float((ty % 2) * (tileSize / 2));
        coarseLevelSamplerU->set(t);
        coarseLevelOSLU->set(vec4f(dx / parentGpuData->getWidth(), dy / parentGpuData->getHeight(), 1.0f / parentGpuData->getWidth(), parentGpuData->l));
    } else {
        coarseLevelOSLU->set(vec4f(-1.0f, -1.0f, -1.0f, -1.0f));
    }

    int residualTileWidth = residualTiles == NULL ? 0 : residualTiles->getCache()->getStorage()->getTileSize();
    int mod = (residualTileWidth - 5) / tileSize;

    if (residualTiles != NULL && residualTiles->hasTile(level, tx / mod, ty / mod)) {
        residualSamplerU->set(residualTexture);
        residualOSHU->set(vec4f(0.25f / tileWidth, 0.25f / tileWidth, 2.0f / tileWidth, 1.0f));

        int rx = (tx % mod) * tileSize;
        int ry = (ty % mod) * tileSize;
        TileCache::Tile *t = residualTiles->findTile(level, tx / mod, ty / mod);
        assert(t != NULL);
        CPUTileStorage<float>::CPUSlot *cpuTile = dynamic_cast<CPUTileStorage<float>::CPUSlot*>(t->getData());
        assert(cpuTile != NULL);
        for (int y = 0; y < tileWidth; ++y) {
            for (int x = 0; x < tileWidth; ++x) {

                float r = cpuTile->data[(x + rx) + (y + ry) * residualTileWidth];
                assert(isFinite(r));
                residualTile[x + y * tileWidth] = r;
            }
        }

        residualTexture->setSubImage(0, 0, 0, tileWidth, tileWidth, RED, FLOAT, Buffer::Parameters(), CPUBuffer(residualTile));
    } else {
        residualSamplerU->set(residualTexture);
        residualOSHU->set(vec4f(0.0, 0.0, 1.0, 0.0));
    }

    float rs = level < int(noiseAmp.size()) ? noiseAmp[level] : 0.0f;

    int noiseL = 0;
    if (face == 1) {
        int offset = 1 << level;
        int bottomB = cnoise(tx + 0.5, ty + offset) > 0.0 ? 1 : 0;
        int rightB = (tx == offset - 1 ? cnoise(ty + offset + 0.5, offset) : cnoise(tx + 1, ty + offset + 0.5)) > 0.0 ? 2 : 0;
        int topB = (ty == offset - 1 ? cnoise((3 * offset - 1 - tx) + 0.5, offset) : cnoise(tx + 0.5, ty + offset + 1)) > 0.0 ? 4 : 0;
        int leftB = (tx == 0 ? cnoise((4 * offset - 1 - ty) + 0.5, offset) : cnoise(tx, ty + offset + 0.5)) > 0.0 ? 8 : 0;
        noiseL = bottomB + rightB + topB + leftB;
    } else if (face == 6) {
        int offset = 1 << level;
        int bottomB = (ty == 0 ? cnoise((3 * offset - 1 - tx) + 0.5, 0) : cnoise(tx + 0.5, ty - offset)) > 0.0 ? 1 : 0;
        int rightB = (tx == offset - 1 ? cnoise((2 * offset - 1 - ty) + 0.5, 0) : cnoise(tx + 1, ty - offset + 0.5)) > 0.0 ? 2 : 0;
        int topB = cnoise(tx + 0.5, ty - offset + 1) > 0.0 ? 4 : 0;
        int leftB = (tx == 0 ? cnoise(3 * offset + ty + 0.5, 0) : cnoise(tx, ty - offset + 0.5)) > 0.0 ? 8 : 0;
        noiseL = bottomB + rightB + topB + leftB;
    } else {
        int offset = (1 << level) * (face - 2);
        int bottomB = cnoise(tx + offset + 0.5, ty) > 0.0 ? 1 : 0;
        int rightB = cnoise((tx + offset + 1) % (4 << level), ty + 0.5) > 0.0 ? 2 : 0;
        int topB = cnoise(tx + offset + 0.5, ty + 1) > 0.0 ? 4 : 0;
        int leftB = cnoise(tx + offset, ty + 0.5) > 0.0 ? 8 : 0;
        noiseL = bottomB + rightB + topB + leftB;
    }
    const int noiseRs[16] = { 0, 0, 1, 0, 2, 0, 1, 0, 3, 3, 1, 3, 2, 2, 1, 0 };
    const int noiseLs[16] = { 0, 1, 1, 2, 1, 3, 2, 4, 1, 2, 3, 4, 2, 4, 4, 5 };
    int noiseR = noiseRs[noiseL];
    noiseL = noiseLs[noiseL];

    noiseSamplerU->set(noiseTexture);
    noiseUVLHU->set(vec4f(noiseR, (noiseR + 1) % 4, noiseL, rs));

    frameBuffer->setClearColor(vec4<GLfloat>(0.0f, 0.f, 0.f, 255.f));
    frameBuffer->clear(true, true, true);

    frameBuffer->drawQuad(upsample);

    if (hasLayers()) {
        frameBuffer->setDepthTest(true, LESS);
        TileProducer::doCreateTile(level, tx, ty, data);
        frameBuffer->setDepthTest(false);
        frameBuffer->setDrawBuffer(COLOR1);
        frameBuffer->setReadBuffer(COLOR1);

        blendCoarseLevelSamplerU->set(demTexture);
        if (blendScaleU != NULL) {
            blendScaleU->set(1.0f / tileWidth);
        }
        frameBuffer->drawQuad(blend);

        gpuData->copyPixels(frameBuffer, 0, 0, tileWidth, tileWidth);

        frameBuffer->setDrawBuffer(COLOR0);
        frameBuffer->setReadBuffer(COLOR0);
    } else {
        gpuData->copyPixels(frameBuffer, 0, 0, tileWidth, tileWidth);
    }

    return true;
}

void ElevationProducer::endCreateTile()
{
    TileProducer::endCreateTile();
    SceneManager::setCurrentFrameBuffer(old);
    old = NULL;
}

void ElevationProducer::stopCreateTile(int level, int tx, int ty)
{
    if (level > 0) {
        TileCache::Tile *t = findTile(level - 1, tx / 2, ty / 2);
        assert(t != NULL);
        putTile(t);
    }

    if (residualTiles != NULL) {
        int tileSize = getCache()->getStorage()->getTileSize() - 5;
        int residualTileSize = residualTiles->getCache()->getStorage()->getTileSize() - 5;
        int mod = residualTileSize / tileSize;
        if (residualTiles->hasTile(level, tx / mod, ty / mod)) {
            TileCache::Tile *t = residualTiles->findTile(level, tx / mod, ty / mod);
            assert(t != NULL);
            residualTiles->putTile(t);
        }
    }

    TileProducer::stopCreateTile(level, tx, ty);
}

void ElevationProducer::swap(ptr<ElevationProducer> p)
{
    TileProducer::swap(p);
    std::swap(frameBuffer, p->frameBuffer);
    std::swap(upsample, p->upsample);
    std::swap(blend, p->blend);
    std::swap(residualTiles, p->residualTiles);
    std::swap(demTexture, p->demTexture);
    std::swap(residualTexture, p->residualTexture);
    std::swap(layerTexture, p->layerTexture);
    std::swap(noiseAmp, p->noiseAmp);
    std::swap(residualTile, p->residualTile);
    std::swap(gridMeshSize, p->gridMeshSize);
    std::swap(flipDiagonals, p->flipDiagonals);
    std::swap(old, p->old);
    std::swap(noiseTexture, p->noiseTexture);
    std::swap(tileWSDFU, p->tileWSDFU);
    std::swap(coarseLevelSamplerU, p->coarseLevelSamplerU);
    std::swap(coarseLevelOSLU, p->coarseLevelOSLU);
    std::swap(residualSamplerU, p->residualSamplerU);
    std::swap(residualOSHU, p->residualOSHU);
    std::swap(noiseSamplerU, p->noiseSamplerU);
    std::swap(noiseUVLHU, p->noiseUVLHU);
    std::swap(elevationSamplerU, p->elevationSamplerU);
    std::swap(blendCoarseLevelSamplerU, p->blendCoarseLevelSamplerU);
    std::swap(blendScaleU, p->blendScaleU);
}

void ElevationProducer::init(ptr<ResourceManager> manager, Resource *r, const string &name, ptr<ResourceDescriptor> desc, const TiXmlElement *e)
{
    ptr<TileCache> cache;
    ptr<TileProducer> residuals;
    ptr<Program> upsampleProg;
    ptr<Program> blendProg;
    ptr<Texture2D> demTexture;
    ptr<Texture2D> layerTexture;
    ptr<Texture2D> residualTexture;
    int gridSize = 24;
    vector<float> noiseAmp;
    bool flip = false;
    cache = manager->loadResource(r->getParameter(desc, e, "cache")).cast<TileCache>();
    if (e->Attribute("residuals") != NULL) {
        residuals = manager->loadResource(r->getParameter(desc, e, "residuals")).cast<TileProducer>();
    }
    string upsample = "upsampleShader;";
    if (e->Attribute("upsampleProg") != NULL) {
        upsample = r->getParameter(desc, e, "upsampleProg");
    }
    upsampleProg = manager->loadResource(upsample).cast<Program>();
    if (e->Attribute("gridSize") != NULL) {
        r->getIntParameter(desc, e, "gridSize", &gridSize);
    }
    if (e->Attribute("noise") != NULL) {
        string noiseAmps = string(e->Attribute("noise")) + ",";
        string::size_type start = 0;
        string::size_type index;
        while ((index = noiseAmps.find(',', start)) != string::npos) {
            float value;
            string amp = noiseAmps.substr(start, index - start);
            sscanf(amp.c_str(), "%f", &value);
            noiseAmp.push_back(value);
            start = index + 1;
        }
    }
    if (e->Attribute("flip") != NULL && strcmp(e->Attribute("flip"), "true") == 0) {
        flip = true;
    }
    if (e->Attribute("face") != NULL) {
        r->getIntParameter(desc, e, "face", &face);
    } else if (name.at(name.size() - 1) >= '1' && name.at(name.size() - 1) <= '6') {
        face = name.at(name.size() - 1) - '0';
    } else {
        face = 0;
    }

    int tileWidth = cache->getStorage()->getTileSize();

    ostringstream demTex;

    demTex << "renderbuffer-" << tileWidth << "-RGBA32F";
    demTexture = manager->loadResource(demTex.str()).cast<Texture2D>();

    ostringstream residualTex;
    residualTex << "renderbuffer-" << tileWidth << "-R32F";
    residualTexture = manager->loadResource(residualTex.str()).cast<Texture2D>();

    const TiXmlNode *n = e->FirstChild();
    while (n != NULL) {
        const TiXmlElement *f = n->ToElement();
        if (f == NULL) {
            n = n->NextSibling();
            continue;
        }

        ptr<TileLayer> l = manager->loadResource(desc, f).cast<TileLayer>();
        if (l != NULL) {
            addLayer(l);
        } else {
            if (Logger::WARNING_LOGGER != NULL) {
                r->log(Logger::WARNING_LOGGER, desc, f, "Unknown scene node element '" + f->ValueStr() + "'");
            }
        }
        n = n->NextSibling();
    }

    if (hasLayers()) {
        demTex << "-1";
        layerTexture = manager->loadResource(demTex.str()).cast<Texture2D>();

        string blend = "blendShader;";
        if (e->Attribute("blendProg") != NULL) {
            blend = r->getParameter(desc, e, "blendProg");
        }
        blendProg = manager->loadResource(blend).cast<Program>();
    }

    init(cache, residuals, demTexture, layerTexture, residualTexture, upsampleProg, blendProg, gridSize, noiseAmp, flip);
}

class ElevationProducerResource : public ResourceTemplate<40, ElevationProducer>
{
public:
    ElevationProducerResource(ptr<ResourceManager> manager, const string &name, ptr<ResourceDescriptor> desc, const TiXmlElement *e = NULL) :
        ResourceTemplate<40, ElevationProducer>(manager, name, desc)
    {
        e = e == NULL ? desc->descriptor : e;
        checkParameters(desc, e, "name,cache,residuals,face,upsampleProg,blendProg,gridSize,noise,flip,");
        init(manager, this, name, desc, e);
    }

    virtual bool prepareUpdate()
    {
        if (dynamic_cast<Resource*>(upsample.get())->changed()) {
            invalidateTiles();
        } else if (blend != NULL && dynamic_cast<Resource*>(blend.get())->changed()) {
            invalidateTiles();
        }
        return ResourceTemplate<40, ElevationProducer>::prepareUpdate();
    }
};

extern const char elevationProducer[] = "elevationProducer";

static ResourceFactory::Type<elevationProducer, ElevationProducerResource> ElevationProducerType;

}
