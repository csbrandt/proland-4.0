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

#include "proland/ortho/RoadOrthoLayer.h"

#include "ork/resource/ResourceTemplate.h"
#include "ork/scenegraph/SceneManager.h"

#include "proland/math/color.h"
#include "proland/producer/ObjectTileStorage.h"
#include "proland/graph/producer/GetCurveDatasTask.h"
#include "proland/ortho/OrthoMargin.h"

using namespace ork;

namespace proland
{

RoadOrthoLayer::RoadOrthoLayer() :
    GraphLayer("RoadOrthoLayer"), CurveDataFactory()
{
}

RoadOrthoLayer::RoadOrthoLayer(ptr<GraphProducer> graphProducer, ptr<Program> layerProgram, int displayLevel, bool quality, vec4f color, vec4f dirt, vec4f border, float border_width, float inner_border_width, bool deform) :
    GraphLayer("RoadOrthoLayer")
{
    init(graphProducer, layerProgram, displayLevel, quality, color, dirt, border, border_width, inner_border_width, deform);
}

void RoadOrthoLayer::init(ptr<GraphProducer> graphProducer, ptr<Program> layerProgram, int displayLevel, bool quality, vec4f color, vec4f dirt, vec4f border, float border_width, float inner_border_width, bool deform)
{
    GraphLayer::init(graphProducer, layerProgram, displayLevel, quality, false, deform);
    CurveDataFactory::init(graphProducer);
    this->color = color;
    this->dirt = dirt;
    this->border = border;
    this->border_width = border_width;
    this->inner_border_width = inner_border_width;

    this->mesh = new Mesh<vec2f, unsigned int>(TRIANGLE_STRIP, CPU);
    this->meshuv = new Mesh<vec4f, unsigned int>(TRIANGLE_STRIP, CPU);
    this->mesh->addAttributeType(0, 2, A32F, false);   // pos
    this->meshuv->addAttributeType(0, 2, A32F, false); // pos
    this->meshuv->addAttributeType(1, 2, A32F, false); // uv

    tileOffsetU = layerProgram->getUniform3f("tileOffset");
    colorU = layerProgram->getUniform4f("color");
    stripeSizeU = layerProgram->getUniform3f("stripeSize");
    blendSizeU  = layerProgram->getUniform2f("blendSize");
}

RoadOrthoLayer::~RoadOrthoLayer()
{
}

void RoadOrthoLayer::setTileSize(int tileSize, int tileBorder, float rootQuadSize)
{
    GraphLayer::setTileSize(tileSize, tileBorder, rootQuadSize);
    float borderFactor = tileSize / (tileSize - 1.0f - 2.0f * tileBorder) - 1.0f;
    graphProducer->addMargin(new OrthoMargin(tileSize - 2 * tileBorder, borderFactor, border_width));
}

void RoadOrthoLayer::startCreateTile(int level, int tx, int ty, unsigned int deadline, ptr<Task> task, ptr<TaskGraph> result)
{
    Task *t = NULL;
    if (Logger::DEBUG_LOGGER != NULL) {
        ostringstream oss;
        oss << "START OrthoRoad tile " << getProducerId() << " " << level << " " << tx << " " << ty;
        Logger::DEBUG_LOGGER->log("ORTHO", oss.str());
        Logger::DEBUG_LOGGER->log("GRAPH", oss.str());
    }
    if (level >= displayLevel) {
        if (result != NULL) {
            t = new GetCurveDatasTask<GraphLayer>(task.get(), result.get(), this, graphProducer.get(), NULL, this, level, tx, ty, deadline); // This will prefectch the CurveData.
            result->addTask(t);
            result->addDependency(task, t);
        }
    }
    GraphLayer::startCreateTile(level, tx, ty, deadline, t == NULL ? task : t, result);
}

bool RoadOrthoLayer::doCreateTile(int level, int tx, int ty, TileStorage::Slot *data)
{
    if (Logger::DEBUG_LOGGER != NULL) {
        ostringstream oss;
        oss << "OrthoRoad tile " << getProducerId() << " " << level << " " << tx << " " << ty;
        Logger::DEBUG_LOGGER->log("ORTHO", oss.str());
        Logger::DEBUG_LOGGER->log("GRAPH", oss.str());
    }
    if (level >= displayLevel) {
        ptr<FrameBuffer> fb = SceneManager::getCurrentFrameBuffer();

        TileCache::Tile * t = graphProducer->findTile(level, tx, ty);
        assert(t != NULL);
        ObjectTileStorage::ObjectSlot *graphData = dynamic_cast<ObjectTileStorage::ObjectSlot*>(t->getData());
        GraphPtr g = graphData->data.cast<Graph>();

        vec3d q = getTileCoords(level, tx, ty);
        float scale = 2.0f * (1.0f - getTileBorder() * 2.0f / getTileSize()) / q.z;

        vec2d nx, ny, lx, ly;
        getDeformParameters(q, nx, ny, lx, ly);

        vec3d tileOffset = vec3d(q.x + q.z / 2.0f, q.y + q.z / 2.0f, scale);
        //tileOffsetU->set(vec3f(q.x + q.z / 2.0f, q.y + q.z / 2.0f, scale));
        tileOffsetU->set(vec3f(0.0, 0.0, 1.0));

        float scale2 = scale * getTileSize();
        stripeSizeU->set(vec3f::ZERO);

        ptr<Graph::CurveIterator> ci;

        if (quality) {
            if (border_width > 1.0f) {
                for (int pass = 0; pass < 2; ++pass) { // Drawing borders
                    if (pass == 0) {
                        fb->setBlend(true, ADD, ZERO, ONE, MIN, ONE, ONE);
                    } else {
                        fb->setBlend(true, ADD, ONE_MINUS_DST_ALPHA, DST_ALPHA, ADD, ONE, ZERO);
                    }
                    ptr<Graph::CurveIterator> ci = g->getCurves();
                    while (ci->hasNext()) {
                        CurvePtr p = ci->next();
                        if (p->getType() == ROAD) {
                            float pwidth = p->getWidth();
                            float swidth = pwidth * scale2;
                            if (swidth > 2 && pwidth > 1) {
                                float b1 = pwidth * inner_border_width / 2;
                                float b0 = pwidth * border_width / 2;
                                float a = 1.0f / (b1 - b0);
                                float b = -b0 * a;
                                if (pass == 0) {
                                    blendSizeU->set(vec2f(-a, 1.0f - b));
                                    colorU->set(border);
                                } else {
                                    blendSizeU->set(vec2f(0, 0));
                                    colorU->set(vec4f(border.xyz(), 1.0));
                                }
                                CurveData *data = findCurveData(p);
                                drawCurve(tileOffset, p, data, pwidth*border_width, pwidth, scale2, fb, layerProgram, *meshuv, &nx, &ny, &lx, &ly);
                            } else if (swidth > 0.1) {
                                float alpha = pass == 0 ? 1.0f - min(1.0f, swidth) : 1.0f;
                                blendSizeU->set(vec2f::ZERO);
                                if (pwidth == 1) {
                                    colorU->set(vec4f(dirt.xyz(), alpha));
                                    drawCurve(tileOffset, p, NULL, pwidth, 0, scale2, fb, layerProgram, *meshuv, &nx, &ny, &lx, &ly);
                                } else {
                                    colorU->set(vec4f(color.xyz(), alpha));
                                    drawCurve(tileOffset, p, NULL, pwidth*border_width, pwidth, scale2, fb, layerProgram, *meshuv, &nx, &ny, &lx, &ly);
                                }
                            }
                        }
                    }
                }
                fb->setBlend(false);
            }

            colorU->set(color);
            blendSizeU->set(vec2f::ZERO);
            ci = g->getCurves();
            while (ci->hasNext()) { // Drawing Roads
                CurvePtr p = ci->next();
                float pwidth = p->getWidth();
                if (p->getType() == ROAD && pwidth > 0) {
                    if (pwidth * scale2 <= 2) {
                        continue;
                    }
                    CurveData *d = findCurveData(p);
                    if (pwidth == 1) {
                        colorU->set(vec4f(dirt.xyz(), 0.0));
                    } else {
                        colorU->set(color);
                    }
                    drawCurve(tileOffset, p, d, pwidth, 0, scale2, fb, layerProgram, *meshuv, &nx, &ny, &lx, &ly);
                }
            }
            colorU->set(color);
            ci = g->getCurves();
            while (ci->hasNext()) { // Drawing road ends & stripes
                CurvePtr p = ci->next();
                float pwidth = p->getWidth();
                float swidth = pwidth * scale2;
                if (p->getType() == ROAD && pwidth > 1 && swidth > 2) {
                    CurvePtr parent = p->getAncestor();
                    CurveData *d = findCurveData(p);
                    float l0 = getLengthWithoutStripes(parent, d, true);
                    float l1 = getLengthWithoutStripes(parent, d, false);
                    vec2d a, na;
                    vec2d b, nb;
                    d->getCurvilinearCoordinate(l0, &a, &na);
                    d->getCurvilinearCoordinate(d->getCurvilinearLength() - l1, &b, &nb);
                    if (isDeformed()) {
                        na = na.normalize();
                        nb = nb.normalize();
                        float f0 = 1.0 / (lx * na.y - ly * na.x).length();
                        float f1 = 1.0 / (lx * nb.y - ly * nb.x).length();
                        a += vec2d(na.y, -na.x) * (l0 * f0 - l0);
                        b += vec2d(nb.y, -nb.x) * (l1 * f1 - l1);
                        l0 *= f0;
                        l1 *= f1;
                    }
                    stripeSizeU->set(vec3f(0.f, 0.f, -1.f));
                    drawCurve(tileOffset, p, d, l0, d->getCurvilinearLength() - l1, pwidth, scale2, fb, layerProgram, *meshuv, &nx, &ny, &lx, &ly);
                    if (l0 != 0 && swidth > 4) {
                        drawRoadEnd(tileOffset, fb, a, na, na.length(), pwidth / 2, scale2, &nx, &ny, &lx, &ly);
                    }
                    if (l1 != 0 && swidth > 4) {
                        drawRoadEnd(tileOffset, fb, b, nb, -nb.length(), pwidth / 2, scale2, &nx, &ny, &lx, &ly);
                    }
                }
            }
        } else {
            colorU->set(color);
            stripeSizeU->set(vec3f::ZERO);
            ci = g->getCurves();
            while (ci->hasNext()) {
                CurvePtr p = ci->next();
                float pwidth = p->getWidth();
                if (p->getType() == ROAD && pwidth > 0) {
                    drawCurve(tileOffset, p, pwidth, scale2, fb, layerProgram, *mesh, &nx, &ny, &lx, &ly);
                }
            }
        }
    }
    return true;
}

void RoadOrthoLayer::stopCreateTile(int level, int tx, int ty)
{
    CurveDataFactory::releaseCurveData(level, tx, ty);
    GraphLayer::stopCreateTile(level, tx, ty);
}

void RoadOrthoLayer::drawRoadEnd(const vec3d &tileOffset, ptr<FrameBuffer> fb, const vec2d &p, const vec2d &n, double length, float w, float scale, vec2d *nx, vec2d *ny, vec2d *lx, vec2d *ly)
{
    //TODO antialiasing
    //TODO passages pietons dans les zones de ville
    float h = 0.3f;
    float alpha = min(1.0f, h * scale);
    float cr = alpha + (1 - alpha) * color.x;
    float cg = alpha + (1 - alpha) * color.y;
    float cb = alpha + (1 - alpha) * color.z;
    colorU->set(vec4f(cr, cg, cb, 0.f));
    stripeSizeU->set(vec3f::ZERO);
    meshuv->setMode(TRIANGLE_STRIP);
    meshuv->clear();
    float dx, dy;
    if (isDeformed()) {
        double dx1 = nx->x * n.y + ny->x * -n.x;
        double dy1 = nx->y * n.y + ny->y * -n.x;
        float f1 = w / ((*lx) * dx1 + (*ly) * dy1).length();
        dx1 *= f1;
        dy1 *= f1;
        double dx2 = n.y;
        double dy2 = -n.x;
        float f2 = h / ((*lx) * dx2 + (*ly) * dy2).length();
        dx2 *= f2;
        dy2 *= f2;
        vec2d a = (vec2d(p.x, p.y) - tileOffset.xy()) * tileOffset.z;
        vec2d b = (vec2d(p.x + dx2, p.y + dy2) - tileOffset.xy()) * tileOffset.z;
        vec2d c = (vec2d(p.x + dx1, p.y + dy1) - tileOffset.xy()) * tileOffset.z;
        vec2d d = (vec2d(p.x + dx2 + dx1, p.y + dy2 + dy1) - tileOffset.xy()) * tileOffset.z;
        meshuv->addVertex(vec4f(a.x, a.y, 0, 0));
        meshuv->addVertex(vec4f(b.x, b.y, 0, 0));
        meshuv->addVertex(vec4f(c.x, c.y, 0, 0));
        meshuv->addVertex(vec4f(d.x, d.y, 0, 0));
    } else {
        vec2d n2 = n / length;
        vec2d a = (vec2d(p.x, p.y) - tileOffset.xy()) * tileOffset.z;
        vec2d b = (vec2d(p.x + n2.y * h, p.y - n2.x * h) - tileOffset.xy()) * tileOffset.z;
        vec2d c = (vec2d(p.x + n2.x * w, p.y + n2.y * w) - tileOffset.xy()) * tileOffset.z;
        vec2d d = (vec2d(p.x + n2.y * h + n2.x * w, p.y - n2.x * h + n2.y * w) - tileOffset.xy()) * tileOffset.z;
        meshuv->addVertex(vec4f(a.x, a.y, 0, 0));
        meshuv->addVertex(vec4f(b.x, b.y, 0, 0));
        meshuv->addVertex(vec4f(c.x, c.y, 0, 0));
        meshuv->addVertex(vec4f(d.x, d.y, 0, 0));
    }
    fb->draw(layerProgram, *meshuv);
    colorU->set(color);
}

float RoadOrthoLayer::getLengthWithoutStripes(CurvePtr p, CurveData *data, bool start)
{
    NodePtr pt = start ? p->getStart() : p->getEnd();
    if (pt->getCurveCount() == 1) {
        return 0.0f;
    }
    vec2d o = pt->getPos();
    vec2d prev = p->getXY(pt, 1);
    float maxWidth = 0;
    bool oppositeCurve = false;
    for (int i = 0; i < pt->getCurveCount(); ++i) {
        CurvePtr path = pt->getCurve(i);
        if (path != p && path->getType() != ROAD) {
            return 0.0f;
        }
        if (path != p && path->getType() == ROAD) {
            vec2d next = path->getXY(pt, 1);
            if (abs(angle(prev - o, next - o) - M_PI) < 0.01) {
                oppositeCurve = true;
            } else {
                maxWidth = max(maxWidth, path->getWidth());
            }
        }
    }
    float pwidth = p->getWidth();
    if (!oppositeCurve || pwidth < maxWidth || (pwidth == maxWidth && pt->getCurveCount() > 3)) {
        return start ? data->getStartCapLength() : data->getEndCapLength();
    }
    return 0.0f;
}

float RoadOrthoLayer::getFlatLength(NodePtr p, vec2d q, CurvePtr path)
{
    vec2d o = p->getPos();
    float flatLength = 0.0f;
    if (p->getCurveCount() > 1 ) {
        for (int i = 0; i < p->getCurveCount(); ++i) {
            CurvePtr ipath = p->getCurve(i);
            if (ipath != path) {
                vec2d r = ipath->getXY(p, 1);
                if (abs(angle(q - o, r - o) - M_PI) < 0.01) {
                    continue;
                }
                float pw = path->getType() == ROAD ? 2 * path->getWidth() : path->getWidth();
                float ipw = ipath->getType() == ROAD ? 2 * ipath->getWidth() : ipath->getWidth();
                vec2d corner = proland::corner(o, q, r, (double) pw, (double) ipw);
                float dot = (q - o).dot(corner - o);
                flatLength = max((double) flatLength, dot / (o - q).length());
            }
        }
    }
    return ceil(flatLength);
}

void RoadOrthoLayer::swap(ptr<RoadOrthoLayer> p)
{
    GraphLayer::swap(p);
    std::swap(color, p->color);
    std::swap(dirt, p->dirt);
    std::swap(border, p->border);
    std::swap(border_width, p->border_width);
    std::swap(inner_border_width, p->inner_border_width);
    std::swap(mesh, p->mesh);
    std::swap(meshuv, p->meshuv);
    std::swap(stripeSizeU, p->stripeSizeU);
    std::swap(colorU, p->colorU);
    std::swap(blendSizeU, p->blendSizeU);
    std::swap(tileOffsetU, p->tileOffsetU);
}

class RoadOrthoLayerResource : public ResourceTemplate<40, RoadOrthoLayer>
{
public:
    RoadOrthoLayerResource(ptr<ResourceManager> manager, const string &name, ptr<ResourceDescriptor> desc,
            const TiXmlElement *e = NULL) :
        ResourceTemplate<40, RoadOrthoLayer> (manager, name, desc)
    {
        e = e == NULL ? desc->descriptor : e;
        ptr<GraphProducer>graphProducer;
        int displayLevel = 0;
        vec4f color = vec4f((float)64/255,(float)64/255,(float)64/255,0);
        vec4f border = vec4f((float)43/255,(float)68/255,(float)20/255, 0);
        vec4f dirt = vec4f((float)154/512,(float)121/512,(float)7.0/512,0);

        float border_width = 2.0f;
        float inner_border_width = 1.2f;
        bool quality = true;
        bool deform = false;

        checkParameters(desc, e, "name,graph,renderProg,level,quality,color,dirt,border,borderWidth,innerBorderWidth,deform,");
        string g = getParameter(desc, e, "graph");

        graphProducer = manager->loadResource(g).cast<GraphProducer>();

        if (e->Attribute("level") != NULL) {
            getIntParameter(desc, e, "level", &displayLevel);
        }

        if (e->Attribute("quality") != NULL) {
            quality = strcmp(e->Attribute("quality"), "true") == 0;
        }

        if (e->Attribute("color") != NULL) {
            string c = getParameter(desc, e, "color") + ",";
            string::size_type start = 0;
            string::size_type index;
            for (int i = 0; i < 3; i++) {
                index = c.find(',', start);
                color[i] = (float) atof(c.substr(start, index - start).c_str()) / 255;
                start = index + 1;
            }
        }

        if (e->Attribute("border") != NULL) {
            string c = getParameter(desc, e, "border") + ",";
            string::size_type start = 0;
            string::size_type index;
            for (int i = 0; i < 3; i++) {
                index = c.find(',', start);
                border[i] = (float) atof(c.substr(start, index - start).c_str()) / 255;
                start = index + 1;
            }
        }

        if (e->Attribute("dirt") != NULL) {
            string c = getParameter(desc, e, "dirt") + ",";
            string::size_type start = 0;
            string::size_type index;
            for (int i = 0; i < 3; i++) {
                index = c.find(',', start);
                dirt[i] = (float) atof(c.substr(start, index - start).c_str()) / 255;
                start = index + 1;
            }
        }

        if (e->Attribute("borderWidth") != NULL) {
            getFloatParameter(desc, e, "borderWidth", &border_width);
        }

        if (e->Attribute("innerBorderWidth") != NULL) {
            getFloatParameter(desc, e, "innerBorderWidth", &inner_border_width);
        }

        if (e->Attribute("deform") != NULL) {
            deform = strcmp(e->Attribute("deform"), "true") == 0;
        }
        ptr<Program> layerProgram = manager->loadResource(getParameter(desc, e, "renderProg")).cast<Program>();
        init(graphProducer, layerProgram, displayLevel, quality, color, dirt, border, border_width, inner_border_width, deform);
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

extern const char roadOrthoLayer[] = "roadOrthoLayer";

static ResourceFactory::Type<roadOrthoLayer, RoadOrthoLayerResource> RoadOrthoLayerType;

}
