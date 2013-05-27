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

#include "proland/ortho/TextureLayer.h"

#include "ork/core/Logger.h"
#include "ork/resource/ResourceTemplate.h"
#include "ork/render/FrameBuffer.h"
#include "ork/scenegraph/SceneManager.h"

#include "proland/producer/GPUTileStorage.h"

using namespace std;
using namespace ork;

namespace proland
{

TextureLayer::TextureLayer(ptr<TileProducer> tiles, ptr<Program> program,
    std::string tilesSamplerName, BlendParams blend, int minDisplayLevel, bool storeTiles) : TileLayer("TextureLayer")
{
    init(tiles, program, tilesSamplerName, blend, minDisplayLevel, storeTiles);
}

TextureLayer::TextureLayer() : TileLayer("TextureLayer")
{
}

void TextureLayer::init(ptr<TileProducer> tiles, ptr<Program> program,
    std::string tilesSamplerName, BlendParams blend, int minDisplayLevel, bool storeTiles)
{
    TileLayer::init(false);

    this->tiles = tiles;
    this->program = program;
    this->tilesSamplerName = tilesSamplerName;
    this->blend = blend;
    this->minDisplayLevel = minDisplayLevel;
    this->storeTiles = storeTiles;

    assert(tiles->isGpuProducer());

    ptr<GPUTileStorage> storage = tiles->getCache()->getStorage().cast<GPUTileStorage>();
    assert(storage != NULL);

    samplerU = program->getUniformSampler(tilesSamplerName + ".tilePool");
    coordsU = program->getUniform3f(tilesSamplerName + ".tileCoords");
    sizeU = program->getUniform3f(tilesSamplerName + ".tileSize");
}

TextureLayer::~TextureLayer()
{
}

void TextureLayer::getReferencedProducers(vector< ptr<TileProducer> > &producers) const
{
    producers.push_back(tiles);
}

void TextureLayer::useTile(int level, int tx, int ty, unsigned int deadline)
{
    if (storeTiles && level >= minDisplayLevel) {
        int l = level;
        int x = tx;
        int y = ty;
        while (!tiles->hasTile(l, x, y)) {
            l -= 1;
            x /= 2;
            y /= 2;
            assert(l >= 0);
        }
        TileCache::Tile *t = tiles->getTile(l, x, y, deadline);
        assert(t != NULL);
    }
}

void TextureLayer::unuseTile(int level, int tx, int ty)
{
    if (storeTiles && level >= minDisplayLevel) {
        int l = level;
        int x = tx;
        int y = ty;
        while (!tiles->hasTile(l, x, y)) {
            l -= 1;
            x /= 2;
            y /= 2;
            assert(l >= 0);
        }
        TileCache::Tile *t = tiles->findTile(l, x, y);
        assert(t != NULL);
        tiles->putTile(t);
    }
}

void TextureLayer::startCreateTile(int level, int tx, int ty, unsigned int deadline, ptr<Task> task, ptr<TaskGraph> result)
{
    if ((result != NULL) && (level >= minDisplayLevel)) {
        int l = level;
        int x = tx;
        int y = ty;
        while (!tiles->hasTile(l, x, y)) {
            l -= 1;
            x /= 2;
            y /= 2;
            assert(l >= 0);
        }

        TileCache::Tile *tile = tiles->getTile(l, x, y, deadline);

        // if you fail here, try to log "CACHE" debug messages
        // because a cache might be full
        assert(tile != NULL);

        result->addTask(tile->task);
        result->addDependency(task, tile->task);
    }
}

void TextureLayer::stopCreateTile(int level, int tx, int ty)
{
    if (level >= minDisplayLevel) {
        int l = level;
        int x = tx;
        int y = ty;
        while (!tiles->hasTile(l, x, y)) {
            l -= 1;
            x /= 2;
            y /= 2;
            assert(l >= 0);
        }
        TileCache::Tile *t = tiles->findTile(l, x, y);
        if (t != NULL) {
            tiles->putTile(t);
        }
    }
}

bool TextureLayer::doCreateTile(int level, int tx, int ty, TileStorage::Slot *data)
{
    if (Logger::DEBUG_LOGGER != NULL) {
        ostringstream oss;
        oss << "Texture tile " << getProducerId() << " " << level << " " << tx << " " << ty;
        Logger::DEBUG_LOGGER->log("ORTHO", oss.str());
    }

    if (level < minDisplayLevel) {
        return true;
    }

    int l = level;
    int x = tx;
    int y = ty;
    while (!tiles->hasTile(l, x, y)) {
        l -= 1;
        x /= 2;
        y /= 2;
        assert(l >= 0);
    }

    ptr<FrameBuffer> fb = SceneManager::getCurrentFrameBuffer();

    TileCache::Tile *t = tiles->findTile(l, x, y);
    assert(t != NULL);
    GPUTileStorage::GPUSlot *gput = dynamic_cast<GPUTileStorage::GPUSlot*>(t->getData());
    assert(gput != NULL);

    vec4f coords = tiles->getGpuTileCoords(level, tx, ty, &t);
    int s = tiles->getCache()->getStorage()->getTileSize();
    float b = float(tiles->getBorder()) / (1 << (level - l));
    float S = s / (s - 2.0f * tiles->getBorder());

    // correct border
    vec4f coordsCorrected = coords;
    coordsCorrected.x -= b / gput->getWidth();
    coordsCorrected.y -= b / gput->getHeight();
    coordsCorrected.w *= S;

    samplerU->set(gput->t);
    coordsU->set(vec3f(coordsCorrected.x, coordsCorrected.y, coords.z));
    sizeU->set(vec3f(coordsCorrected.w, coordsCorrected.w, (s / 2) * 2.0f - 2.0f * b));

    if (blend.buffer != (BufferId) -1) {
        fb->setBlend(blend.buffer, true, blend.rgb, blend.srgb, blend.drgb,
            blend.alpha, blend.salpha, blend.dalpha);
    } else {
        fb->setBlend(true, blend.rgb, blend.srgb, blend.drgb,
            blend.alpha, blend.salpha, blend.dalpha);
    }

    fb->drawQuad(program);

    if (blend.buffer != (BufferId) -1) {
        fb->setBlend(blend.buffer, false);
    } else {
        fb->setBlend(false);
    }

    return true;
}

void TextureLayer::swap(ptr<TextureLayer> p)
{
    TileLayer::swap(p);
    std::swap(program, p->program);
    std::swap(tiles, p->tiles);
    std::swap(tilesSamplerName, p->tilesSamplerName);
    std::swap(blend, p->blend);
    std::swap(storeTiles, p->storeTiles);
    std::swap(minDisplayLevel, p->minDisplayLevel);
    std::swap(samplerU, p->samplerU);
    std::swap(coordsU, p->coordsU);
    std::swap(sizeU, p->sizeU);
}

class TextureOrthoLayerResource : public ResourceTemplate<40, TextureLayer>
{
public:
    BlendEquation getBlendEquation(ptr<ResourceDescriptor> desc, const TiXmlElement *e, const char *name)
    {
        try {
            if (strcmp(e->Attribute(name), "ADD") == 0) {
                return ADD;
            }
            if (strcmp(e->Attribute(name), "SUBTRACT") == 0) {
                return SUBTRACT;
            }
            if (strcmp(e->Attribute(name), "REVERSE_SUBTRACT") == 0) {
                return REVERSE_SUBTRACT;
            }
            if (strcmp(e->Attribute(name), "MIN") == 0) {
                return MIN;
            }
            if (strcmp(e->Attribute(name), "MAX") == 0) {
                return MAX;
            }
        } catch (...) {
        }
        if (Logger::ERROR_LOGGER != NULL) {
            log(Logger::ERROR_LOGGER, desc, e, "Invalid blend equation");
        }
        throw exception();
    }

