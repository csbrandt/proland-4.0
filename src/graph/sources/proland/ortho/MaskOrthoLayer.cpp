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

#include "proland/ortho/MaskOrthoLayer.h"
#include "proland/ortho/OrthoMargin.h"

#include "ork/scenegraph/SceneManager.h"
#include "proland/producer/ObjectTileStorage.h"

using namespace ork;

namespace proland
{

MaskOrthoLayer::MaskOrthoLayer() : GraphLayer("MaskOrthoLayer")
{
}

MaskOrthoLayer::MaskOrthoLayer(ptr<GraphProducer> graphs, set<int> ignored, ptr<Program> layerProgram,
    int writeMask, vec4f color, float depth, float widthFactor, MaskOrthoLayer::BlendParams blendParams, vec4f blendColor, int displayLevel, bool deform) : GraphLayer("MaskOrthoLayer")
{
    init(graphs, ignored, layerProgram, writeMask, color, depth, widthFactor, blendParams, blendColor, displayLevel, deform);
}

MaskOrthoLayer::~MaskOrthoLayer()
{
}

void MaskOrthoLayer::init(ptr<GraphProducer> graphs, set<int> ignored, ptr<Program> layerProgram,
    int writeMask, vec4f color, float depth, float widthFactor, BlendParams blendParams, vec4f blendColor, int displayLevel, bool deform)
{
    GraphLayer::init(graphs, layerProgram, displayLevel, false, false, deform);
    this->ignored = ignored;
    this->writeMask = writeMask;
    this->color = color;
    this->depth = depth;
    this->widthFactor = widthFactor;
    this->blendParams = blendParams;
    this->blendColor = blendColor;
    this->mesh = new Mesh<vec2f, unsigned int>(TRIANGLES, GPU_STREAM);
    this->mesh->addAttributeType(0, 2, A32F, false);
    this->tess = new Tesselator();

    tileOffsetU = layerProgram->getUniform3f("tileOffset");
    depthU = layerProgram->getUniform1f("depth");
    colorU = layerProgram->getUniform4f("color");
}

void MaskOrthoLayer::setTileSize(int tileSize, int tileBorder, float rootQuadSize)
{
    GraphLayer::setTileSize(tileSize, tileBorder, rootQuadSize);
    float borderFactor = tileSize / (tileSize - 1.0f - 2.0f * tileBorder) - 1.0f;
    graphProducer->addMargin(new OrthoMargin(tileSize - 2 * tileBorder, borderFactor, 1.0f));
}

bool MaskOrthoLayer::doCreateTile(int level, int tx, int ty, TileStorage::Slot *data)
{
    if (Logger::DEBUG_LOGGER != NULL) {
        ostringstream oss;
        oss << "OrthoMask tile " << getProducerId() << " " << level << " " << tx << " " << ty;
        Logger::DEBUG_LOGGER->log("ORTHO", oss.str());
    }
    if (level >= displayLevel) {
        TileCache::Tile * t = graphProducer->findTile(level, tx, ty);
        assert(t != NULL);
        ObjectTileStorage::ObjectSlot *graphData = dynamic_cast<ObjectTileStorage::ObjectSlot*>(t->getData());
        GraphPtr g = graphData->data.cast<Graph>();
        if (g != NULL) {
            ptr<FrameBuffer> fb = SceneManager::getCurrentFrameBuffer();
            FrameBuffer::Parameters old = fb->getParameters();

            if (blendParams.buffer != -1) {
                fb->setBlend(blendParams.buffer, blendParams.enable, blendParams.rgb, blendParams.srgb, blendParams.drgb,
                    blendParams.alpha, blendParams.salpha, blendParams.dalpha);
            } else {
                fb->setBlend(blendParams.enable, blendParams.rgb, blendParams.srgb, blendParams.drgb,
                    blendParams.alpha, blendParams.salpha, blendParams.dalpha);
            }
            fb->setBlendColor(blendColor);

            fb->setColorMask(0 != (writeMask & 1), 0 != (writeMask & 2), 0 != (writeMask & 4), 0 != (writeMask & 8));
            fb->setDepthMask(0 != (writeMask & 16));
            fb->setStencilMask(0 != (writeMask & 32), 0 != (writeMask & 64));

            vec3d q = getTileCoords(level, tx, ty);
            float scale = 2.0f * (1.0f - getTileBorder() * 2.0f / getTileSize()) / q.z;
            float scale2 = 2.0f * (getTileSize() - 2.0f * getTileBorder()) / q.z;

            vec2d nx, ny, lx, ly;
            getDeformParameters(q, nx, ny, lx, ly);

            vec3d tileOffset = vec3d(q.x + q.z / 2.0f, q.y + q.z / 2.0f, scale);
            //tileOffsetU->set(vec3f(q.x + q.z / 2.0f, q.y + q.z / 2.0f, scale));
            tileOffsetU->set(vec3f(0.0, 0.0, 1.0));
            colorU->set(vec4f(color.x, color.y, color.z, color.w));
            depthU->set(depth);

            mesh->setMode(TRIANGLES);
            mesh->clear();
            ptr<Graph::AreaIterator> ai = g->getAreas();
            tess->beginPolygon(mesh);
            while (ai->hasNext()) {
                AreaPtr a = ai->next();
                drawArea(tileOffset, a, *tess);
            }
            tess->endPolygon();
            fb->draw(layerProgram, *mesh);
            ptr<Graph::CurveIterator> ci = g->getCurves();
            while (ci->hasNext()) {
                CurvePtr p = ci->next();
                if (ignored.find(p->getType()) != ignored.end()) {
                    continue;
                }

                float pwidth = widthFactor * p->getWidth();
                float swidth = pwidth * scale2;
                if (swidth > 0.1) {
                    if (isDeformed()) {
                        drawCurve(tileOffset, p, pwidth, scale * getTileSize(), fb, layerProgram, *mesh, &nx, &ny, &lx, &ly);
                    } else {
                        drawCurve(tileOffset, p, pwidth, scale, fb, layerProgram, *mesh);
                    }
                }
            }
            fb->setColorMask(true, true, true, true);
            fb->setDepthMask(true);
            fb->setStencilMask(0xFFFFFFFF, 0xFFFFFFFF);
            fb->setBlendColor(vec4f::ZERO);
            fb->setBlend(false);
        } else {
            if (Logger::DEBUG_LOGGER != NULL) {
                ostringstream oss;
                oss << "NULL Graph : " << level << " " << tx << " " << ty;
                Logger::DEBUG_LOGGER->log("GRAPH", oss.str());
            }
        }
    }
    return true;
}

void MaskOrthoLayer::swap(ptr<MaskOrthoLayer> p)
{
    GraphLayer::swap(p);
    std::swap(blendParams, p->blendParams);
    std::swap(blendColor, p->blendColor);
    std::swap(color, p->color);
    std::swap(depth, p->depth);
    std::swap(tileOffsetU, p->tileOffsetU);
    std::swap(colorU, p->colorU);
    std::swap(depthU, p->depthU);
    std::swap(mesh, p->mesh);
    std::swap(tess, p->tess);
    std::swap(ignored, p->ignored);
}

class MaskOrthoLayerResource : public ResourceTemplate<40, MaskOrthoLayer>
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

