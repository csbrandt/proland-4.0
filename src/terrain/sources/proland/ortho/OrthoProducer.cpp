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

#include "proland/ortho/OrthoProducer.h"

#include <sstream>

#include "ork/core/Logger.h"
#include "ork/resource/ResourceTemplate.h"
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

ptr<Texture2DArray> createOrthoNoise(int tileWidth)
{
    const int layers[6] = {0, 1, 3, 5, 7, 15};
    unsigned char *noiseArray = new unsigned char[6 * tileWidth * tileWidth * 4];
    long rand = 1234567;
    for (int nl = 0; nl < 6; ++nl) {
        unsigned char *n = noiseArray + nl * tileWidth * tileWidth * 4;
        int l = layers[nl];
        // corners
        for (int v = 0; v < tileWidth; ++v) {
            for (int h = 0; h < tileWidth; ++h) {
               for (int c = 0; c < 4; ++c) {
                   n[4*(h+v*tileWidth)+c] = 128;
               }
            }
        }
        long brand;
        // bottom border
        brand = (l & 1) == 0 ? 7654321 : 5647381;
        for (int v = 2; v < 4; ++v) {
            for (int h = 4; h < tileWidth - 4; ++h) {
               for (int c = 0; c < 4; ++c) {
                    int N = int(frandom(&brand) * 255.0f);
                    n[4*(h+v*tileWidth)+c] = N;
                    n[4*(tileWidth-1-h+(3-v)*tileWidth)+c] = N;
               }
            }
        }
        // right border
        brand = (l & 2) == 0 ? 7654321 : 5647381;
        for (int h = tileWidth - 3; h >= tileWidth - 4; --h) {
            for (int v = 4; v < tileWidth - 4; ++v) {
               for (int c = 0; c < 4; ++c) {
                    int N = int(frandom(&brand) * 255.0f);
                    n[4*(h+v*tileWidth)+c] = N;
                    n[4*(2*tileWidth-5-h+(tileWidth-1-v)*tileWidth)+c] = N;
               }
            }
        }
        // top border
        brand = (l & 4) == 0 ? 7654321 : 5647381;
        for (int v = tileWidth - 2; v < tileWidth; ++v) {
            for (int h = 4; h < tileWidth - 4; ++h) {
               for (int c = 0; c < 4; ++c) {
                    int N = int(frandom(&brand) * 255.0f);
                    n[4*(h+v*tileWidth)+c] = N;
                    n[4*(tileWidth-1-h+(2*tileWidth-5-v)*tileWidth)+c] = N;
               }
            }
        }
        // left border
        brand = (l & 8) == 0 ? 7654321 : 5647381;
        for (int h = 1; h >= 0; --h) {
            for (int v = 4; v < tileWidth - 4; ++v) {
               for (int c = 0; c < 4; ++c) {
                    int N = int(frandom(&brand) * 255.0f);
                    n[4*(h+v*tileWidth)+c] = N;
                    n[4*(3-h+(tileWidth-1-v)*tileWidth)+c] = N;
               }
            }
        }
        // center
        for (int v = 4; v < tileWidth - 4; ++v) {
            for (int h = 4; h < tileWidth - 4; ++h) {
                for (int c = 0; c < 4; ++c) {
                    n[4*(h+v*tileWidth)+c] = int(frandom(&rand) * 255.0f);
                }
            }
        }
    }
    ptr<Texture2DArray> noiseTexture = new Texture2DArray(tileWidth, tileWidth, 6, RGBA8,
        RGBA, UNSIGNED_BYTE, Texture::Parameters().wrapS(REPEAT).wrapT(REPEAT).min(NEAREST).mag(NEAREST), Buffer::Parameters(), CPUBuffer(noiseArray));
    delete[] noiseArray;
    return noiseTexture;
}

static_ptr< Factory< int, ptr<Texture2DArray> > > orthoNoiseFactory(new Factory< int, ptr<Texture2DArray> >(createOrthoNoise));

