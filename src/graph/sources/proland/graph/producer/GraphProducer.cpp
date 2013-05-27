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

#include "proland/graph/producer/GraphProducer.h"

#include "ork/taskgraph/TaskGraph.h"
#include "proland/producer/ObjectTileStorage.h"

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#endif
#include <errno.h>

using namespace ork;

namespace proland
{

GraphProducer::GraphFactory::GraphFactory() : Object("GraphFactory")
{
}

GraphProducer::GraphFactory::~GraphFactory()
{
}

Graph *GraphProducer::GraphFactory::newGraph(int nodeCacheSize, int curveCacheSize, int areaCacheSize)
{
    return new BasicGraph();
}

void GraphProducer::GraphFactory::swap(ptr<GraphFactory> g)
{
}

GraphProducer::LazyGraphFactory::LazyGraphFactory() : GraphProducer::GraphFactory()
{
}

GraphProducer::LazyGraphFactory::~LazyGraphFactory()
{
}

Graph *GraphProducer::LazyGraphFactory::newGraph(int nodeCacheSize, int curveCacheSize, int areaCacheSize)
{
    LazyGraph *l = new LazyGraph();
    l->setNodeCacheSize(nodeCacheSize);
    l->setCurveCacheSize(curveCacheSize);
    l->setAreaCacheSize(areaCacheSize);
    return l;
}

GraphProducer::GraphCache::GraphCache(GraphPtr root, string graphName, ptr<ResourceManager> manager, bool loadSubgraphs) : Object("GraphCache")
{
    graphs.insert(make_pair(TileCache::Tile::getId(0, 0, 0), root));
    this->manager = manager;
    this->graphName = graphName;
    this->loadSubgraphs = loadSubgraphs;
}

GraphProducer::GraphCache::~GraphCache()
{
    graphs.clear();
}

void GraphProducer::GraphCache::add(TileCache::Tile::Id id, GraphPtr graph)
{
    graphs.insert(make_pair(id, graph));
    // finding the correct file name
    char fileName[100];
    sprintf(fileName, "%s.graph", graphName.c_str());
    string rootPath = manager->getLoader()->findResource(fileName);
    rootPath.erase(rootPath.end() - 6, rootPath.end());
#if defined(_WIN32) || defined(_WIN64)
    // GP : fix for error handlong on Windows
    int err = (0 != CreateDirectory(rootPath.c_str(), NULL));
#else
    char mdir[110];
    sprintf(mdir, "mkdir %s", rootPath.c_str());
    int err = system(mdir);
#endif
    if (err == 0 || errno == 2) { // no error or directory exists
        string::size_type slash = graphName.rfind('/');
        string graphBase = "";
        if (slash != string::npos) {
            graphBase = graphName.substr(slash + 1, graphName.size());
        } else {
            graphBase = graphName;
        }
        sprintf(fileName, "/%s_%02d_%02d_%02d.graph", graphBase.c_str(), id.first, id.second.first, id.second.second);
        rootPath.append(fileName);
        // and saving
        graph->save(rootPath, true, false);
    }
}

GraphPtr GraphProducer::GraphCache::getTile(TileCache::Tile::Id tileId)
{
    if (graphs.find(tileId) != graphs.end()) {
        return graphs[tileId];
    }
    GraphPtr tmp = NULL;
    char fileName[100];
    try {
        string::size_type slash = graphName.rfind('/');
        string graphBase = "";
        if (slash != string::npos) {
            graphBase = graphName.substr(slash + 1, graphName.size());
        } else {
            graphBase = graphName;
        }
        sprintf(fileName, "%s/%s_%02d_%02d_%02d.graph", graphName.c_str(), graphBase.c_str(), tileId.first, tileId.second.first, tileId.second.second);
        string filePath = manager->getLoader()->findResource(fileName);
        tmp = graphs.find(TileCache::Tile::getId(0, 0, 0))->second->createChild();
        tmp->setParent(getTile(TileCache::Tile::getId(0, 0, 0)).get());
        tmp->load(filePath, loadSubgraphs);
        graphs.insert(make_pair(tileId, tmp));

    } catch(exception) { // In case the manager doesn't find the file, the tile will be computed from the root graph.
        if (Logger::DEBUG_LOGGER != NULL) {
            ostringstream oss;
            oss << "Couldn't find file " << fileName;
           Logger::DEBUG_LOGGER->log("GRAPH", oss.str());
        }
        tmp = NULL;
    }
    return tmp;
}

void GraphProducer::GraphCache::swap(ptr<GraphProducer::GraphCache> p)
{
    std::swap(graphs, p->graphs);
    std::swap(manager, p->manager);
    std::swap(loadSubgraphs, p->loadSubgraphs);
    std::swap(graphName, p->graphName);
}

GraphProducer::GraphProducer(string name, ptr<TileCache> cache, ptr<GraphCache> precomputedGraphs, set<int> precomputedLevels, bool doFlatten, float flatnessFactor, bool storeParents, int maxNodes) :
    TileProducer("GraphProducer", "CreateGraphTile")
{
    init(name, cache, precomputedGraphs, precomputedLevels, doFlatten, flatnessFactor, storeParents, maxNodes);
}

GraphProducer::GraphProducer(string name, ptr<TileCache> cache, ptr<GraphCache> precomputedGraphs, bool doFlatten, float flatnessFactor, bool storeParents, int maxNodes) :
    TileProducer("GraphProducer", "CreateGraphTile")
{
    set<int> precomputed;
    precomputed.insert(0);
    init(name, cache, precomputedGraphs, precomputed, doFlatten, flatnessFactor, storeParents, maxNodes);
}

GraphProducer::GraphProducer() :
    TileProducer("GraphProducer", "CreateGraphTile")
{
}

void GraphProducer::init(string name, ptr<TileCache> cache, ptr<GraphCache> precomputedGraphs, set<int> precomputedLevels, bool doFlatten, float flatnessFactor, bool storeParents, int maxNodes)
{
    TileProducer::init(cache, true);
    this->name = name;
    this->precomputedGraphs = precomputedGraphs;
    this->precomputedLevels = precomputedLevels;
    this->tileSize = 0;
    this->margins = new ComposedMargin();
    this->doFlatten = doFlatten;
    this->flatnessFactor = flatnessFactor;
    this->storeParents = storeParents;
    this->maxNodes = maxNodes;
    getRoot()->addListener(this);
}

GraphProducer::~GraphProducer()
{
    getRoot()->removeListener(this);

    precomputedGraphs = NULL;

    flattenCurves.clear();
    flattenCurveCount.clear();

    delete margins;
}

GraphPtr GraphProducer::getRoot()
{
    return precomputedGraphs->getTile(TileCache::Tile::getId(0, 0, 0));
}

string GraphProducer::getName()
{
    return name;
}

int GraphProducer::getBorder()
{
    return 0;
}

void GraphProducer::setTileSize(int tileSize)
{
    this->tileSize = tileSize;
}

void GraphProducer::addMargin(Margin *m)
{
    margins->addMargin(m);
}

void GraphProducer::removeMargin(Margin *m)
{
    margins->removeMargin(m);
}

CurvePtr GraphProducer::getFlattenCurve(CurvePtr c)
{
    CurveId id = c->getId();
    map<CurveId, CurvePtr>::iterator i = flattenCurves.find(id);
    if (i == flattenCurves.end()) {
        CurvePtr flatten = new Curve(NULL, c, c->getStart(), c->getEnd());
        if (doFlatten) {
            flatten->flatten(flatnessFactor);
        }
        flattenCurves.insert(pair<CurveId, CurvePtr>(id, flatten));
        flattenCurveCount.insert(pair<CurvePtr, int>(flatten, 1));
        return flatten;
    } else {
        flattenCurveCount[i->second] = flattenCurveCount[i->second] + 1;
        return i->second;
    }
}

void GraphProducer::putFlattenCurve(CurveId id)
{
    map<CurveId, CurvePtr>::iterator i = flattenCurves.find(id);
    assert(i != flattenCurves.end());
    flattenCurveCount[i->second] = flattenCurveCount[i->second] - 1;
    if(flattenCurveCount[i->second] == 0) { // we delete the Curve
        flattenCurveCount.erase(i->second);
        flattenCurves.erase(id);
    }
}

void GraphProducer::updateFlattenCurve(const set<CurveId> &changedCurves)
{
    set<CurveId>::const_iterator end = changedCurves.end();
    set<CurveId>::const_iterator i = changedCurves.begin();
    while (i != end) {
        CurveId id = *i++;
        map<CurveId, CurvePtr>::iterator j = flattenCurves.find(id);
        if (j != flattenCurves.end()) {
            CurvePtr c = j->second;
            flattenCurveCount.erase(c);
            flattenCurves.erase(j);
        }
    }
}

bool GraphProducer::isPrecomputedLevel(int level)
{
    return precomputedLevels.find(level) != precomputedLevels.end();
}

TileCache::Tile* GraphProducer::getTile(int level, int tx, int ty, unsigned int deadline)
{
    if (storeParents) {
        if (level > 0) {
            if (isPrecomputedLevel(level)) {
                getTile(0, 0, 0, deadline);
            } else {
                getTile(level - 1, tx / 2, ty / 2, deadline);
            }
        }
    }
    return TileProducer::getTile(level, tx, ty, deadline);
}

void GraphProducer::putTile(TileCache::Tile *t)
{
    TileProducer::putTile(t);
    if (storeParents) {
        if (t->level > 0) {
            if (isPrecomputedLevel(t->level)) {
                t = findTile(0, 0, 0);
            } else {
                t = findTile(t->level - 1, t->tx / 2, t->ty / 2);
            }
            assert(t != NULL);
            putTile(t);
        }
    }
}

ptr<Task> GraphProducer::startCreateTile(int level, int tx, int ty, unsigned int deadline, ptr<Task> task, ptr<TaskGraph> owner)
{
    ptr<TaskGraph> result = owner == NULL ? createTaskGraph(task) : owner;
    if (Logger::DEBUG_LOGGER != NULL) {
        ostringstream oss;
        oss << "START Graph tile " << getId() << " " << level << " " << tx << " " << ty;
        Logger::DEBUG_LOGGER->log("GRAPH", oss.str());
    }
    if (level > 0) {
        TileCache::Tile *t;
        if (isPrecomputedLevel(level)) {
            t = getTile(0, 0, 0, deadline);
        } else {
            t = getTile(level - 1, tx / 2, ty / 2, deadline);
        }
        assert(t != NULL);
        result->addTask(t->task);
        result->addDependency(task, t->task);
    }
    return result;
}

bool GraphProducer::doCreateTile(int level, int tx, int ty, TileStorage::Slot *data)
{

    if (Logger::DEBUG_LOGGER != NULL) {
        ostringstream oss;
        oss << "Graph tile " << getId() << " " << level << " " << tx << " " << ty;
        Logger::DEBUG_LOGGER->log("GRAPH", oss.str());
    }

    ObjectTileStorage::ObjectSlot* objectData = static_cast<ObjectTileStorage::ObjectSlot*> (data);
    assert(objectData != NULL);
    //updateCase recomputeChilds = UPDATE;
    bool res = true;

    TileCache::Tile::TId id = TileCache::Tile::getTId(getId(), level, tx, ty);
    TileCache::Tile::Id tileId = TileCache::Tile::getId(level, tx, ty);

    if (level == 0) {
        GraphPtr g = getRoot();
        objectData->data = g;
        //updateChilds(g, level, tx, ty);

        if (g->changes.empty() == false) {
            set<CurveId> changes;
            changes.insert(g->changes.addedCurves.begin(), g->changes.addedCurves.end());
            changes.insert(g->changes.removedCurves.begin(), g->changes.removedCurves.end());
            updateFlattenCurve(changes);
        }
    } else {
        TileCache::Tile *t;

        if (isPrecomputedLevel(level)) {
            t = findTile(0, 0, 0);
        } else {
            t = findTile(level - 1, tx / 2, ty / 2);
        }

        assert(t != NULL);

        ObjectTileStorage::ObjectSlot *parentObjectData;
        parentObjectData = static_cast<ObjectTileStorage::ObjectSlot*> (t->getData());

        GraphPtr parentGraph = parentObjectData->data.cast<Graph>();
        assert(parentGraph != NULL);

        int versionDiff = 0;
        if (id == objectData->id && objectData->data != NULL) {
            if (objectData->data.cast<Graph>()->version != parentGraph->version) {
                versionDiff = parentGraph->version - objectData->data.cast<Graph>()->version;
            }
        }

        if (id != objectData->id || objectData->data == NULL || versionDiff > 0) {

            if (parentGraph->getNodeCount() < maxNodes && parentGraph->getCurveCount() < maxNodes / 2) {
                int sizeSum = 0;
                ptr<Graph::CurveIterator> ci = parentGraph->getCurves();
                while (ci->hasNext()) {
                    sizeSum += ci->next()->getSize();
                }
                if (sizeSum < maxNodes) {
                    objectData->data = parentGraph;
                    return true;
                }
            }

            // If tile needs an update
            float rootQuadSize = getRootQuadSize();
            double ox = rootQuadSize * (double(tx) / (1 << level) - 0.5f);
            double oy = rootQuadSize * (double(ty) / (1 << level) - 0.5f);
            double l = rootQuadSize / (1 << level);
            assert(l >= 0);
            box2d clip = box2d(ox, ox + l, oy, oy + l);
            float flat = l / tileSize * flatnessFactor;
            float squareFlat = max(0.1f, flat * flat);

            if (versionDiff == 1 && objectData->data != NULL && id == objectData->id) {
                // incremental clip
                GraphPtr graph = objectData->data.cast<Graph>();
                graph->changes.clear();
                graph->version++;
                parentGraph->clipUpdate(parentGraph->changes, clip, margins, *graph, graph->changes);
                if (doFlatten) {
                    graph->flattenUpdate(graph->changes, squareFlat);
                }
                if (graph->changes.empty()) { // No changes in this tile
                    res = false;
                }
            } else { //if tile is outdated or was deleted (unused) or if it was not created yet
                // full clip
                GraphPtr graph = NULL;
                if (isPrecomputedLevel(level)) {
                    graph = precomputedGraphs->getTile(tileId);
                }
                if (graph == NULL) {
                    graph = parentGraph->clip(clip, margins);
                    if (doFlatten) {
                        graph->flatten(squareFlat);
                    }
                    if (isPrecomputedLevel(level)) {
                        precomputedGraphs->add(tileId, graph);
                    }
                }
                graph->version = parentGraph->version;
                objectData->data = graph;
            }
        } else {
            // Tile doesn't need to be updated (same version as parent).
            res = false;
        }
    }

    return res;
}

void GraphProducer::stopCreateTile(int level, int tx, int ty)
{
    if (level > 0) {
        TileCache::Tile *t;
        if (isPrecomputedLevel(level)) {
            t = findTile(0, 0, 0);
        } else {
            t = findTile(level - 1, tx / 2, ty / 2);
        }
        assert(t != NULL);
        putTile(t);
    }
}

void GraphProducer::graphChanged()
{
    invalidateTile(0, 0, 0);
    updateFlattenCurve(getRoot()->changes.removedCurves);
}

void GraphProducer::swap(ptr<GraphProducer> p)
{
    TileProducer::swap(p);
    invalidateTiles();
    p->invalidateTiles();
    std::swap(precomputedGraphs, p->precomputedGraphs);
    std::swap(precomputedLevels, p->precomputedLevels);
    std::swap(tileSize, p->tileSize);
    std::swap(margins, p->margins);
    std::swap(doFlatten, p->doFlatten);
    std::swap(flatnessFactor, p->flatnessFactor);
    std::swap(storeParents, p->storeParents);
    std::swap(flattenCurves, p->flattenCurves);
    std::swap(flattenCurveCount, p->flattenCurveCount);
}

class GraphFactoryResource : public ResourceTemplate<3, GraphProducer::GraphFactory>
{
public:
    GraphFactoryResource(ptr<ResourceManager> manager, const string &name, ptr<ResourceDescriptor> desc, const TiXmlElement *e = NULL) :
        ResourceTemplate<3, GraphProducer::GraphFactory>(manager, name, desc)
    {
    }
};

class LazyGraphFactoryResource : public ResourceTemplate<3, GraphProducer::LazyGraphFactory>
{
public:
    LazyGraphFactoryResource(ptr<ResourceManager> manager, const string &name, ptr<ResourceDescriptor> desc, const TiXmlElement *e = NULL) :
        ResourceTemplate<3, GraphProducer::LazyGraphFactory>(manager, name, desc)
    {
    }
};

class GraphProducerResource : public ResourceTemplate<3, GraphProducer>
{

public:
    GraphProducerResource(ptr<ResourceManager> manager, const string &name, ptr<ResourceDescriptor> desc,
            const TiXmlElement *e = NULL) :
        ResourceTemplate<3, GraphProducer> (manager, name, desc)
    {
        e = e == NULL ? desc->descriptor : e;
        ptr<TileCache> cache;
        string gname;
        string graphName;
        string filePath;
        char fileName[100];
        ptr<GraphFactory> factory;

        bool loadSubgraphs = true;
        bool doFlatten = true;
        bool storeParents = false;
        float flatnessFactor = 0.1f;
        int nodeCacheSize = 0;
        int curveCacheSize = 0;
        int areaCacheSize = 0;
        //int precomputedLevel = 0;
        set<int> precomputedLevels;
        precomputedLevels.insert(0);
        int maxNodes = 0;
        checkParameters(desc, e, "name,factory,cache,file,loadSubgraphs,storeParents,doFlatten,flattness,nodeCacheSize,curveCacheSize,areaCacheSize,precomputedLevel,precomputedLevels,maxNodes,");
        gname = getParameter(desc, e, "name");
        cache = manager->loadResource(getParameter(desc, e, "cache")).cast<TileCache>();
        graphName = getParameter(desc, e, "file");
        sprintf(fileName, "%s.graph", graphName.c_str());
        filePath = manager->getLoader()->findResource(fileName);

        if (e->Attribute("precomputedLevels") != NULL) {
            string s = getParameter(desc, e, "precomputedLevels") + ",";
            string::size_type start = 0;
            string::size_type index;
            string::size_type i;
            while ((index = s.find(',', start)) != string::npos) {
                string levelList = s.substr(start, index - start);
                i = levelList.find(':', 0);
                if (i != string::npos) {
                    int first = atoi(levelList.substr(0, i).c_str());
                    int last = atoi(levelList.substr(i + 1, levelList.size()).c_str());
                    for (int j = first; j <= last; j++) {
                        precomputedLevels.insert(j);
                    }
                } else {
                    precomputedLevels.insert(atoi(levelList.c_str()));

                }
                start = index + 1;
            }
        }

        if (e->Attribute("precomputedLevel") != NULL) {
            int precomputedLevel;
            getIntParameter(desc, e, "precomputedLevel", &precomputedLevel);
            precomputedLevels.insert(precomputedLevel);
        }
        if (e->Attribute("loadSubgraphs") != NULL) {
            loadSubgraphs = strcmp(e->Attribute("loadSubgraphs"), "true") == 0;
        }
        if (e->Attribute("doFlatten") != NULL) {
            doFlatten = strcmp(e->Attribute("doFlatten"), "true") == 0;
        }
        if (doFlatten == false) {
            flatnessFactor = 0.0f;
        } else if (e->Attribute("flattness") != NULL) {
            getFloatParameter(desc, e, "flattness", &flatnessFactor);
        }
        if (e->Attribute("nodeCacheSize") != NULL) {
            getIntParameter(desc, e, "nodeCacheSize", &nodeCacheSize);
        }
        if (e->Attribute("curveCacheSize") != NULL) {
            getIntParameter(desc, e, "curveCacheSize", &curveCacheSize);
        }
        if (e->Attribute("areaCacheSize") != NULL) {
            getIntParameter(desc, e, "areaCacheSize", &areaCacheSize);
        }
        if (e->Attribute("maxNodes") != NULL) {
            getIntParameter(desc, e, "maxNodes", &maxNodes);
        }
        if (e->Attribute("storeParents") != NULL) {
            storeParents = strcmp(e->Attribute("storeParents"), "true") == 0;
        }

        if (e->Attribute("factory") != NULL) {
            factory = manager->loadResource(getParameter(desc, e, "factory")).cast<GraphFactory>();
        } else {
            factory = new GraphProducer::LazyGraphFactory();
        }

        ptr<Graph> root = factory->newGraph(nodeCacheSize, curveCacheSize, areaCacheSize);
        root->load(filePath, loadSubgraphs);

        ptr<GraphCache> precomputedGraphs = new GraphCache(root, graphName, manager, loadSubgraphs);

        init(gname, cache, precomputedGraphs, precomputedLevels, doFlatten, flatnessFactor, storeParents, maxNodes);
    }
};

extern const char graphProducer[] = "graphProducer";
extern const char lazyGraphFactory[] = "lazyGraphFactory";
extern const char basicGraphFactory[] = "basicGraphFactory";

static ResourceFactory::Type<graphProducer, GraphProducerResource> GraphProducerType;
static ResourceFactory::Type<lazyGraphFactory, LazyGraphFactoryResource> LazyGraphFactoryType;
static ResourceFactory::Type<basicGraphFactory, GraphFactoryResource> BasicGraphFactoryType;

}