    BlendArgument getBlendArgument(ptr<ResourceDescriptor> desc, const TiXmlElement *e, const char *name)
    {
        try {
            if (strcmp(e->Attribute(name), "ZERO") == 0) {
                return ZERO;
            }
            if (strcmp(e->Attribute(name), "ONE") == 0) {
                return ONE;
            }
            if (strcmp(e->Attribute(name), "SRC_COLOR") == 0) {
                return SRC_COLOR;
            }
            if (strcmp(e->Attribute(name), "ONE_MINUS_SRC_COLOR") == 0) {
                return ONE_MINUS_SRC_COLOR;
            }
            if (strcmp(e->Attribute(name), "DST_COLOR") == 0) {
                return DST_COLOR;
            }
            if (strcmp(e->Attribute(name), "ONE_MINUS_DST_COLOR") == 0) {
                return ONE_MINUS_DST_COLOR;
            }
            if (strcmp(e->Attribute(name), "SRC_ALPHA") == 0) {
                return SRC_ALPHA;
            }
            if (strcmp(e->Attribute(name), "ONE_MINUS_SRC_ALPHA") == 0) {
                return ONE_MINUS_SRC_ALPHA;
            }
            if (strcmp(e->Attribute(name), "DST_ALPHA") == 0) {
                return DST_ALPHA;
            }
            if (strcmp(e->Attribute(name), "ONE_MINUS_DST_ALPHA") == 0) {
                return ONE_MINUS_DST_ALPHA;
            }
            if (strcmp(e->Attribute(name), "CONSTANT_COLOR") == 0) {
                return CONSTANT_COLOR;
            }
            if (strcmp(e->Attribute(name), "ONE_MINUS_CONSTANT_COLOR") == 0) {
                return ONE_MINUS_CONSTANT_COLOR;
            }
            if (strcmp(e->Attribute(name), "CONSTANT_ALPHA") == 0) {
                return CONSTANT_ALPHA;
            }
            if (strcmp(e->Attribute(name), "ONE_MINUS_CONSTANT_ALPHA") == 0) {
                return ONE_MINUS_CONSTANT_ALPHA;
            }
        } catch (...) {
        }
        if (Logger::ERROR_LOGGER != NULL) {
            log(Logger::ERROR_LOGGER, desc, e, "Invalid blend argument");
        }
        throw exception();
    }