ptr<FrameBuffer> createOrthoFramebuffer(ptr<Texture2D> orthoTexture)
{
    int tileWidth = orthoTexture->getWidth();
    ptr<FrameBuffer> frameBuffer(new FrameBuffer());
    frameBuffer->setReadBuffer(COLOR0);
    frameBuffer->setDrawBuffer(COLOR0);
    frameBuffer->setViewport(vec4<GLint>(0, 0, tileWidth, tileWidth));
    frameBuffer->setTextureBuffer(COLOR0, orthoTexture, 0);
    frameBuffer->setPolygonMode(FILL, FILL);
    return frameBuffer;
}

static_ptr< Factory< ptr<Texture2D>, ptr<FrameBuffer> > > orthoFramebufferFactory(
    new Factory< ptr<Texture2D>, ptr<FrameBuffer> >(createOrthoFramebuffer));

static_ptr<FrameBuffer> OrthoProducer::old;

OrthoProducer::OrthoProducer(ptr<TileCache> cache, ptr<TileProducer> residualTiles,
    ptr<Texture2D> orthoTexture, ptr<Texture2D> residualTexture,
    ptr<Program> upsample, vec4f rootNoiseColor, vec4f noiseColor,
    vector<float> &noiseAmp, bool noiseHsv,
    float scale, int maxLevel) : TileProducer("OrthoProducer", "CreateOrthoTile")
{
    init(cache, residualTiles, orthoTexture, residualTexture, upsample, rootNoiseColor, noiseColor, noiseAmp, noiseHsv, scale, maxLevel);
}

OrthoProducer::OrthoProducer() : TileProducer("OrthoProducer", "CreateOrthoTile")
{
}

void OrthoProducer::init(ptr<TileCache> cache, ptr<TileProducer> residualTiles,
    ptr<Texture2D> orthoTexture, ptr<Texture2D> residualTexture,
    ptr<Program> upsample, vec4f rootNoiseColor, vec4f noiseColor,
    vector<float> &noiseAmp, bool noiseHsv,
    float scale, int maxLevel)
{
    int tileWidth = cache->getStorage()->getTileSize();
    TileProducer::init(cache, true);
    this->frameBuffer = orthoFramebufferFactory->get(orthoTexture);
    this->residualTiles = residualTiles;
    this->orthoTexture = orthoTexture;
    this->residualTexture = residualTexture;
    this->upsample = upsample;
    this->noiseTexture = orthoNoiseFactory->get(tileWidth);
    this->rootNoiseColor = rootNoiseColor;
    this->noiseColor = noiseColor;
    this->noiseAmp = noiseAmp;
    this->noiseHsv = noiseHsv;
    this->scale = scale;
    this->maxLevel = maxLevel;

    tileWidthU = upsample->getUniform1f("tileWidth");
    coarseLevelSamplerU = upsample->getUniformSampler("coarseLevelSampler");
    coarseLevelOSLU = upsample->getUniform4f("coarseLevelOSL");
    residualSamplerU = upsample->getUniformSampler("residualSampler");
    residualOSHU = upsample->getUniform4f("residualOSH");
    noiseSamplerU = upsample->getUniformSampler("noiseSampler");
    noiseUVLHU = upsample->getUniform4i("noiseUVLH");
    noiseColorU = upsample->getUniform4f("noiseColor");
    rootNoiseColorU = upsample->getUniform4f("rootNoiseColor");

    if (residualTiles != NULL) {
        ptr<TileStorage> s = residualTiles->getCache()->getStorage();
        channels = s.cast< CPUTileStorage<unsigned char> >()->getChannels();
        assert(tileWidth == s->getTileSize());
        assert((int) cache->getStorage().cast<GPUTileStorage>()->getTexture(0)->getComponents() >= channels);
    } else {
        channels = (int) cache->getStorage().cast<GPUTileStorage>()->getTexture(0)->getComponents();
    }
}

OrthoProducer::~OrthoProducer()
{
}

void OrthoProducer::getReferencedProducers(vector< ptr<TileProducer> > &producers) const
{
    if (residualTiles != NULL) {
        producers.push_back(residualTiles);
    }
}

void OrthoProducer::setRootQuadSize(float size)
{
    TileProducer::setRootQuadSize(size);
    if (residualTiles != NULL) {
        residualTiles->setRootQuadSize(size);
    }
}

int OrthoProducer::getBorder()
{
    assert(residualTiles == NULL || residualTiles->getBorder() == 2);
    return 2;
}

