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

#include "proland/ortho/OrthoGPUProducer.h"

#include <sstream>

#include "ork/core/Logger.h"
#include "ork/resource/ResourceTemplate.h"
#include "ork/render/Program.h"
#include "ork/render/CPUBuffer.h"
#include "ork/taskgraph/TaskGraph.h"
#include "ork/scenegraph/SceneManager.h"
#include "ork/scenegraph/SetTargetTask.h"

#include "proland/producer/CPUTileStorage.h"
#include "proland/producer/GPUTileStorage.h"
#include "proland/ortho/OrthoCPUProducer.h"

using namespace std;
using namespace ork;

namespace proland
{

const char* uncompressShader = "\
#ifdef _VERTEX_\n\
layout (location = 0) in vec4 vertex;\n\
out vec2 uv;\n\
void main() {\n\
    gl_Position = vertex;\n\
    uv = vertex.xy * 0.5 + 0.5;\n\
}\n\
#endif\n\
#ifdef _FRAGMENT_\n\
layout (location = 0) out vec4 data;\n\
in vec2 uv;\n\
uniform sampler2D source;\n\
void main() {\n\
    data = textureLod(source, uv, 0.0);\n\
}\n\
#endif\n";

const char* upsampleShader = "\
#ifdef _VERTEX_\n\
uniform vec4 tile;\n\
layout (location = 0) in vec4 vertex;\n\
out vec3 uvl;\n\
void main() {\n\
    gl_Position = vertex;\n\
    uvl = vec3(tile.xy + (vertex.xy * 0.5 + 0.5) * tile.w, tile.z);\n\
}\n\
#endif\n\
#ifdef _FRAGMENT_\n\
uniform sampler2DArray source;\n\
layout (location = 0) out vec4 data;\n\
in vec3 uvl;\n\
void main() {\n\
    data = texture(source, uvl);\n\
}\n\
#endif\n";

static_ptr<Program> OrthoGPUProducer::uncompress;

static_ptr<Program> OrthoGPUProducer::upsample;

ptr<FrameBuffer> createOrthoGPUFramebuffer(ptr<Texture2D> uncompressedTexture)
{
    if (uncompressedTexture == NULL) {
        return NULL;
    }
    int tileSize = uncompressedTexture->getWidth();
    ptr<FrameBuffer> frameBuffer(new FrameBuffer());
    frameBuffer->setReadBuffer(COLOR0);
    frameBuffer->setDrawBuffer(COLOR0);
    frameBuffer->setViewport(vec4<GLint>(0, 0, tileSize, tileSize));
    frameBuffer->setTextureBuffer(COLOR0, uncompressedTexture, 0);
    frameBuffer->setPolygonMode(FILL, FILL);
    frameBuffer->setDepthTest(false);
    frameBuffer->setBlend(false);
    frameBuffer->setColorMask(true, true, true, true);
    frameBuffer->setDepthMask(true);
    return frameBuffer;
}

static_ptr< Factory< ptr<Texture2D>, ptr<FrameBuffer> > > orthoGPUFramebufferFactory(
    new Factory< ptr<Texture2D>, ptr<FrameBuffer> >(createOrthoGPUFramebuffer));

static_ptr<FrameBuffer> OrthoGPUProducer::old;

OrthoGPUProducer::OrthoGPUProducer(ptr<TileCache> cache, ptr<TileCache> backgroundCache, ptr<TileProducer> orthoTiles,
        int maxLevel, ptr<Texture2D> compressedTexture, ptr<Texture2D> uncompressedTexture) :
    TileProducer("OrthoGPUProducer", "CreateOrthoGPUTile")
{
    init(cache, backgroundCache, orthoTiles, maxLevel, compressedTexture, uncompressedTexture);
}

OrthoGPUProducer::OrthoGPUProducer() :
    TileProducer("OrthoGPUProducer", "CreateOrthoGPUTile")
{
}

void OrthoGPUProducer::init(ptr<TileCache> cache, ptr<TileCache> backgroundCache, ptr<TileProducer> orthoTiles, int maxLevel, ptr<Texture2D> compressedTexture, ptr<Texture2D> uncompressedTexture)
{
    TileProducer::init(cache, true);
    this->orthoTiles = orthoTiles;
    this->frameBuffer = orthoGPUFramebufferFactory->get(uncompressedTexture);
    this->maxLevel = maxLevel;
    this->compressedTexture = compressedTexture;
    this->uncompressedTexture = uncompressedTexture;
    tileSize = cache->getStorage()->getTileSize();
    channels = 0;
    cache->getStorage().cast<GPUTileStorage>()->getTexture(0)->getComponents();
    if (orthoTiles != NULL) {
        ptr<TileStorage> s = orthoTiles->getCache()->getStorage();
        assert(tileSize == s->getTileSize());
        channels = s.cast< CPUTileStorage<unsigned char> >()->getChannels();
    }

    if (orthoTiles != NULL && orthoTiles.cast<OrthoCPUProducer>()->isCompressed()) {
        assert(compressedTexture != NULL);
        assert(compressedTexture->getWidth() == tileSize);
        assert(compressedTexture->getHeight() == tileSize);
        assert(uncompressedTexture != NULL);
        assert(uncompressedTexture->getWidth() == tileSize);
        assert(uncompressedTexture->getHeight() == tileSize);
        if (uncompress == NULL) {
            uncompress = new Program(new Module(330, uncompressShader));
        }
        uncompressSourceU = uncompress->getUniformSampler("source");
    }

    if (backgroundCache != NULL) {
        coarseGpuTiles = new OrthoGPUProducer(backgroundCache, NULL, orthoTiles, -1, compressedTexture, uncompressedTexture);
    }
}

OrthoGPUProducer::~OrthoGPUProducer()
{
}

void OrthoGPUProducer::getReferencedProducers(vector< ptr<TileProducer> > &producers) const
{
    if (coarseGpuTiles != NULL) {
        producers.push_back(coarseGpuTiles);
    }
    if (orthoTiles != NULL) {
        producers.push_back(orthoTiles);
    }
}

void OrthoGPUProducer::setRootQuadSize(float size)
{
    TileProducer::setRootQuadSize(size);
    if (orthoTiles != NULL) {
        orthoTiles->setRootQuadSize(size);
    }
}

int OrthoGPUProducer::getBorder()
{
    return orthoTiles == NULL ? 2 : orthoTiles->getBorder();
}

bool OrthoGPUProducer::hasTile(int level, int tx, int ty)
{
    if (hasLayers()) {
        return maxLevel == -1 || level <= maxLevel;
    } else {
        return orthoTiles == NULL ? (maxLevel == -1 || level <= maxLevel) : orthoTiles->hasTile(level, tx, ty);
    }
}

bool OrthoGPUProducer::prefetchTile(int level, int tx, int ty)
{
    bool b = TileProducer::prefetchTile(level, tx, ty);
    if (!b) {
        if (orthoTiles != NULL) {
            if (hasLayers() && !orthoTiles->hasTile(level, tx, ty)) {
                int l = level;
                int x = tx;
                int y = ty;
                while (!coarseGpuTiles->hasTile(l, x, y)) {
                    l -= 1;
                    x /= 2;
                    y /= 2;
                }
                coarseGpuTiles->prefetchTile(l, x, y);
            } else {
                orthoTiles->prefetchTile(level, tx, ty);
            }
        }
    }
    return b;
}

void *OrthoGPUProducer::getContext() const
{
    return uncompressedTexture.get();
}

ptr<Task> OrthoGPUProducer::startCreateTile(int level, int tx, int ty, unsigned int deadline, ptr<Task> task, ptr<TaskGraph> owner)
{
    ptr<TaskGraph> result = owner == NULL ? createTaskGraph(task) : owner;
    if (orthoTiles != NULL) {
        TileCache::Tile *t;
        if (hasLayers() && !orthoTiles->hasTile(level, tx, ty)) {
            int l = level;
            int x = tx;
            int y = ty;
            while (!coarseGpuTiles->hasTile(l, x, y)) {
                l -= 1;
                x /= 2;
                y /= 2;
            }
            t = coarseGpuTiles->getTile(l, x, y, deadline);

            if (upsample == NULL) {
                upsample = new Program(new Module(330, upsampleShader));
                upsampleSourceU = upsample->getUniformSampler("source");
                tileU = upsample->getUniform4f("tile");
            }
        } else {
            t = orthoTiles->getTile(level, tx, ty, deadline);
        }
        assert(t != NULL);
        result->addTask(t->task);
        result->addDependency(task, t->task);
    }

    // calls each layer so that it can complete the task graph
    // with the necessary sub tasks
    TileProducer::startCreateTile(level, tx, ty, deadline, task, result);

    return result;
}

void OrthoGPUProducer::beginCreateTile()
{
    old = SceneManager::getCurrentFrameBuffer();
    SceneManager::setCurrentFrameBuffer(frameBuffer);
    TileProducer::beginCreateTile();
}

bool OrthoGPUProducer::doCreateTile(int level, int tx, int ty, TileStorage::Slot *data)
{
    if (Logger::DEBUG_LOGGER != NULL) {
        ostringstream oss;
        oss << "GPU tile " << getId() << " " << level << " " << tx << " " << ty;
        Logger::DEBUG_LOGGER->log("ORTHO", oss.str());
    }

    GPUTileStorage::GPUSlot *gpuData = dynamic_cast<GPUTileStorage::GPUSlot*> (data);
    assert(gpuData != NULL);
    getCache()->getStorage().cast<GPUTileStorage> ()->notifyChange(gpuData);

    CPUTileStorage<unsigned char>::CPUSlot *cpuData = NULL;

    TileCache::Tile* coarseTile = NULL;
    GPUTileStorage::GPUSlot *coarseGpuData = NULL;

    if (orthoTiles != NULL) {
        if (hasLayers() && !orthoTiles->hasTile(level, tx, ty)) {
            int l = level;
            int x = tx;
            int y = ty;
            while (!coarseGpuTiles->hasTile(l, x, y)) {
                l -= 1;
                x /= 2;
                y /= 2;
            }

            coarseTile = coarseGpuTiles->findTile(l, x, y);
            assert(coarseTile != NULL);
            coarseGpuData = dynamic_cast<GPUTileStorage::GPUSlot*>(coarseTile->getData());
            assert(coarseGpuData != NULL);
        } else {
            TileCache::Tile *t = orthoTiles->findTile(level, tx, ty);
            assert(t != NULL);
            cpuData = dynamic_cast<CPUTileStorage<unsigned char>::CPUSlot*>(t->getData());
            assert(cpuData != NULL);
        }
    }

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
    if (compressedTexture == NULL && !hasLayers()) {
        assert(cpuData != NULL);
        if (channels != 2 || tileSize%2 == 0) {
            gpuData->setSubImage(0, 0, tileSize, tileSize, f, UNSIGNED_BYTE, Buffer::Parameters(), CPUBuffer(cpuData->data));
        } else {
            // TODO better way to fix this "OpenGL bug" (?) with odd texture sizes?
            unsigned char *tmp = new unsigned char[tileSize*tileSize*4];
            for (int i = 0; i < tileSize*tileSize; ++i) {
                tmp[4*i] = cpuData->data[2*i];
                tmp[4*i+1] = cpuData->data[2*i+1];
                tmp[4*i+2] = 0;
                tmp[4*i+3] = 0;
            }
            gpuData->setSubImage(0, 0, tileSize, tileSize, RGBA, UNSIGNED_BYTE, Buffer::Parameters(), CPUBuffer(tmp));
            delete[] tmp;
        }
    } else {
        if (cpuData != NULL) {
            if (compressedTexture != NULL) {
                compressedTexture->setCompressedSubImage(0, 0, 0, tileSize, tileSize, cpuData->size, CPUBuffer(cpuData->data));
                uncompressSourceU->set(compressedTexture);
                frameBuffer->drawQuad(uncompress);
            } else {
                uncompressedTexture->setSubImage(0, 0, 0, tileSize, tileSize, f, UNSIGNED_BYTE, Buffer::Parameters(), CPUBuffer(cpuData->data));
            }
        }
        if (coarseGpuData != NULL) {
            vec4f coords = coarseGpuTiles->getGpuTileCoords(level, tx, ty, &coarseTile);
            float b = float(getBorder()) / (1 << (level - coarseTile->level));
            float s = (float) getCache()->getStorage()->getTileSize();
            float S = s / (s - 2 * getBorder());
            coords.x -= b / coarseGpuData->getWidth();
            coords.y -= b / coarseGpuData->getHeight();
            coords.w *= S;

            upsampleSourceU->set(coarseGpuData->t);
            tileU->set(coords);
            frameBuffer->drawQuad(upsample);
        }
        if (hasLayers()) {
            TileProducer::doCreateTile(level, tx, ty, data);
        }
        gpuData->copyPixels(frameBuffer, 0, 0, tileSize, tileSize);
    }

    return true;
}

void OrthoGPUProducer::endCreateTile()
{
    TileProducer::endCreateTile();
    SceneManager::setCurrentFrameBuffer(old);
    old = NULL;
}

void OrthoGPUProducer::stopCreateTile(int level, int tx, int ty)
{
    if (orthoTiles != NULL) {
        TileCache::Tile *t;
        if (hasLayers() && !orthoTiles->hasTile(level, tx, ty)) {
            int l = level;
            int x = tx;
            int y = ty;
            while (!coarseGpuTiles->hasTile(l, x, y)) {
                l -= 1;
                x /= 2;
                y /= 2;
            }
            t = coarseGpuTiles->findTile(l, x, y);
            assert(t != NULL);
            coarseGpuTiles->putTile(t);
        } else {
            t = orthoTiles->findTile(level, tx, ty);
            assert(t != NULL);
            orthoTiles->putTile(t);
        }
    }

    TileProducer::stopCreateTile(level, tx, ty);
}

void OrthoGPUProducer::swap(ptr<OrthoGPUProducer> p)
{
    TileProducer::swap(p);
    std::swap(frameBuffer, p->frameBuffer);
    std::swap(old, p->old);
    std::swap(orthoTiles, p->orthoTiles);
    std::swap(coarseGpuTiles, p->coarseGpuTiles);
    std::swap(channels, p->channels);
    std::swap(tileSize, p->tileSize);
    std::swap(compressedTexture, p->compressedTexture);
    std::swap(uncompressedTexture, p->uncompressedTexture);
}

class OrthoGPUProducerResource : public ResourceTemplate<3, OrthoGPUProducer>
{
public:
    OrthoGPUProducerResource(ptr<ResourceManager> manager, const string &name, ptr<ResourceDescriptor> desc,
            const TiXmlElement *e = NULL) :
        ResourceTemplate<3, OrthoGPUProducer> (manager, name, desc)
    {
        e = e == NULL ? desc->descriptor : e;
        ptr<TileCache> cache;
        ptr<TileCache> backgroundCache;
        ptr<TileProducer> ortho;
        int maxLevel = -1;
        ptr<Texture2D> compressedTexture;
        ptr<Texture2D> uncompressedTexture;
        checkParameters(desc, e, "name,cache,backgroundCache,ortho,maxLevel,");
        cache = manager->loadResource(getParameter(desc, e, "cache")).cast<TileCache>();
        if (e->Attribute("backgroundCache") != NULL) {
            backgroundCache = manager->loadResource(getParameter(desc, e, "backgroundCache")).cast<TileCache>();
        }
        if (e->Attribute("ortho") != NULL) {
            ortho = manager->loadResource(getParameter(desc, e, "ortho")).cast<TileProducer>();
        }
        if (e->Attribute("maxLevel") != NULL) {
            getIntParameter(desc, e, "maxLevel", &maxLevel);
        }

        bool hasLayers = false;
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
                hasLayers = true;
            } else {
                if (Logger::WARNING_LOGGER != NULL) {
                    log(Logger::WARNING_LOGGER, desc, f, "Unknown scene node element '" + f->ValueStr() + "'");
                }
            }
            n = n->NextSibling();
        }

