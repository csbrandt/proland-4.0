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

#include "proland/ortho/FieldsOrthoLayer.h"

#include "ork/resource/ResourceTemplate.h"
#include "ork/scenegraph/SceneManager.h"

#include "proland/math/color.h"
#include "proland/graph/Area.h"
#include "proland/producer/ObjectTileStorage.h"
#include "proland/ortho/OrthoMargin.h"

using namespace ork;

namespace proland
{

vec4f FieldsOrthoLayer::COLOR[] = {
    vec4f(132, 124, 20, 0) / 255,
    vec4f(62, 102, 22, 0) / 255,
    vec4f(97, 102, 39, 0) / 255,
    vec4f(43, 67, 20, 0) / 255,
    vec4f(65, 85, 18, 0) / 255,
    vec4f(61, 81, 20, 0) / 255,
    vec4f(57, 81, 20, 0) / 255,
    vec4f(50, 75, 14, 0) / 255,
    vec4f(64, 51, 25, 0) / 255,
    vec4f(43, 68, 20, 0) / 255
};

mat3f FieldsOrthoLayer::DCOLOR[] = {
    dcolor(COLOR[0].xyz()),
    dcolor(COLOR[1].xyz()),
    dcolor(COLOR[2].xyz()),
    dcolor(COLOR[3].xyz()),
    dcolor(COLOR[4].xyz()),
    dcolor(COLOR[5].xyz()),
    dcolor(COLOR[6].xyz()),
    dcolor(COLOR[7].xyz()),
    dcolor(COLOR[8].xyz()),
    dcolor(COLOR[9].xyz())
};

vec3f FieldsOrthoLayer::STRIPES[] = {
    vec3f(0.4f, 12.0f, 1.0f),
    vec3f(0.4f, 10.0f, 1.0f),
    vec3f(0.4f, 14.0f, 1.0f),
    vec3f(0.4f, 12.0f, 1.0f),
    vec3f(0.4f, 8.0f, 1.0f),
    vec3f(0.4f, 6.0f, 1.0f),
    vec3f(0.0f, 0.0f, 0.0f),
    vec3f(0.0f, 0.0f, 0.0f),
    vec3f(0.1f, 3.0f, 1.0f),
    vec3f(0.0f, 0.0f, 0.0f)
};

FieldsOrthoLayer::FieldsOrthoLayer() :
    GraphLayer("FieldsOrthoLayer")
{
}

FieldsOrthoLayer::FieldsOrthoLayer(ptr<GraphProducer> graphProducer, ptr<Program> layerProgram, ptr<Program> fillProgram, int displayLevel, bool quality) :
    GraphLayer("FieldsOrthoLayer")
{
    init(graphProducer, layerProgram, fillProgram, displayLevel, quality);
}

void FieldsOrthoLayer::init(ptr<GraphProducer> graphProducer, ptr<Program> layerProgram, ptr<Program> fillProgram, int displayLevel, bool quality)
{
    GraphLayer::init(graphProducer, layerProgram, displayLevel, quality, false);
    this->mesh = new Mesh<vec2f, unsigned int>(TRIANGLE_STRIP, GPU_STREAM);
    this->mesh->addAttributeType(0, 2, A32F, false);
    this->tess = new Tesselator();
    this->fill = fillProgram;

    fillOffsetU = fillProgram->getUniform3f("tileOffset");
    depthU = fillProgram->getUniform1f("depth");
    fillColorU = fillProgram->getUniform4f("color");


    stripeSizeU = layerProgram->getUniform3f("stripeSize");
    stripeDirU = layerProgram->getUniform4f("stripeDir");
    colorU = layerProgram->getUniform4f("color");
    tileOffsetU = layerProgram->getUniform3f("tileOffset");
}

FieldsOrthoLayer::~FieldsOrthoLayer()
{
}

void FieldsOrthoLayer::setTileSize(int tileSize, int tileBorder, float rootQuadSize)
{
    GraphLayer::setTileSize(tileSize, tileBorder, rootQuadSize);
    float borderFactor = tileSize / (tileSize - 1.0f - 2.0f * tileBorder) - 1.0f;
    graphProducer->addMargin(new OrthoMargin(tileSize - 2 * tileBorder, borderFactor, 1.0f));
}

bool FieldsOrthoLayer::doCreateTile(int level, int tx, int ty, TileStorage::Slot *data)
{
    if (Logger::DEBUG_LOGGER != NULL) {
        ostringstream oss;
        oss << "OrthoFields tile " << getProducerId() << " " << level << " " << tx << " " << ty;
        Logger::DEBUG_LOGGER->log("ORTHO", oss.str());
    }
    if (level >= displayLevel) {
        TileCache::Tile * t = graphProducer->findTile(level, tx, ty);
        assert(t != NULL);
        ObjectTileStorage::ObjectSlot *graphData = dynamic_cast<ObjectTileStorage::ObjectSlot*>(t->getData());
        GraphPtr g = graphData->data.cast<Graph>();

        if (g != NULL) {
            ptr<FrameBuffer> fb = SceneManager::getCurrentFrameBuffer();

            vec3d q = getTileCoords(level, tx, ty);

            bool init = false;

            float scale = 2.0f * (1.0f - 2.0f * getTileBorder() / getTileSize()) / q.z;
            fillOffsetU->set(vec3f(q.x + q.z / 2, q.y + q.z / 2, scale));
            vec3d tileOffset = vec3d(q.x + q.z / 2, q.y + q.z / 2, scale);
            //tileOffsetU->set(vec3f(q.x + q.z / 2, q.y + q.z / 2, scale));
            tileOffsetU->set(vec3f(0.0, 0.0, 1.0));

            ptr<Graph::AreaIterator> ai = g->getAreas();
            while (ai->hasNext()) {
                AreaPtr a = ai->next();
                GraphPtr sg = a->getSubgraph();
                if (sg == NULL || sg->getCurveCount() == 0) {
                    continue;
                }
                init = true;
                fb->setColorMask(false, false, false, true);
                fb->setDepthMask(false);

                fillColorU->set(vec4f::ZERO);

                float b = q.z * scale;
                mesh->setMode(TRIANGLE_STRIP);
                mesh->clear();
                mesh->addVertex(((vec2d(q.x - b, q.y - b) - tileOffset.xy()) * tileOffset.z).cast<float>());
                mesh->addVertex(((vec2d(q.x + q.z + b, q.y - b) - tileOffset.xy()) * tileOffset.z).cast<float>());
                mesh->addVertex(((vec2d(q.x - b, q.y + q.z + b) - tileOffset.xy()) * tileOffset.z).cast<float>());
                mesh->addVertex(((vec2d(q.x + q.z + b, q.y + q.z + b) - tileOffset.xy()) * tileOffset.z).cast<float>());
                fb->draw(fill, *mesh);
                fillColorU->set(vec4f(0.0f, 0.0f, 0.0f, 1.0f));

                mesh->setMode(TRIANGLES);
                mesh->clear();
                tess->beginPolygon(mesh);
                GraphLayer::drawArea(tileOffset, a, *tess);
                tess->endPolygon();
                fb->draw(fill, *mesh);


                fb->setColorMask(true, true, true, true);
                fb->setDepthMask(true);
                fb->setBlend(true,  ADD, DST_ALPHA, ONE_MINUS_DST_ALPHA,
                                    MIN, ONE, ONE);

                colorU->set(vec4f(COLOR[9].x, COLOR[9].y, COLOR[9].z, COLOR[9].w));

                stripeSizeU->set(vec3f(STRIPES[9].x, STRIPES[9].y, STRIPES[9].z));
                stripeDirU->set(vec4f(1.0f, 0.0f, 0.0f, 0.0f));

                ptr<Graph::CurveIterator> ci = sg->getCurves();
                while (ci->hasNext()) {
                    CurvePtr p = ci->next();
                    float pwidth = p->getWidth();
                    float swidth = pwidth * scale;
                    if (swidth > 0.1 && p->getArea2() != NULL) {
                        float alpha = min(1.0f, swidth);
                        colorU->set(vec4f(COLOR[9].x * alpha, COLOR[9].y * alpha, COLOR[9].z * alpha, 1.0f - alpha));
                        GraphLayer::drawCurve(tileOffset, p, pwidth, scale, fb, layerProgram, *mesh);
                    }
                }

               ptr<Graph::AreaIterator> aj = sg->getAreas();
                while (aj->hasNext()) {
                    AreaPtr sa = aj->next();
                    mat3f *dcolor;
                    vec3f *stripeSize;
                    vec2f stripeDir;
                    vec4f *color = getColor(sa, &dcolor, &stripeSize, stripeDir);
                    if (stripeDir.x == 0 && stripeDir.y == 0) {
                        continue;
                    }
                    colorU->set(vec4f(color->x, color->y, color->z, color->w));
                    stripeSizeU->set(vec3f(stripeSize->x, stripeSize->y, stripeSize->z));
                    stripeDirU->set(vec4f(stripeDir.x, stripeDir.y, 0.f, 0.f));
                    mesh->setMode(TRIANGLES);
                    mesh->clear();
                    tess->beginPolygon(mesh);
                    GraphLayer::drawArea(tileOffset, sa, *tess);
                    tess->endPolygon();
                    fb->draw(layerProgram, *mesh);
                }

                fb->setBlend(false);
            }

            if (init) {
                fb->setColorMask(false, false, false, true);
                fb->setDepthMask(true);

                fillColorU->set(vec4f::ZERO);
                mesh->setMode(TRIANGLE_STRIP);
                mesh->clear();
                mesh->addVertex(((vec2d(q.x, q.y) - tileOffset.xy()) * tileOffset.z).cast<float>());
                mesh->addVertex(((vec2d(q.x + q.z, q.y) - tileOffset.xy()) * tileOffset.z).cast<float>());
                mesh->addVertex(((vec2d(q.x, q.y + q.z) - tileOffset.xy()) * tileOffset.z).cast<float>());
                mesh->addVertex(((vec2d(q.x + q.z, q.y + q.z) - tileOffset.xy()) * tileOffset.z).cast<float>());
                fb->draw(fill, *mesh);
                fb->setColorMask(true, true, true, true);

                stripeSizeU->set(vec3f::ZERO);
                stripeDirU->set(vec4f(1.0f, 0.0f, 0.f, 0.f));
            }
        }
    }
    return true;
}

vec4f* FieldsOrthoLayer::getColor(AreaPtr field, mat3f **dcolor, vec3f **stripeSize, vec2f &stripeDir)
{
    int t = abs(field->getInfo()) % 9;

    float maxl = 0;
    CurvePtr pmax = NULL;
    AreaPtr a = field->getAncestor();
    for (int i = 0; i < a->getCurveCount(); ++i) {
        int orientation;
        CurvePtr p = a->getCurve(i, orientation);
        float squarel = 0;
        for (int j = 1; j < p->getSize(); ++j) {
            squarel += (p->getXY(j - 1) - p->getXY(j)).squaredLength();
        }
        if (squarel > maxl) {
            maxl = squarel;
            pmax = p;
        }
    }
    assert(pmax != NULL);
    vec2d dir = pmax->getEnd()->getPos() - pmax->getStart()->getPos();
    float l = dir.length();
    stripeDir = (l == 0 ? dir : dir / l).cast<float>();
    if (abs(field->getInfo()) % 27 < 20) {
        stripeDir = vec2f(-stripeDir.y, stripeDir.x);
    }

    *dcolor = &DCOLOR[t];
    *stripeSize = &STRIPES[t];
    return &COLOR[t];
}

void FieldsOrthoLayer::swap(ptr<FieldsOrthoLayer> p)
{
    GraphLayer::swap(p);
    std::swap(fill, p->fill);
    std::swap(mesh, p->mesh);
    std::swap(tess, p->tess);
    std::swap(fillOffsetU, p->fillOffsetU);
    std::swap(depthU, p->depthU);
    std::swap(fillColorU, p->fillColorU);
    std::swap(stripeSizeU, p->stripeSizeU);
    std::swap(stripeDirU, p->stripeDirU);
    std::swap(colorU, p->colorU);
    std::swap(tileOffsetU, p->tileOffsetU);
}

class FieldsOrthoLayerResource : public ResourceTemplate<40, FieldsOrthoLayer>
{
public:
    FieldsOrthoLayerResource(ptr<ResourceManager> manager, const string &name, ptr<ResourceDescriptor> desc,
            const TiXmlElement *e = NULL) :
        ResourceTemplate<40, FieldsOrthoLayer> (manager, name, desc)
    {
        e = e == NULL ? desc->descriptor : e;
        ptr<GraphProducer>graphProducer;
        int displayLevel = 0;

        checkParameters(desc, e, "name,graph,renderProg,fillProg,level,quality,");
        string g = getParameter(desc, e, "graph");

        graphProducer = manager->loadResource(g).cast<GraphProducer>();
        if (e->Attribute("level") != NULL) {
            getIntParameter(desc, e, "level", &displayLevel);
        }

        if (e->Attribute("quality") != NULL) {
            quality = strcmp(e->Attribute("quality"), "true") == 0;
        }

        ptr<Program> layerProgram = manager->loadResource(getParameter(desc, e, "renderProg")).cast<Program>();

        ptr<Program> fillProgram = manager->loadResource(getParameter(desc, e, "fillProg")).cast<Program>();

        init(graphProducer, layerProgram, fillProgram, displayLevel, quality);
    }

    virtual bool prepareUpdate()
    {
        bool changed = false;

        if (dynamic_cast<Resource*>(layerProgram.get())->changed() || dynamic_cast<Resource*>(fill.get())->changed()) {
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

extern const char fieldsOrthoLayer[] = "fieldsOrthoLayer";

static ResourceFactory::Type<fieldsOrthoLayer, FieldsOrthoLayerResource> FieldsOrthoLayerType;

}