bool OrthoProducer::hasTile(int level, int tx, int ty)
{
    return maxLevel == -1 || level <= maxLevel;
}

void *OrthoProducer::getContext() const
{
    return orthoTexture.get();
}

bool OrthoProducer::prefetchTile(int level, int tx, int ty)
{
    bool b = TileProducer::prefetchTile(level, tx, ty);
    if (!b) {
        if (residualTiles != NULL && residualTiles->hasTile(level, tx, ty)) {
            residualTiles->prefetchTile(level, tx, ty);
        }
    }
    return b;
}

ptr<Task> OrthoProducer::startCreateTile(int level, int tx, int ty, unsigned int deadline, ptr<Task> task, ptr<TaskGraph> owner)
{
    ptr<TaskGraph> result = owner == NULL ? createTaskGraph(task) : owner;

    if (level > 0) {
        TileCache::Tile *t = getTile(level - 1, tx / 2, ty / 2, deadline);
        assert(t != NULL);
        result->addTask(t->task);
        result->addDependency(task, t->task);
    }

    if (residualTiles != NULL && residualTiles->hasTile(level, tx, ty)) {
        TileCache::Tile *t = residualTiles->getTile(level, tx, ty, deadline);
        assert(t != NULL);
        result->addTask(t->task);
        result->addDependency(task, t->task);
    }
    TileProducer::startCreateTile(level, tx, ty, deadline, task, result);

    return result;
}

void OrthoProducer::beginCreateTile()
{
    old = SceneManager::getCurrentFrameBuffer();
    SceneManager::setCurrentFrameBuffer(frameBuffer);
    TileProducer::beginCreateTile();
}

