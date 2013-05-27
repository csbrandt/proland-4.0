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

#include "proland/rivers/HydroFlowProducer.h"

#include "ork/core/Timer.h"

#include "proland/producer/ObjectTileStorage.h"
#include "proland/rivers/graph/HydroGraph.h"
#include "proland/rivers/graph/LazyHydroGraph.h"
#include "proland/rivers/HydroFlowTile.h"
#include "proland/graph/producer/GetCurveDatasTask.h"

namespace proland
{

static Timer *swHYDRODATA;

HydroFlowProducer::RiverMargin::RiverMargin(int samplesPerTile, float borderFactor) :
     borderFactor(borderFactor), samplesPerTile(samplesPerTile)
{
}

double HydroFlowProducer::RiverMargin::getMargin(double clipSize)
{
    return (clipSize / 2) * borderFactor;
}

double HydroFlowProducer::RiverMargin::getMargin(double clipSize, CurvePtr p)
{
    if (p->getType() != HydroCurve::BANK) {
        float pwidth = p->getWidth();
        if (p->getType() == HydroCurve::AXIS) {
            float scale = 2.0f * (samplesPerTile - 1) / clipSize;
            if (pwidth * scale >= 1) {
                return TOTALWIDTH(BASEWIDTH(pwidth, scale));
            } else {
                return 0.0f;
            }
        } else {
            return pwidth / 2.0f;
        }
    }
    if (p.cast<HydroCurve>()->getRiver().id == NULL_ID) {
        return 0.f;
    }
    CurvePtr ancestor = p.cast<HydroCurve>()->getRiverPtr();
    assert(ancestor != NULL);
    return getMargin(clipSize, ancestor) * 2.0f;
}

HydroFlowProducer::HydroFlowProducer() : TileProducer("HydroFlowProducer", "CreateHydroData")
{
}

HydroFlowProducer::HydroFlowProducer(ptr<GraphProducer> graphs, ptr<TileCache> cache, int displayTileSize, float slipParameter, float searchRadiusFactor, float potentialDelta, int minLevel) : TileProducer("HydroFlowProducer", "CreateHydroData")
{
    init(graphs, cache, displayTileSize, slipParameter, searchRadiusFactor, potentialDelta, minLevel);
}

void HydroFlowProducer::init(ptr<GraphProducer> graphs, ptr<TileCache> cache, int displayTileSize, float slipParameter, float searchRadiusFactor, float potentialDelta, int minLevel)
{
    TileProducer::init(cache, false);
    CurveDataFactory::init(graphs);
    this->displayTileSize = displayTileSize;
    this->slipParameter = slipParameter;
    this->graphs = graphs;
    this->searchRadiusFactor = searchRadiusFactor;
    this->potentialDelta = potentialDelta;
    this->minLevel = minLevel;

    float borderFactor = displayTileSize / (displayTileSize - 1.0f - 2.0f * getBorder()) - 1.0f;
    this->graphs->addMargin(new RiverMargin(displayTileSize - 2 * getBorder(), borderFactor));
    swHYDRODATA = new Timer();
}

HydroFlowProducer::~HydroFlowProducer()
{
    printf("TEMPS CREATION TILE : %f : %f\n", swHYDRODATA->getAvgTime(), 1.0f / swHYDRODATA->getAvgTime());
    delete swHYDRODATA;
}

ptr<GraphProducer> HydroFlowProducer::getGraphProducer()
{
    return graphs;
}

int HydroFlowProducer::getTileSize()
{
    return displayTileSize;
}

float HydroFlowProducer::getSlipParameter()
{
    return slipParameter;
}

void HydroFlowProducer::setSlipParameter(float slip)
{
    this->slipParameter = slip;
    invalidateTiles();
}

float HydroFlowProducer::getPotentialDelta()
{
    return potentialDelta;
}

void HydroFlowProducer::setPotentialDelta(float delta)
{
    this->potentialDelta = delta;
    invalidateTiles();
}

float HydroFlowProducer::getRootQuadSize()
{
    return TileProducer::getRootQuadSize();
}

void HydroFlowProducer::setRootQuadSize(float size)
{
    graphs->setRootQuadSize(size);
    TileProducer::setRootQuadSize(size);
}

int HydroFlowProducer::getBorder()
{
    return graphs->getBorder();
}

TileCache::Tile* HydroFlowProducer::getTile(int level, int tx, int ty, unsigned int deadline)
{
    if (level > 0) {
        getTile(level - 1, tx / 2, ty / 2, deadline);
    }

    graphs->getTile(level, tx, ty, deadline);

    return TileProducer::getTile(level, tx, ty, deadline);
}

void HydroFlowProducer::putTile(TileCache::Tile *t)
{
    TileProducer::putTile(t);
    if (t->level > 0) {
        TileCache::Tile *t2 = findTile(t->level - 1, t->tx / 2, t->ty / 2);
        assert(t2 != NULL);
        putTile(t2);
    }

    TileCache::Tile *t2 = graphs->findTile(t->level, t->tx, t->ty);
    graphs->putTile(t2);

}

void HydroFlowProducer::getReferencedProducers(vector< ptr<TileProducer> > &producers) const
{
    producers.push_back(graphs);
}

void HydroFlowProducer::graphChanged()
{
    invalidateTiles();
    CurveDataFactory::graphChanged();
}

vec3d HydroFlowProducer::getTileCoords(int level, int tx, int ty)
{
    double ox = getRootQuadSize() * (double(tx) / (1 << level) - 0.5f);
    double oy = getRootQuadSize() * (double(ty) / (1 << level) - 0.5f);
    double l = getRootQuadSize() / (1 << level);
    return vec3d(ox, oy, l);
}

void HydroFlowProducer::addUsedTiles(int level, int tx, int ty, TileProducer *producer, set<TileCache::Tile*> tiles)
{
    TileCache::Tile::Id id = TileCache::Tile::getId(level, tx, ty);
    usedTiles.insert(make_pair(id, make_pair(producer, tiles)));
}

ptr<Task> HydroFlowProducer::startCreateTile(int level, int tx, int ty, unsigned int deadline, ptr<Task> task, ptr<TaskGraph> owner)
{
    if (Logger::DEBUG_LOGGER != NULL) {
        ostringstream oss;
        oss << "Start Hydro tile " << getId() << " " << level << " " << tx << " " << ty;
        Logger::DEBUG_LOGGER->log("RIVER", oss.str());
    }

    ptr<TaskGraph> result = owner == NULL ? createTaskGraph(task) : owner;

    Task *datasTask = new GetCurveDatasTask<HydroFlowProducer>(task.get(), result.get(), this, graphs.get(), NULL, this, level, tx, ty, 0);
    result->addTask(datasTask);
    result->addDependency(task, datasTask);

    TileCache::Tile *t = graphs->getTile(level, tx, ty, deadline);
    assert(t != NULL);
    result->addTask(t->task);
    result->addDependency(datasTask, t->task);

    if (level > 0) {
        t = getTile(level - 1, tx / 2, ty / 2, deadline);
        assert(t != NULL);
        result->addTask(t->task);
        result->addDependency(task, t->task);
    }
    TileProducer::startCreateTile(level, tx, ty, deadline, datasTask, result);
    return result;
}

bool HydroFlowProducer::doCreateTile(int level, int tx, int ty, TileStorage::Slot *data)
{
    if (Logger::DEBUG_LOGGER != NULL) {
        ostringstream oss;
        oss << "Hydro tile " << getId() << " " << level << " " << tx << " " << ty;
        Logger::DEBUG_LOGGER->log("RIVER", oss.str());
    }
    swHYDRODATA->start();
    bool res = false;

    ObjectTileStorage::ObjectSlot *objectData = dynamic_cast<ObjectTileStorage::ObjectSlot*>(data);
    assert(objectData != NULL);
    TileCache::Tile::TId id = TileCache::Tile::getTId(getId(), level, tx, ty);

    TileCache::Tile *graphTile = graphs->findTile(level, tx, ty);
    assert(graphTile != NULL);
    ptr<Graph> graphData = dynamic_cast<ObjectTileStorage::ObjectSlot*>(graphTile->getData())->data.cast<Graph>();

    float quadSize = getRootQuadSize() / (1 << level);

    bool diffVersion = false;
    if (objectData->data != NULL) {
        diffVersion = !objectData->data.cast<HydroFlowTile>()->equals(graphData->version, slipParameter, min((int) (quadSize / potentialDelta), displayTileSize), searchRadiusFactor);//objectData->data.cast<HydroFlowTile>()->version != graphData->version;
    }

    if (diffVersion || id != objectData->id || objectData->data == NULL ) {
        ptr<HydroFlowTile> hydroData;

        double ox = getRootQuadSize() * (double(tx) / (1 << level) - 0.5f);
        double oy = getRootQuadSize() * (double(ty) / (1 << level) - 0.5f);

        if (level >= minLevel) {
            if (graphData.cast<HydroGraph>() == NULL && graphData.cast<LazyHydroGraph>() == NULL) { // if the type of graph is wrong
                if (Logger::ERROR_LOGGER != NULL) {
                    ostringstream oss;
                    oss << "Bad Graph Type : Should be a [Lazy]HydroGraph.";
                    Logger::ERROR_LOGGER->log("RIVER", oss.str());
                }
                return false;
            }

            if (quadSize / potentialDelta < displayTileSize / 2 && level - 1 >=minLevel) { //maxLevel
                objectData->data = dynamic_cast<ObjectTileStorage::ObjectSlot*>(findTile(level - 1, tx / 2, ty / 2)->getData())->data;
                return true;
            }

            float scale = displayTileSize == -1 ? 1 : displayTileSize / quadSize;

            ptr<Graph::CurveIterator> ci = graphData->getCurves();
            vector<ptr<HydroCurve> > banks;
            float width = 0.f;
            while(ci->hasNext()) {
                ptr<HydroCurve> c = ci->next().cast<HydroCurve>();
                bool display = false;
                if (c->getType() != HydroCurve::BANK && c->getWidth() * scale > 1.0f) {
                    display = true;
                    if (width < c->getWidth()) {
                        width = c->getWidth();
                    }
                } else if (c->getType() == HydroCurve::BANK && c->getRiver().id != NULL_ID) {
                    if (c->getOwner()->getAncestor()->getCurve(c->getRiver()).cast<HydroCurve>()->getWidth() * scale > 1.0f) {
                        display = true;
                    }
                }
                if (display) {
                    banks.push_back(c);
                }
            }
            hydroData = new HydroFlowTile(ox, oy, quadSize, slipParameter, min((int) (quadSize / potentialDelta), displayTileSize), searchRadiusFactor);
            hydroData->addBanks(banks, width);
        } else {
            hydroData = new HydroFlowTile(ox, oy, quadSize, slipParameter, min((int) (quadSize / potentialDelta), displayTileSize), searchRadiusFactor);
        }

        objectData->data = hydroData;
        hydroData->version = graphData->version;
        res = true;
    }
    swHYDRODATA->end();

    TileProducer::doCreateTile(level, tx, ty, data);
    return res;
}

void HydroFlowProducer::stopCreateTile(int level, int tx, int ty)
{
    TileProducer::stopCreateTile(level, tx, ty);

    if (level > 0) {
            TileCache::Tile *t = findTile(level - 1, tx / 2, ty / 2);
            assert(t != NULL);
            putTile(t);
    }

    TileCache::Tile *t = graphs->findTile(level, tx, ty);
    assert(t != NULL);
    graphs->putTile(t);
}

void HydroFlowProducer::swap(ptr<HydroFlowProducer> p)
{
    TileProducer::swap(p);
    CurveDataFactory::swap(p.get());
    std::swap(graphs, p->graphs);
    std::swap(displayTileSize, p->displayTileSize);
    std::swap(slipParameter, p->slipParameter);
    std::swap(searchRadiusFactor, p->searchRadiusFactor);
    std::swap(potentialDelta, p->potentialDelta);
    std::swap(minLevel, p->minLevel);
}

class HydroFlowProducerResource : public ResourceTemplate<30, HydroFlowProducer>
{
public:
    HydroFlowProducerResource(ptr<ResourceManager> manager, const string &name, ptr<ResourceDescriptor> desc, const TiXmlElement *e = NULL) :
        ResourceTemplate<30, HydroFlowProducer>(manager, name, desc)
    {
        e = e == NULL ? desc->descriptor : e;
        ptr<TileCache> cache;
        ptr<GraphProducer> graphs;
        int displayTileSize = 192;
        float slip = 1.0f;
        float searchRadiusFactor = 1.0f;
        float potentialDelta = 0.01f;
        int minLevel = 0;

        checkParameters(desc, e, "name,cache,graphs,displayTileSize,slip,searchRadiusFactor, potentialDelta,minLevel,");
        cache = manager->loadResource(getParameter(desc, e, "cache")).cast<TileCache>();
        graphs = manager->loadResource(getParameter(desc, e, "graphs")).cast<GraphProducer>();
        if (e->Attribute("displayTileSize") != NULL) {
            getIntParameter(desc, e, "displayTileSize", &displayTileSize);
        }
        if (e->Attribute("slip") != NULL) {
            getFloatParameter(desc, e, "slip", &slip);
        }
        if (e->Attribute("searchRadiusFactor") != NULL) {
            getFloatParameter(desc, e, "searchRadiusFactor", &searchRadiusFactor);
        }
        if (e->Attribute("potentialDelta") != NULL) {
            getFloatParameter(desc, e, "potentialDelta", &potentialDelta);
        }
        if (e->Attribute("minLevel") != NULL) {
            getIntParameter(desc, e, "minLevel", &minLevel);
        }

        init(graphs, cache, displayTileSize, slip, searchRadiusFactor, potentialDelta, minLevel);
    }
};


extern const char hydroFlowProducer[] = "hydroFlowProducer";

static ResourceFactory::Type<hydroFlowProducer, HydroFlowProducerResource> HydroFlowProducerType;

}
