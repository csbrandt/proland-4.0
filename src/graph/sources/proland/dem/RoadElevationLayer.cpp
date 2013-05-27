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

#include "proland/dem/RoadElevationLayer.h"

#include "ork/resource/ResourceTemplate.h"
#include "ork/scenegraph/SceneManager.h"
#include "ork/scenegraph/SetTargetTask.h"

#include "proland/graph/Area.h"
#include "proland/producer/ObjectTileStorage.h"

using namespace ork;

namespace proland
{

#define BASEWIDTH(width, scale) ((width) + 2.0f * sqrt(2.0f)/(scale))
#define TOTALWIDTH(basewidth) ((basewidth) * 3.0f)

RoadElevationLayer::RoadElevationCurveData::RoadElevationCurveData(CurveId id, CurvePtr flattenCurve, ptr<TileProducer> elevations) :
    ElevationCurveData(id, flattenCurve, elevations, false)
{
    startBridge = false;
    endBridge = false;
    startBridgez = 0.0f;
    endBridgez = 0.0f;
    initBridges = false;
}

RoadElevationLayer::RoadElevationCurveData::~RoadElevationCurveData()
{
}

void RoadElevationLayer::RoadElevationCurveData::getBridgesz()
{
    if (flattenCurve->getStart()->getCurveCount() > 1) {
        NodePtr p = flattenCurve->getStart();
        startCapLength = getCapLength(p, flattenCurve->getXY(p, 1));
        if (flattenCurve->getType() == ROAD) { // normal road
            startBridge = true;
            NodePtr q = NULL;
            NodePtr r = NULL;
            for (int i = 0; i < p->getCurveCount(); ++i) {
                CurvePtr c = p->getCurve(i);
                if ((c->getAncestor()->getId() == id) == false) {
                    if (c->getType() != BRIDGE) {
                        startBridge = false;
                    } else {
                        q = c->getOpposite(p);
                        if (q->getCurveCount() == 4) {
                            for(int j=0; j<4; j++) {
                                CurvePtr cc = q->getCurve(j);
                                cc = cc->getAncestor();
                                if(cc->getType() == BRIDGE && cc->getWidth() > 0 && cc != c) {
                                    r = cc->getOpposite(q);
                                }
                            }
                        }
                    }
                }
            }
            if (q != NULL && startBridge) {
                vec2d a = p->getPos();
                vec2d b = q->getPos();
                //flatLength0 = path->width;
                startCapLength = (b - a).length() / 2;
                if (r != NULL) {
                    vec2d c = r->getPos();
                    startBridgez = max(getSample(a), max(getSample(b)+3, getSample(c)));
                } else {
                    startBridgez = max(getSample(a), getSample(b));
                }
            }
        }
    } else {
        startCapLength = 0;
    }
    if (flattenCurve->getEnd()->getCurveCount() > 1) {
        NodePtr p = flattenCurve->getEnd();
        endCapLength = getCapLength(p, flattenCurve->getXY(p, 1));
        if (flattenCurve->getType() == ROAD) { // normal road
            endBridge = true;
            NodePtr q = NULL;
            NodePtr r = NULL;
            for (int i = 0; i < p->getCurveCount(); ++i) {
                CurvePtr c = p->getCurve(i);
                if ((c->getAncestor()->getId() == id) == false) {
                    if (c->getType() != BRIDGE) {
                        endBridge = false;
                    } else {
                        q = c->getOpposite(p);
                        if (q->getCurveCount() == 4) {
                            for(int j=0; j<4; j++) {
                                CurvePtr cc = q->getCurve(j);
                                cc = cc->getAncestor();
                                if(cc->getType() == BRIDGE && cc->getWidth() > 0 && cc != c) {
                                    r = cc->getOpposite(q);
                                }
                            }
                        }
                    }
                }
            }
            if (q != NULL && endBridge) {
                vec2d a = p->getPos();
                vec2d b = q->getPos();
                endCapLength = (b - a).length() / 2;
                if (r != NULL) {
                    vec2d c = r->getPos();
                    endBridgez = max(getSample(a), max(getSample(b)+3, getSample(c)));
                } else {
                    endBridgez = max(getSample(a), getSample(b));
                }
            }
        }
    } else {
        endCapLength = 0;
    }
    initBridges = true;
}

float RoadElevationLayer::RoadElevationCurveData::getStartHeight()
{
    if (!initBridges) {
        getBridgesz();
    }
    return startBridge ? startBridgez + 1 : getSample(0);
}

float RoadElevationLayer::RoadElevationCurveData::getEndHeight()
{
    if (!initBridges) {
        getBridgesz();
    }
    return endBridge ? endBridgez + 1 : getSample(sampleCount - 1);
}

float RoadElevationLayer::RoadElevationCurveData::getAltitude(float s)
{
    float L = getCurvilinearLength();
    float l = flattenCurve->getCurvilinearLength(s, NULL, NULL);

    if (l < startCapLength) {
        return getStartHeight();
    }
    if (l > L - endCapLength) {
        return getEndHeight();
    }

    if (flattenCurve->getType() == BRIDGE) {
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
    float flat0 = startCapLength == 0.0f ? 0.0f : startCapLength + (startBridge ? 2*x : x);
    float flat1 = endCapLength == 0.0f ? 0.0f : endCapLength + (endBridge ? 2*x : x);

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

float RoadElevationLayer::RoadElevationCurveData::getCapLength(NodePtr p, vec2d q)
{
    vec2d o = p->getPos();
    float capLength = 0;
    for (int i = 0; i < p->getCurveCount(); ++i) {
        CurvePtr ipath = p->getCurve(i);
        if ((ipath->getAncestor()->getId() == id) == false) {
            vec2d r = ipath->getXY(p, 1);
            if (abs(angle(q - o, r - o) - M_PI) < 0.01) {
                continue;
            }
            float pw = flattenCurve->getType() == ROAD ? 2 * flattenCurve->getWidth() : flattenCurve->getWidth();
            float ipw = ipath->getType() == ROAD ? 2 * ipath->getWidth() : ipath->getWidth();
            vec2d corner = proland::corner(o, q, r, (double) pw, (double) ipw);
            float dot = (q - o).dot(corner - o);
            capLength = max((double) capLength, dot / (o - q).length());
        }
    }
    return ceil(capLength);
}

void RoadElevationLayer::RoadElevationCurveData::getUsedTiles(set<TileCache::Tile::Id> &tiles, float rootSampleLength)
{
    if ((int) usedTiles.size() == 0) {
        ElevationCurveData::getUsedTiles(tiles, rootSampleLength);
        int level = 0;
        float sampleLength = getSampleLength(flattenCurve);
        float rootQuadSize = elevations->getRootQuadSize();
        float l = rootQuadSize / (elevations->getCache()->getStorage()->getTileSize() - elevations->getBorder() * 2 - 1.0f);
        while (l > sampleLength) {
            l = l/2;
            level += 1;
        }
        int nTiles = 1 << level;
        float levelTileSize = rootQuadSize / nTiles;

        int tx;
        int ty;

        if (flattenCurve->getType() == ROAD) {
            for (int i = 0; i < 2; ++i) {
                NodePtr q = NULL;
                NodePtr r = NULL;
                bool b = true;
                int count = 0;
                NodePtr p = i == 0 ? flattenCurve->getStart() : flattenCurve->getEnd();
                for (int j = 0; j < p->getCurveCount(); j++) {
                    CurvePtr c = p->getCurve(j);
                    if ((c->getAncestor()->getId() == id) == false) {
                        if (c->getType() != BRIDGE) {
                            b = false;
                            count++;
                        } else {
                            count++;
                            q = c->getOpposite(p);
                            if (q->getCurveCount() == 4) {
                                for(int k=0; k<4; k++) {
                                    CurvePtr cc = q->getCurve(k);
                                    cc = cc->getAncestor();
                                    if(cc->getType() == BRIDGE && cc->getWidth() > 0 && cc != c) {
                                        r = cc->getOpposite(q);
                                    }
                                }
                            }
                        }
                    }
                }
                if (q != NULL && b) {
                    if (r != NULL) {
                        tx = (int) floor((r->getPos().x + rootQuadSize / 2.0f) / levelTileSize);
                        ty = (int) floor((r->getPos().y + rootQuadSize / 2.0f) / levelTileSize);
                        if (tx < nTiles && tx >= 0 && ty < nTiles && ty >= 0) {
                            usedTiles.insert(make_pair(level, make_pair(tx, ty)));
                        }
                    }
                    tx = (int) floor((p->getPos().x + rootQuadSize / 2.0f) / levelTileSize);
                    ty = (int) floor((p->getPos().y + rootQuadSize / 2.0f) / levelTileSize);
                    if (tx < nTiles && tx >= 0 && ty < nTiles && ty >= 0) {
                        usedTiles.insert(make_pair(level, make_pair(tx, ty)));
                    }
                    tx = (int) floor((q->getPos().x + rootQuadSize / 2.0f) / levelTileSize);
                    ty = (int) floor((q->getPos().y + rootQuadSize / 2.0f) / levelTileSize);
                    if (tx < nTiles && tx >= 0 && ty < nTiles && ty >= 0) {
                        usedTiles.insert(make_pair(level, make_pair(tx, ty)));
                    }
                }
                if (i == 0) {
                    startBridge = b;
                } else {
                    endBridge = b;
                }
            }
        }
    }
    tiles.insert(usedTiles.begin(), usedTiles.end());
}

RoadElevationLayer::RoadElevationMargin::RoadElevationMargin(int samplesPerTile, float borderFactor) :
    ElevationMargin(samplesPerTile, borderFactor)
{
}

RoadElevationLayer::RoadElevationMargin::~RoadElevationMargin()
{
}

double RoadElevationLayer::RoadElevationMargin::getMargin(double clipSize, CurvePtr p)
{
    double pwidth = p->getWidth();
    if (p->getType() == RoadElevationCurveData::ROAD) {
        double scale = 2.0f * (samplesPerTile - 1) / clipSize;
        if (p->getParent() != NULL && pwidth * scale >= 1) {
            return TOTALWIDTH(BASEWIDTH(pwidth, scale));
        } else {
            return 0;
        }
    } else {
        return pwidth / 2;
    }
}

RoadElevationLayer::RoadElevationLayer() : ElevationGraphLayer("RoadElevationLayer")
{
}

RoadElevationLayer::RoadElevationLayer(ptr<GraphProducer> graphProducer, ptr<Program> layerProgram, ptr<TileProducer> elevations, int displayLevel, bool quality, bool deform) :
    ElevationGraphLayer("RoadElevationLayer")
{
    init(graphProducer, layerProgram, elevations, displayLevel, quality, deform);
}

void RoadElevationLayer::init(ptr<GraphProducer> graphProducer, ptr<Program> layerProgram, ptr<TileProducer> elevations, int displayLevel, bool quality, bool deform)
{
    ElevationGraphLayer::init(graphProducer, layerProgram, elevations, displayLevel, quality, false, deform);
    this->meshuv = new Mesh<vec4f, unsigned int>(TRIANGLE_STRIP, GPU_STREAM);
    this->meshuv->addAttributeType(0, 2, A32F, false); //pos
    this->meshuv->addAttributeType(1, 2, A32F, false); //uv

    this->tileOffsetU = layerProgram->getUniform3f("tileOffset");
}

RoadElevationLayer::~RoadElevationLayer()
{
}

void RoadElevationLayer::setTileSize(int tileSize, int tileBorder, float rootQuadSize)
{
    GraphLayer::setTileSize(tileSize, tileBorder, rootQuadSize);
    float borderFactor = tileSize / (tileSize - 1.0f - 2.0f * tileBorder) - 1.0f;
    graphProducer->addMargin(new RoadElevationMargin(tileSize - 2 * tileBorder, borderFactor));
    elevations->setRootQuadSize(rootQuadSize);
}

CurveData *RoadElevationLayer::newCurveData(CurveId id, CurvePtr flattenCurve)
{
    return new RoadElevationCurveData(id, flattenCurve, elevations);
}

bool RoadElevationLayer::doCreateTile(int level, int tx, int ty, TileStorage::Slot *data)
{
    if (Logger::DEBUG_LOGGER != NULL) {
        ostringstream oss;
        oss << "ElevationRoad tile " << getProducerId() << " " << level << " " << tx << " " << ty;
        Logger::DEBUG_LOGGER->log("DEM", oss.str());
    }

    if (level >= displayLevel) {
        ptr<FrameBuffer> fb = SceneManager::getCurrentFrameBuffer();

        TileCache::Tile *t = graphProducer->findTile(level, tx, ty);
        assert(t != NULL);

        ObjectTileStorage::ObjectSlot *graphData = dynamic_cast<ObjectTileStorage::ObjectSlot*>(t->getData());
        GraphPtr g = graphData->data.cast<Graph>();

        if (g->getCurveCount() == 0) {
            return false;
        }

        vec3d q = getTileCoords(level, tx, ty);
        vec2d nx, ny, lx, ly;
        getDeformParameters(q, nx, ny, lx, ly);

        float scale = 2.0f * (getTileSize() - 1.0f - (2.0f * getTileBorder())) / q.z;
        vec3d tileOffset = vec3d(q.x + q.z / 2.0f, q.y + q.z / 2.0f, scale / getTileSize());
        //tileOffsetU->set(vec3f(q.x + q.z / 2.0f, q.y + q.z / 2.0f, scale / getTileSize()));
        tileOffsetU->set(vec3f(0.0, 0.0, 1.0));

        fb->setColorMask(false, false, true, true);
        fb->clear(false, false, true);

        ptr<Graph::CurveIterator> ci = g->getCurves();
        while (ci->hasNext()) {
            CurvePtr c = ci->next();
            float cwidth = c->getWidth();
            if (cwidth * scale <= 1 || c->getType() != RoadElevationCurveData::ROAD || (c->getParent() == NULL && level != 0)) {
                continue;
            }

            float w = BASEWIDTH(cwidth, scale);
            float tw = TOTALWIDTH(w);

            ElevationCurveData *cData = dynamic_cast<ElevationCurveData*>(findCurveData(c));
            ElevationGraphLayer::drawCurveAltitude(tileOffset, c, cData, tw, tw / w, max(1.0f, (1.0f / scale)), true, fb, layerProgram, *meshuv, &nx, &ny, &lx, &ly);
        }
        fb->setColorMask(true, true, true, true);

    }
    return true;
}

void RoadElevationLayer::swap(ptr<RoadElevationLayer> p)
{
    ElevationGraphLayer::swap(p);
    std::swap(meshuv, p->meshuv);
    std::swap(tileOffsetU, p->tileOffsetU);
}

class RoadElevationLayerResource : public ResourceTemplate<40, RoadElevationLayer>
{
public:
    RoadElevationLayerResource(ptr<ResourceManager> manager, const string &name, ptr<ResourceDescriptor> desc, const TiXmlElement *e = NULL) :
        ResourceTemplate<40, RoadElevationLayer> (manager, name, desc)
    {
        e = e == NULL ? desc->descriptor : e;
        ptr<GraphProducer> graphProducer;
        ptr<TileProducer> elevations;
        int displayLevel = 0;
        bool quality = true;
        bool deform = false;

        checkParameters(desc, e, "name,graph,renderProg,level,cpuElevations,quality,deform,");

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
        init(graphProducer, layerProgram, elevations, displayLevel, quality, deform);
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

extern const char roadElevationLayer[] = "roadElevationLayer";

static ResourceFactory::Type<roadElevationLayer, RoadElevationLayerResource> RoadElevationLayerType;

}