    TextureOrthoLayerResource(ptr<ResourceManager> manager, const string &name, ptr<ResourceDescriptor> desc,
            const TiXmlElement *e = NULL) :
        ResourceTemplate<40, TextureLayer> (manager, name, desc)
    {
        e = e == NULL ? desc->descriptor : e;

        checkParameters(desc, e, "name,producer,renderProg,tileSamplerName,storeTiles,equation,sourceFunction,destinationFunction,equationAlpha,sourceFunctionAlpha,destinationFunctionAlpha,level,");

        ptr<Object> programRessource = manager->loadResource(getParameter(desc, e, "renderProg"));
        assert(programRessource != NULL);
        ptr<Program> program = programRessource.cast<Program>();
        assert(program != NULL);

        ptr<TileProducer> tiles = manager->loadResource(getParameter(desc, e, "producer")).cast<TileProducer>();
        assert(tiles != NULL);

        std::string tileSamplerName = getParameter(desc, e, "tileSamplerName");
        assert(tileSamplerName != "");

        BlendParams blendParams;
        blendParams.rgb = ADD;
        blendParams.srgb = SRC_ALPHA;
        blendParams.drgb = ONE_MINUS_SRC_ALPHA;
        blendParams.alpha = ADD;
        blendParams.salpha = SRC_ALPHA;
        blendParams.dalpha = ONE_MINUS_SRC_ALPHA;
        const char *c = e->Attribute("buffer");
        if (c != NULL) {
            if (strcmp(c, "COLOR0") == 0) {
                blendParams.buffer = COLOR0;
            } else if (strcmp(c, "COLOR1") == 0) {
                blendParams.buffer = COLOR1;
            } else if (strcmp(c, "COLOR2") == 0) {
                blendParams.buffer = COLOR2;
            } else if (strcmp(c, "COLOR3") == 0) {
                blendParams.buffer = COLOR3;
            } else if (strcmp(c, "DEPTH") == 0) {
                blendParams.buffer = DEPTH;
            } else if (strcmp(c, "STENCIL") == 0) {
                blendParams.buffer = STENCIL;
            }
        }
        if (e->Attribute("equation") != NULL) {
            blendParams.rgb = getBlendEquation(desc, e, "equation");
            blendParams.srgb = getBlendArgument(desc, e, "sourceFunction");
            blendParams.drgb = getBlendArgument(desc, e, "destinationFunction");
        }
        if (e->Attribute("equationAlpha") != NULL) {
            blendParams.alpha = getBlendEquation(desc, e, "equationAlpha");
            blendParams.salpha = getBlendArgument(desc, e, "sourceFunctionAlpha");
            blendParams.dalpha = getBlendArgument(desc, e, "destinationFunctionAlpha");
        }

        int displayLevel = 0;
        if (e->Attribute("level") != NULL) {
            getIntParameter(desc, e, "level", &displayLevel);
        }

        bool storeTiles = false;
        if (e->Attribute("storeTiles") != NULL) {
            storeTiles = strcmp(e->Attribute("storeTiles"), "true") == 0;
        }

        init(tiles, program, tileSamplerName, blendParams, displayLevel, storeTiles);
    }

    virtual bool prepareUpdate() {
        oldValue = NULL;
        newDesc = NULL;
        if (dynamic_cast<Resource*>(program.get())->changed()) {
            invalidateTiles();
        }
        return true;
    }
};

extern const char TextureLayer[] = "textureLayer";

static ResourceFactory::Type<TextureLayer, TextureOrthoLayerResource> TextureOrthoLayerType;

}