    MaskOrthoLayerResource(ptr<ResourceManager> manager, const string &name, ptr<ResourceDescriptor> desc,
            const TiXmlElement *e = NULL) :
        ResourceTemplate<40, MaskOrthoLayer> (manager, name, desc)
    {
        e = e == NULL ? desc->descriptor : e;
        ptr<GraphProducer>graphProducer;
        int displayLevel = 0;
        //int channel = 0;
        // R  G  B  A DEPTH FSTENCIL BSTENCIL
        // 1  2  4  8 16    32       64
        int writeMask = 0;
        float depth = 0.f;
        float widthFactor = 1.0f;
        vec4f color = vec4f(0.f, 0.f, 0.f, 0.f);
        vec4f blendColor = vec4f(0.f, 0.f, 0.f, 0.f);
        set<int> ignored;
        bool deform;

        checkParameters(desc, e, "name,graph,deform,ignore,renderProg,level,blendBuffer,blendColor,color,depth,widthFactor,channels,equation,sourceFunction,destinationFunction,equationAlpha,sourceFunctionAlpha,destinationFunctionAlpha,");
        string g = getParameter(desc, e, "graph");

        graphProducer = manager->loadResource(g).cast<GraphProducer>();
        if (e->Attribute("level") != NULL) {
            getIntParameter(desc, e, "level", &displayLevel);
        }
        string channels = getParameter(desc, e, "channels");
        for (int i = 0; i < (int) channels.size(); i++) {
            switch (channels[i]) {
                case 'r': {writeMask += 1; break;}
                case 'g': {writeMask += 2; break;}
                case 'b': {writeMask += 4; break;}
                case 'a': {writeMask += 8; break;}
                case 'D': {writeMask += 16; break;}
                case 'F': {writeMask += 32; break;}
                case 'B': {writeMask += 64; break;}
                default:{
                    if (Logger::ERROR_LOGGER != NULL) {
                        Logger::ERROR_LOGGER->logf("ORTHO", "Invalid channel marker '%c'", channels[i]);
                    }
                }
            }
        }
        if (e->Attribute("ignore") != NULL) {
            string i = getParameter(desc, e, "ignore");
            string::size_type start = 0;
            string::size_type index;
            while ((index = i.find(',', start)) != string::npos) {
                ignored.insert(atoi(i.substr(start, index - start).c_str()));
                start = index + 1;
            }
        }
        if (e->Attribute("blendColor") != NULL) {
            string c = getParameter(desc, e, "blendColor") + ",";
            string::size_type start = 0;
            string::size_type index;
            for (int i = 0; i < 4; i++) {
                index = c.find(',', start);
                blendColor[i] = (float) atof(c.substr(start, index - start).c_str()) / 255;
                start = index + 1;
            }
        }
        if (e->Attribute("color") != NULL) {
            string c = getParameter(desc, e, "color") + ",";
            string::size_type start = 0;
            string::size_type index;
            for (int i = 0; i < 4; i++) {
                index = c.find(',', start);
                color[i] = (float) atof(c.substr(start, index - start).c_str()) / 255;
                start = index + 1;
            }
        }
        if (e->Attribute("depth") != NULL) {
            getFloatParameter(desc, e, "depth", &depth);
        }
        if (e->Attribute("widthFactor") != NULL) {
            getFloatParameter(desc, e, "widthFactor", &widthFactor);
        }

        BlendParams blendParams;
        if (e->Attribute("equation") != NULL) {
            blendParams.rgb = getBlendEquation(desc, e, "equation");
            blendParams.srgb = getBlendArgument(desc, e, "sourceFunction");
            blendParams.drgb = getBlendArgument(desc, e, "destinationFunction");
        }
        if (e->Attribute("equationAlpha") != NULL) {
            blendParams.alpha = getBlendEquation(desc, e, "equationalpha");
            blendParams.salpha = getBlendArgument(desc, e, "sourceFunctionAlpha");
            blendParams.dalpha = getBlendArgument(desc, e, "destinationFunctionAlpha");
        }
        const char *c = e->Attribute("blendBuffer");
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

        if (e->Attribute("deform") != NULL) {
            deform = strcmp(e->Attribute("deform"), "true") == 0;
        }

        ptr<Program> layerProgram = manager->loadResource(getParameter(desc, e, "renderProg")).cast<Program>();
        init(graphProducer, ignored, layerProgram, writeMask, color, depth, widthFactor, blendParams, blendColor, displayLevel, deform);
    }

    virtual bool prepareUpdate()
    {
        bool changed = false;
        if (dynamic_cast<Resource*>(layerProgram.get())->changed()) {
            changed = true;
        }

        if (changed) {
            invalidateTiles();
        }
        oldValue = NULL;
        newDesc = NULL;

        return true;
    }
};

extern const char maskOrthoLayer[] = "maskOrthoLayer";

static ResourceFactory::Type<maskOrthoLayer, MaskOrthoLayerResource> MaskOrthoLayerType;

}