bool OrthoProducer::doCreateTile(int level, int tx, int ty, TileStorage::Slot *data)
{
    if (Logger::DEBUG_LOGGER != NULL) {
        ostringstream oss;
        oss << "Ortho tile " << getId() << " " << level << " " << tx << " " << ty;
        Logger::DEBUG_LOGGER->log("ORTHO", oss.str());
    }

    GPUTileStorage::GPUSlot *gpuData = dynamic_cast<GPUTileStorage::GPUSlot*>(data);
    assert(gpuData != NULL);
    getCache()->getStorage().cast<GPUTileStorage>()->notifyChange(gpuData);

    int tileWidth = data->getOwner()->getTileSize();
    int tileSize = tileWidth - 4;

    GPUTileStorage::GPUSlot *parentGpuData = NULL;
    if (level > 0) {
        TileCache::Tile *t = findTile(level - 1, tx / 2, ty / 2);
        assert(t != NULL);
        parentGpuData = dynamic_cast<GPUTileStorage::GPUSlot*>(t->getData());
        assert(parentGpuData != NULL);
    }

    tileWidthU->set((float) tileWidth);

    if (level > 0) {
        ptr<Texture> t = parentGpuData->t;
        float dx = float((tx % 2) * (tileSize / 2));
        float dy = float((ty % 2) * (tileSize / 2));
        coarseLevelSamplerU->set(t);
        coarseLevelOSLU->set(vec4f((dx + 0.5f) / parentGpuData->getWidth(), (dy + 0.5f) / parentGpuData->getHeight(), 1.0f / parentGpuData->getWidth(), parentGpuData->l));
    } else {
        coarseLevelOSLU->set(vec4f(-1.0f, -1.0f, -1.0f, -1.0f));
    }

    if (residualTiles != NULL && residualTiles->hasTile(level, tx, ty)) {
        residualSamplerU->set(residualTexture);
        residualOSHU->set(vec4f(0.5f / tileWidth, 0.5f / tileWidth, 1.0f / tileWidth, scale));

        TileCache::Tile *t = residualTiles->findTile(level, tx, ty);
        assert(t != NULL);
        CPUTileStorage<unsigned char>::CPUSlot *cpuTile = dynamic_cast<CPUTileStorage<unsigned char>::CPUSlot*>(t->getData());
        assert(cpuTile != NULL);

        TextureFormat f;
        switch (channels) {
        case 1:
            f = RED;
            break;
        case 2:
            f = RG;
            break;
        case 3:
            f = RGB;
            break;
        default:
            f = RGBA;
            break;
        }
        residualTexture->setSubImage(0, 0, 0, tileWidth, tileWidth, f, UNSIGNED_BYTE, Buffer::Parameters(), CPUBuffer(cpuTile->data));
    } else {
        residualOSHU->set(vec4f(-1.0f, -1.0f, -1.0f, -1.0f));
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
    noiseUVLHU->set(vec4i(noiseR, (noiseR + 1) % 4, noiseL, noiseHsv ? 1 : 0));
    if (noiseHsv) {
        noiseColorU->set(vec4f(noiseColor) * vec4f(rs, rs, rs, scale * rs) / 255.0f);
    } else {
        noiseColorU->set(noiseColor * scale * rs / 255.0f);
    }
    if (rootNoiseColorU != NULL) {
        rootNoiseColorU->set(rootNoiseColor);
    }

    frameBuffer->drawQuad(upsample);
    if (hasLayers()) {
        TileProducer::doCreateTile(level, tx, ty, data);
    }
    gpuData->copyPixels(frameBuffer, 0, 0, tileWidth, tileWidth);

    return true;
}

void OrthoProducer::endCreateTile()
{
    TileProducer::endCreateTile();
    SceneManager::setCurrentFrameBuffer(old);
    old = NULL;
}

void OrthoProducer::stopCreateTile(int level, int tx, int ty)
{
    if (level > 0) {
        TileCache::Tile *t = findTile(level - 1, tx / 2, ty / 2);
        assert(t != NULL);
        putTile(t);
    }

    if (residualTiles != NULL && residualTiles->hasTile(level, tx, ty)) {
        TileCache::Tile *t = residualTiles->findTile(level, tx, ty);
        assert(t != NULL);
        residualTiles->putTile(t);
    }

    TileProducer::stopCreateTile(level, tx, ty);
}

void OrthoProducer::swap(ptr<OrthoProducer> p)
{
    TileProducer::swap(p);
    std::swap(frameBuffer, p->frameBuffer);
    std::swap(residualTiles, p->residualTiles);
    std::swap(orthoTexture, p->orthoTexture);
    std::swap(residualTexture, p->residualTexture);
    std::swap(old, p->old);
    std::swap(upsample, p->upsample);
    std::swap(channels, p->channels);
    std::swap(maxLevel, p->maxLevel);
    std::swap(noiseTexture, p->noiseTexture);
    std::swap(rootNoiseColor, p->rootNoiseColor);
    std::swap(noiseColor, p->noiseColor);
    std::swap(noiseAmp, p->noiseAmp);
    std::swap(noiseHsv, p->noiseHsv);
    std::swap(scale, p->scale);
    std::swap(tileWidthU, p->tileWidthU);
    std::swap(coarseLevelSamplerU, p->coarseLevelSamplerU);
    std::swap(coarseLevelOSLU, p->coarseLevelOSLU);
    std::swap(residualSamplerU, p->residualSamplerU);
    std::swap(residualOSHU, p->residualOSHU);
    std::swap(noiseSamplerU, p->noiseSamplerU);
    std::swap(noiseUVLHU, p->noiseUVLHU);
    std::swap(noiseColorU, p->noiseColorU);
    std::swap(rootNoiseColorU, p->rootNoiseColorU);
}

void OrthoProducer::init(ptr<ResourceManager> manager, Resource *r, const string &name, ptr<ResourceDescriptor> desc, const TiXmlElement *e)
{
    ptr<TileCache> cache;
    ptr<TileProducer> residuals;
    ptr<Program> upsampleProg;
    ptr<Texture2D> orthoTexture;
    ptr<Texture2D> residualTexture;
    vec4f rootNoiseColor = vec4f(0.5, 0.5, 0.5, 0.5);
    vec4f noiseColor = vec4f(1.0, 1.0, 1.0, 1.0);
    vector<float> noiseAmp;
    bool noiseHsv = false;
    float scale = 2.0f;
    int maxLevel = -1;
    cache = manager->loadResource(r->getParameter(desc, e, "cache")).cast<TileCache>();
    if (e->Attribute("residuals") != NULL) {
        residuals = manager->loadResource(r->getParameter(desc, e, "residuals")).cast<TileProducer>();
    }
    string upsample = "upsampleOrthoShader;";
    if (e->Attribute("upsampleProg") != NULL) {
        upsample = r->getParameter(desc, e, "upsampleProg");
    }
    upsampleProg = manager->loadResource(upsample).cast<Program>();
    if (e->Attribute("rnoise") != NULL) {
        string c = string(e->Attribute("rnoise")) + ",";
        string::size_type start = 0;
        string::size_type index;
        for (int i = 0; i < 4; i++) {
            index = c.find(',', start);
            rootNoiseColor[i] = (float) atof(c.substr(start, index - start).c_str()) / 255;
            start = index + 1;
        }
    }
    if (e->Attribute("cnoise") != NULL) {
        string c = string(e->Attribute("cnoise")) + ",";
        string::size_type start = 0;
        string::size_type index;
        for (int i = 0; i < 4; i++) {
            index = c.find(',', start);
            noiseColor[i] = (float) atof(c.substr(start, index - start).c_str()) / 255;
            start = index + 1;
        }
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
    if (e->Attribute("hsv") != NULL && strcmp(e->Attribute("hsv"), "true") == 0) {
        noiseHsv = true;
    }
    if (e->Attribute("scale") != NULL) {
        r->getFloatParameter(desc, e, "scale", &scale);
    }
    if (e->Attribute("maxLevel") != NULL) {
        r->getIntParameter(desc, e, "maxLevel", &maxLevel);
    }
    if (e->Attribute("face") != NULL) {
        r->getIntParameter(desc, e, "face", &face);
    } else if (name.at(name.size() - 1) >= '1' && name.at(name.size() - 1) <= '6') {
        face = name.at(name.size() - 1) - '0';
    } else {
        face = 1;
    }

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

    int tileWidth = cache->getStorage()->getTileSize();
    int channels = hasLayers() ? 4 : cache->getStorage().cast<GPUTileStorage>()->getTexture(0)->getComponents();

    ostringstream orthoTex;
    orthoTex << "renderbuffer-" << tileWidth;
    switch (channels) {
    case 1:
        orthoTex << "-R8";
        break;
    case 2:
        orthoTex << "-RG8";
        break;
    case 3:
        orthoTex << "-RGB8";
        break;
    default:
        orthoTex << "-RGBA8";
        break;
    }
    orthoTexture = manager->loadResource(orthoTex.str()).cast<Texture2D>();

    ostringstream residualTex;
    residualTex << "renderbuffer-" << tileWidth;
    switch (cache->getStorage().cast<GPUTileStorage>()->getTexture(0)->getComponents()) {
    case 1:
        residualTex << "-R8";
        break;
    case 2:
        residualTex << "-RG8";
        break;
    case 3:
        residualTex << "-RGB8";
        break;
    default:
        residualTex << "-RGBA8";
        break;
    }
    residualTex << "-1";
    residualTexture = manager->loadResource(residualTex.str()).cast<Texture2D>();

    init(cache, residuals, orthoTexture, residualTexture, upsampleProg, rootNoiseColor, noiseColor, noiseAmp, noiseHsv, scale, maxLevel);
}

class OrthoProducerResource : public ResourceTemplate<40, OrthoProducer>
{
public:
    OrthoProducerResource(ptr<ResourceManager> manager, const string &name, ptr<ResourceDescriptor> desc, const TiXmlElement *e = NULL) :
        ResourceTemplate<40, OrthoProducer>(manager, name, desc)
    {
        e = e == NULL ? desc->descriptor : e;
        checkParameters(desc, e, "name,cache,residuals,face,upsampleProg,rnoise,cnoise,noise,hsv,scale,maxLevel,");
        init(manager, this, name, desc, e);
    }

    virtual bool prepareUpdate()
    {
        if (dynamic_cast<Resource*>(upsample.get())->changed()) {
            invalidateTiles();
        }
        return ResourceTemplate<40, OrthoProducer>::prepareUpdate();
    }
};

extern const char orthoProducer[] = "orthoProducer";

static ResourceFactory::Type<orthoProducer, OrthoProducerResource> OrthoProducerType;

}
