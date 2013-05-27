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

#include "proland/dem/WaterElevationLayer.h"

#include "ork/resource/ResourceTemplate.h"
#include "ork/scenegraph/SceneManager.h"

#include "proland/graph/Area.h"
#include "proland/producer/ObjectTileStorage.h"

using namespace ork;

namespace proland
{

#define BASEWIDTH(width, scale) ((width) + 2.0f * sqrt(2.0f)/(scale))
#define TOTALWIDTH(basewidth) ((basewidth) * 3.0f)

WaterElevationLayer::WaterElevationCurveData::WaterElevationCurveData(CurveId id, CurvePtr flattenCurve, ptr<TileProducer> elevations) :
    ElevationCurveData(id, flattenCurve, elevations, true)
{
    if (flattenCurve->getType() == LAKE && flattenCurve->getWidth() > 12) { // large river
        if (startCapLength + endCapLength > 2 * length / 3) {
            startCapLength = endCapLength = length / 3;
        }
    }
}

WaterElevationLayer::WaterElevationCurveData::~WaterElevationCurveData()
{
}

float WaterElevationLayer::WaterElevationCurveData::getAltitude(float s)
{
    float L = getCurvilinearLength();
    float l = flattenCurve->getCurvilinearLength(s, NULL, NULL);

    if (l < startCapLength) {
        return getStartHeight();
    }
    if (l > L - endCapLength) {
        return getEndHeight();
    }

    if (flattenCurve->getType() == LAKE) {
        float h0 = getSample(0);
        float h1 = getSample(sampleCount - 1);
        if (flattenCurve->getStart()->getCurveCount() == 1) {
            return h1;
        }
        if (flattenCurve->getEnd()->getCurveCount() == 1) {
            return h0;
        }

        float c = (l - startCapLength) / (L - startCapLength - endCapLength);
        c = 0.5f + 0.5f * sin((c - 0.5f) * M_PI); // ??????
        return h0 * (1 - c) + h1 * c;
    }

    float x = (flattenCurve->getWidth() + 4) * 4;
    float flat0 = startCapLength == 0.f ? 0.f : startCapLength + x;
    float flat1 = endCapLength == 0.f ? 0.f : endCapLength + x;

    if (flat0 + flat1 > L) {
        float h0 = getStartHeight();
        float h1 = getEndHeight();
        float c = (l - startCapLength) / (L - startCapLength - endCapLength);
        c = 0.5f + 0.5f * sin((c - 0.5f) * M_PI); // ??????
        return h0 * (1 - c) + h1 * c;
    }

    int i = (int) floor(l / sampleLength);
    float t = l / sampleLength - i;

    float h0 = getSmoothedSample(i);
    float h1 = getSmoothedSample(i + 1);

    float hp0 = (h1 - getSmoothedSample(i - 1))/2;
    float hp1 = (getSmoothedSample(i + 2) - h0)/2;
    float dhp = hp1 - hp0;
    float dh = h1 - h0 - hp0;
    float z = (((dhp - 2*dh)*t + (3*dh - dhp))*t + hp0)*t + h0; // ??????

    if (l < flat0) {
        float z0 = getStartHeight();
        float c = (l - startCapLength) / (flat0 - startCapLength);
        c = 0.5f + 0.5f * sin((c - 0.5f) * M_PI); // ??????
        return z0 * (1 - c) + z * c;
    }
    if (l > L - flat1) {
        l = L - l;
        float z1 = getEndHeight();
        float c = (l - endCapLength) / (flat1 - endCapLength);
        c = 0.5f + 0.5f * sin((c - 0.5f) * M_PI); // ??????
        return z1 * (1 - c) + z * c;
    }
    return z;
}

float WaterElevationLayer::WaterElevationCurveData::getCapLength(NodePtr p, vec2d q)
{
    vec2d o = p->getPos();
    float capLength = 0;
    bool largeRiver = false;
    for (int i = 0; i < p->getCurveCount(); ++i) {
        CurvePtr ipath = p->getCurve(i);
        if ((ipath->getAncestor()->getId() == id) == false) {
            if (ipath->getType() == LAKE && ipath->getWidth() > 12) {
                largeRiver = true;
            }
            vec2d r = ipath->getXY(p, 1);
            if (abs(angle(q - o, r - o) - M_PI) < 0.01) {
                continue;
            }
            float pw = flattenCurve->getType() == RIVER ? 2 * flattenCurve->getWidth() : flattenCurve->getWidth();
            float ipw = ipath->getType() == RIVER ? 2 * ipath->getWidth() : ipath->getWidth();
            vec2d corner = proland::corner(o, q, r, (double) pw, (double) ipw);
            float dot = (q - o).dot(corner - o);
            capLength = max((double) capLength, dot / (o - q).length());
        }
    }
    if (largeRiver && flattenCurve->getType() == RIVER) {
        capLength = (q - o).length();
    }
    return ceil(capLength);
}

float WaterElevationLayer::WaterElevationCurveData::getSampleLength(CurvePtr c)
{
    float width = min(c->getWidth(), 20.0f);
    if (c->getType() == LAKE) {
        width = 6.0f;
    }
    return 20.0f * width / 6.0f;
}

WaterElevationLayer::WaterElevationMargin::WaterElevationMargin(int samplesPerTile, float borderFactor) :
    ElevationMargin(samplesPerTile, borderFactor)
{
}

WaterElevationLayer::WaterElevationMargin::~WaterElevationMargin()
{
}

double WaterElevationLayer::WaterElevationMargin::getMargin(double clipSize, CurvePtr p)
{
    float pwidth = p->getWidth();
    if (p->getType() == WaterElevationCurveData::RIVER) {
        float scale = 2.0f * (samplesPerTile - 1) / clipSize;
        if (p->getParent() != NULL && pwidth * scale >= 1) {
            return TOTALWIDTH(BASEWIDTH(pwidth, scale));
        } else {
            return 0;
        }
    } else {
        return pwidth / 2;
    }
}

double WaterElevationLayer::WaterElevationMargin::getMargin(double clipSize, AreaPtr a)
{
    float m = Margin::getMargin(clipSize, a);
    return m == 0.0f ? 12.0f : m;
}

WaterElevationLayer::WaterElevationLayer() : ElevationGraphLayer("WaterElevationLayer")
{
}

WaterElevationLayer::WaterElevationLayer(ptr<GraphProducer> graphProducer, ptr<Program> layerProgram, ptr<Program> fillProg, ptr<TileProducer> elevations, int displayLevel, bool quality, bool deform) :
    ElevationGraphLayer("WaterElevationLayer")
{
    init(graphProducer, layerProgram, fillProg, elevations, displayLevel, quality, deform);
}

void WaterElevationLayer::init(ptr<GraphProducer> graphProducer, ptr<Program> layerProgram, ptr<Program> fillProg, ptr<TileProducer> elevations, int displayLevel, bool quality, bool deform)
{
    ElevationGraphLayer::init(graphProducer, layerProgram, elevations, displayLevel, quality, false, deform);

    this->fillProg = fillProg;

    this->mesh = new Mesh<vec2f, unsigned int>(TRIANGLE_STRIP, GPU_STREAM);
    this->meshuv = new Mesh<vec4f, unsigned int>(TRIANGLE_STRIP, GPU_STREAM);
    this->mesh->addAttributeType(0, 2, A32F, false); // pos
    this->meshuv->addAttributeType(0, 2, A32F, false); //pos
    this->meshuv->addAttributeType(1, 2, A32F, false); //uv
    this->tess = new Tesselator();

    tileOffsetU = layerProgram->getUniform3f("tileOffset");
    riverU = layerProgram->getUniform1i("river");

    fillOffsetU = fillProg->getUniform3f("tileOffset");
    depthU = fillProg->getUniform1f("depth");
    colorU = fillProg->getUniform4f("color");
}

WaterElevationLayer::~WaterElevationLayer()
{
}

void WaterElevationLayer::setTileSize(int tileSize, int tileBorder, float rootQuadSize)
{
    GraphLayer::setTileSize(tileSize, tileBorder, rootQuadSize);
    float borderFactor = tileSize / (tileSize - 1.0f - 2.0f * tileBorder) - 1.0f;
    graphProducer->addMargin(new WaterElevationMargin(tileSize - 2 * tileBorder, borderFactor));
    elevations->setRootQuadSize(rootQuadSize);
}

CurveData *WaterElevationLayer::newCurveData(CurveId id, CurvePtr flattenCurve)
{
    return new WaterElevationCurveData(id, flattenCurve, elevations);
}

bool WaterElevationLayer::doCreateTile(int level, int tx, int ty, TileStorage::Slot *data)
{
    if (Logger::DEBUG_LOGGER != NULL) {
        ostringstream oss;
        oss << "ElevationRoad tile " << getProducerId() << " " << level << " " << tx << " " << ty;
        Logger::DEBUG_LOGGER->log("DEM", oss.str());
    }
    if (level >= displayLevel) {
        TileCache::Tile *t = graphProducer->findTile(level, tx, ty);
        assert(t != NULL);

        ObjectTileStorage::ObjectSlot *graphData = dynamic_cast<ObjectTileStorage::ObjectSlot*>(t->getData());
        GraphPtr g = graphData->data.cast<Graph>();

        if (g->getCurveCount() == 0) {
            return false;
        }

        ptr<FrameBuffer> fb = SceneManager::getCurrentFrameBuffer();

        vec3d q = getTileCoords(level, tx, ty);
        vec2d nx, ny, lx, ly;
        getDeformParameters(q, nx, ny, lx, ly);

        float scale = 2.0f * (getTileSize() - 1.0f - (2.0f * getTileBorder())) / q.z;

        vec3d tileOffset = vec3d(q.x + q.z / 2.0f, q.y + q.z / 2.0f, scale / getTileSize());
        //tileOffsetU->set(vec3f(q.x + q.z / 2.0f, q.y + q.z / 2.0f, scale / getTileSize()));
        tileOffsetU->set(vec3f(0.0, 0.0, 1.0));

        fb->clear(false, false, true);

        if (g->getAreaCount() > 0) {
            //fillOffsetU->set(vec3f(q.x + q.z / 2.0f, q.y + q.z / 2.0f, scale / getTileSize()));
            fillOffsetU->set(vec3f(0.0, 0.0, 1.0));
            depthU->set(0.02f);
            colorU->set(vec4f(0.0f, 0.0f, 0.0f, 0.0f));

            fb->setDepthTest(true, ALWAYS);
            fb->setColorMask(false, false, false, true);
            fb->setDepthMask(true);

            mesh->setMode(TRIANGLES);
            mesh->clear();
            tess->beginPolygon(mesh);
            ptr<Graph::AreaIterator> ai = g->getAreas();
            while (ai->hasNext()) {
                AreaPtr a = ai->next();
                GraphLayer::drawArea(tileOffset, a, *tess);
            }
            tess->endPolygon();
            fb->draw(fillProg, *mesh);

            riverU->set(1);

            ai = g->getAreas();
            while (ai->hasNext()) {
                AreaPtr a = ai->next();
                bool island = true;
                for (int j = 0; j < a->getCurveCount(); ++j) {
                    int o;
                    island &= (a->getCurve(j, o)->getType() == WaterElevationCurveData::ISLAND);
                    if (!island) {
                        break;
                    }
                }
                for (int j = 0; j < a->getCurveCount(); ++j) {
                    int orientation;
                    CurvePtr p = a->getCurve(j, orientation);
                    if (island) {
                        orientation = 1 - orientation;
                    } else {
                        if (p->getType() == WaterElevationCurveData::ISLAND) {
                            continue;
                        }
                    }
                    if (orientation != 0) {
                        GraphLayer::drawCurve(tileOffset, p, vec4f(0, 12, 1, 2), fb, layerProgram, *(meshuv), &nx, &ny, &lx, &ly);
                    } else {
                        GraphLayer::drawCurve(tileOffset, p, vec4f(0, -12, 1, 2), fb, layerProgram, *(meshuv), &nx, &ny, &lx, &ly);
                    }
                }
            }

            fb->setDepthTest(true, NOTEQUAL);
            fb->setColorMask(false, false, true, false);
            fb->setDepthMask(false);

            riverU->set(2);

            ptr<Graph::CurveIterator> ci = g->getCurves();
            while (ci->hasNext()) {
                CurvePtr c = ci->next();
                float w = c->getWidth();
                float tw = w;
                if (w * scale <= 1 || (c->getParent() == NULL && level != 0) || c->getType() == WaterElevationCurveData::LAKE || c->getType() == WaterElevationCurveData::RIVER || c->getArea1() != NULL) {//== WaterElevationCurveData::RIVER) {
                    continue;
                }
                ElevationCurveData *cData = dynamic_cast<ElevationCurveData*>(findCurveData(c));
                ElevationGraphLayer::drawCurveAltitude(tileOffset, c, cData, tw, tw / w, max(1.0f, 1.0f / scale), false, fb, layerProgram, *meshuv, &nx, &ny, &lx, &ly);
            }
        }

        fb->setColorMask(false, false, true, true);
        fb->setDepthTest(true, LESS);
        fb->setDepthMask(true);

        riverU->set(1);

        ptr<Graph::CurveIterator> ci = g->getCurves();
        while (ci->hasNext()) {
            CurvePtr c = ci->next();
            float cwidth = c->getWidth();
            if (cwidth * scale <= 1 || c->getType() != WaterElevationCurveData::RIVER || (c->getParent() == NULL && level != 0)) {
                continue;
            }

            float w = BASEWIDTH(cwidth, scale);
            float tw = TOTALWIDTH(w);

            ElevationCurveData *cData = dynamic_cast<ElevationCurveData*>(findCurveData(c));
            ElevationGraphLayer::drawCurveAltitude(tileOffset, c, cData, tw, tw / w, max(1.0f, 1.0f / scale), true, fb, layerProgram, *meshuv, &nx, &ny, &lx, &ly);
        }
        fb->setColorMask(true, true, true, true);
    }
    return true;
}

void WaterElevationLayer::swap(ptr<WaterElevationLayer> p)
{
    ElevationGraphLayer::swap(p);
    std::swap(mesh, p->mesh);
    std::swap(meshuv, p->meshuv);
    std::swap(tess, p->tess);
    std::swap(riverU, p->riverU);
    std::swap(tileOffsetU, p->tileOffsetU);
    std::swap(fillOffsetU, p->fillOffsetU);
    std::swap(colorU, p->colorU);
    std::swap(depthU, p->depthU);
}

class WaterElevationLayerResource : public ResourceTemplate<40, WaterElevationLayer>
{
public:
    WaterElevationLayerResource(ptr<ResourceManager> manager, const string &name, ptr<ResourceDescriptor> desc,
            const TiXmlElement *e = NULL) :
        ResourceTemplate<40, WaterElevationLayer> (manager, name, desc)
    {
        e = e == NULL ? desc->descriptor : e;
        ptr<GraphProducer> graphProducer;
        ptr<TileProducer> elevations;
        int displayLevel = 0;
        bool quality = true;
        bool deform = false;

        checkParameters(desc, e, "name,graph,renderProg,fillProg,level,cpuElevations,quality,deform,");

        string g = getParameter(desc, e, "graph");
        graphProducer = manager->loadResource(g).cast<GraphProducer>();

        string r = getParameter(desc, e, "cpuElevations");
        elevations = manager->loadResource(r).cast<TileProducer>();

        if (e->Attribute("level") != NULL) {
            getIntParameter(desc, e, "level", &displayLevel);
        }
        if (e->Attribute("quality") != NULL) {
            quality = strcmp(e->Attribute("quality"), "true") == 0;
        }
        if (e->Attribute("deform") != NULL) {
            deform = strcmp(e->Attribute("deform"), "true") == 0;
        }

        ptr<Program> layerProgram = manager->loadResource(getParameter(desc, e, "renderProg")).cast<Program>();
        ptr<Program> fillProg = manager->loadResource(getParameter(desc, e, "fillProg")).cast<Program>();
        init(graphProducer, layerProgram, fillProg, elevations, displayLevel, quality, deform);
    }

    virtual bool prepareUpdate()
    {
        bool changed = false;

        if (dynamic_cast<Resource*>(layerProgram.get())->changed() || dynamic_cast<Resource*>(fillProg.get())->changed()) {
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

extern const char waterElevationLayer[] = "waterElevationLayer";

static ResourceFactory::Type<waterElevationLayer, WaterElevationLayerResource> WaterElevationLayerType;

}