        if (ortho != NULL && ortho.cast<OrthoCPUProducer> ()->isCompressed()) {
            int tileSize = ortho->getCache()->getStorage()->getTileSize();
            int channels = ortho->getCache()->getStorage().cast<CPUTileStorage<unsigned char> >()->getChannels();
            assert(tileSize == cache->getStorage()->getTileSize());
            assert(channels >= 3);
            ostringstream compTex;
            compTex << "renderbuffer-" << tileSize << "-" << (channels == 3 ? "COMPRESSED_RGB_S3TC_DXT1_EXT" : "COMPRESSED_RGBA_S3TC_DXT5_EXT");
            compressedTexture = manager->loadResource(compTex.str()).cast<Texture2D>();
        }

        if ((ortho != NULL && ortho.cast<OrthoCPUProducer>()->isCompressed()) || hasLayers) {
            int tileSize = cache->getStorage()->getTileSize();
            int channels = cache->getStorage().cast<GPUTileStorage>()->getTexture(0)->getComponents();
            ostringstream uncompTex;
            uncompTex << "renderbuffer-" << tileSize << "-" << (channels == 3 ? "RGB8" : "RGBA8");
            uncompressedTexture = manager->loadResource(uncompTex.str()).cast<Texture2D>();
        }

        assert(ortho != NULL || hasLayers);

        init(cache, backgroundCache, ortho, maxLevel, compressedTexture, uncompressedTexture);
    }
};

extern const char orthoGpuProducer[] = "orthoGpuProducer";

static ResourceFactory::Type<orthoGpuProducer, OrthoGPUProducerResource> OrthoGPUProducerType;

}
