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

#include "proland/ortho/LineOrthoLayer.h"

#include "ork/resource/ResourceTemplate.h"
#include "ork/scenegraph/SceneManager.h"

#include "proland/producer/ObjectTileStorage.h"

using namespace ork;

namespace proland
{

LineOrthoLayer::LineOrthoLayer() : GraphLayer("LineOrthoLayer")
{
}

LineOrthoLayer::LineOrthoLayer(ptr<GraphProducer> graphProducer, ptr<Program> layerProgram, int displayLevel) :
    GraphLayer("LineOrthoLayer")
{
    init(graphProducer, layerProgram, displayLevel);
}

void LineOrthoLayer::init(ptr<GraphProducer> graphProducer, ptr<Program> layerProgram, int displayLevel)
{
    GraphLayer::init(graphProducer, layerProgram, displayLevel);
    mesh = new Mesh<vec2f, unsigned int>(LINE_STRIP, GPU_STREAM);
    mesh->addAttributeType(0, 2, A32F, false);

    tileOffsetU = layerProgram->getUniform3f("tileOffset");
}

LineOrthoLayer::~LineOrthoLayer()
{
}

bool LineOrthoLayer::doCreateTile(int level, int tx, int ty, TileStorage::Slot *data)
{
    if (Logger::DEBUG_LOGGER != NULL) {
        ostringstream oss;
        oss << "LineOrtho tile " << getProducerId() << " " << level << " " << tx << " " << ty;
        Logger::DEBUG_LOGGER->log("ORTHO", oss.str());
    }
    if (level >= displayLevel) {
        TileCache::Tile* t = graphProducer->findTile(level, tx, ty);
        assert(t != NULL);

        ObjectTileStorage::ObjectSlot* graphData = static_cast<ObjectTileStorage::ObjectSlot*>(t->getData());
        GraphPtr graphForThisTile = graphData->data.cast<Graph>();

        vec3d tileCoords = getTileCoords(level, tx, ty);
        float scale = 2.0f * (1.0f - getTileBorder() * 2.0f / getTileSize()) / tileCoords.z;

        ptr<FrameBuffer> fb = SceneManager::getCurrentFrameBuffer();
        tileOffsetU->set(vec3f(tileCoords.x + tileCoords.z / 2.0f, tileCoords.y + tileCoords.z / 2.0f, scale));

        ptr<Graph::CurveIterator> curveIterator = graphForThisTile->getCurves();
        while (curveIterator->hasNext()) {
            CurvePtr curve = curveIterator->next();
            int numberOfPoints = curve->getSize();
            assert(numberOfPoints >= 2);
            mesh->clear();
            for (int i = 0; i < numberOfPoints; ++i) {
                vec2f vertexPosition = curve->getXY(i).cast<float>();
                mesh->addVertex(vertexPosition);
            }
            fb->draw(layerProgram, *mesh);
        }
    }
    return true;
}

void LineOrthoLayer::swap(ptr<LineOrthoLayer> p)
{
    GraphLayer::swap(p);
    std::swap(mesh, p->mesh);
    std::swap(tileOffsetU, p->tileOffsetU);
}

class LineOrthoLayerResource : public ResourceTemplate<40, LineOrthoLayer>
{
public:
    LineOrthoLayerResource(ptr<ResourceManager> manager, const string &name, ptr<ResourceDescriptor> desc,
            const TiXmlElement *e = NULL) :
        ResourceTemplate<40, LineOrthoLayer> (manager, name, desc)
    {
        e = e == NULL ? desc->descriptor : e;
        ptr<GraphProducer>graphProducer;
        int displayLevel = 0;

        checkParameters(desc, e, "name,graph,renderProg,level,");
        string g = getParameter(desc, e, "graph");

        graphProducer = manager->loadResource(g).cast<GraphProducer>();

        if (e->Attribute("level") != NULL) {
            getIntParameter(desc, e, "level", &displayLevel);
        }
        assert(displayLevel >= 0);

        ptr<Program> layerProgram = manager->loadResource(getParameter(desc, e, "renderProg")).cast<Program>();
        init(graphProducer, layerProgram, displayLevel);
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

extern const char lineOrthoLayer[] = "lineOrthoLayer";

static ResourceFactory::Type<lineOrthoLayer, LineOrthoLayerResource> LineOrthoLayerType;

}
