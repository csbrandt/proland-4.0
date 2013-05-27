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

#include "proland/graph/LazyGraph.h"

#include "proland/graph/BasicGraph.h"
#include "proland/graph/LazyNode.h"
#include "proland/graph/LazyCurve.h"
#include "proland/graph/LazyArea.h"

#include <sstream>

#include "ork/core/Logger.h"

namespace proland
{

LazyGraph::LazyGraph() :
    Graph()
{
    nodeCache = NULL;
    curveCache = NULL;
    areaCache = NULL;
    init();
}

LazyGraph::~LazyGraph()
{
    clear();
    delete areaCache;
    delete curveCache;
    delete nodeCache;
    if (fileReader != NULL) {
        delete fileReader;
    }
}

void LazyGraph::init()
{
    nextNodeId.id = 0;
    nextCurveId.id = 0;
    nextAreaId.id = 0;
    if (nodeCache != NULL) {
        delete nodeCache;
    }
    if (curveCache != NULL) {
        delete curveCache;
    }
    if (areaCache != NULL) {
        delete areaCache;
    }
    nodeCache = new GraphCache<Node> (this);
    curveCache = new GraphCache<Curve> (this);
    areaCache = new GraphCache<Area> (this);
    fileReader = NULL;
}

NodePtr LazyGraph::get(NodeId id)
{
    return getNode(id);
}

CurvePtr LazyGraph::get(CurveId id)
{
    return getCurve(id);
}

AreaPtr LazyGraph::get(AreaId id)
{
    return getArea(id);
}

void LazyGraph::clear()
{
    for (map<AreaId, Area *>::iterator i = areas.begin(); i != areas.end(); i++) {
        i->second->setOwner(NULL);
    }
    for (map<CurveId, Curve *>::iterator i = curves.begin(); i != curves.end(); i++) {
        i->second->setOwner(NULL);
    }
    for (map<NodeId, Node *>::iterator i = nodes.begin(); i != nodes.end(); i++) {
        i->second->setOwner(NULL);
    }
    //while (areas.begin() != areas.end()) {
    //    delete areas.begin()->second;
    //}
    //while (curves.begin() != curves.end()) {
    //    delete curves.begin()->second;
    //}
    //while (nodes.begin() != nodes.end()) {
    //    delete nodes.begin()->second;
    //}
    nodeOffsets.clear();
    curveOffsets.clear();
    areaOffsets.clear();
    subgraphOffsets.clear();
    areas.clear();
    curves.clear();
    nodes.clear();
}

void LazyGraph::setNodeCacheSize(int size)
{
    nodeCache->size = size;
}

void LazyGraph::setCurveCacheSize(int size)
{
    curveCache->size = size;
}

void LazyGraph::setAreaCacheSize(int size)
{
    areaCache->size = size;
}

int LazyGraph::getNodeCount() const
{
    return nodeOffsets.size();
}

int LazyGraph::getCurveCount() const
{
    return curveOffsets.size();
}

int LazyGraph::getAreaCount() const
{
    return areaOffsets.size();
}

map<NodeId, long int> LazyGraph::getNodeOffsets() const
{
    return nodeOffsets;
}

map<CurveId, long int> LazyGraph::getCurveOffsets() const
{
    return curveOffsets;
}

map<AreaId, long int> LazyGraph::getAreaOffsets() const
{
    return areaOffsets;
}

ptr<Graph::NodeIterator> LazyGraph::getNodes()
{
    return new LazyNodeIterator(nodeOffsets, this);
}

ptr<Graph::CurveIterator> LazyGraph::getCurves()
{
    return new LazyCurveIterator(curveOffsets, this);
}

ptr<Graph::AreaIterator> LazyGraph::getAreas()
{
    return new LazyAreaIterator(areaOffsets, this);
}

NodePtr LazyGraph::loadNode(long int offset, NodeId id)
{
    assert(fileReader != NULL);
    long int oldOffset = fileReader->tellg();
    float x, y;
    fileReader->seekg(offset, ios::beg);
    x = fileReader->read<float>();
    y = fileReader->read<float>();
    for (int j = 2; j < nParamsNodes; j++) {
        fileReader->read<float>();
    }
    int size = fileReader->read<int>();
    ptr<LazyNode> n = new LazyNode(this, id, x, y);
    for (int i = 0; i < size; i++) {
        CurveId cid;
        cid.id = fileReader->read<int>();
        n->loadCurve(cid);
    }
    fileReader->seekg(oldOffset, ios::beg);
    return n;
}

CurvePtr LazyGraph::loadCurve(long int offset, CurveId id)
{
    assert(fileReader != NULL);
    long int oldOffset = fileReader->tellg();
    int size, type, start, end;
    float width;
    fileReader->seekg(offset, ios::beg);
    //CurvePtr c = new LazyCurve(this, id);
    ptr<LazyCurve> c = new LazyCurve(this, id);
    size = fileReader->read<int>();
    width = fileReader->read<float>();
    type = fileReader->read<int>();
    for (int j = 3; j < nParamsCurves; j++) {
        fileReader->read<float>();
    }
    start = fileReader->read<int>();
    for (int j = 1; j < nParamsCurveExtremities; j++) {
        fileReader->read<float>();
    }

    c->width = width;
    c->type = type;

    NodeId nis;
    nis.id = start;
    c->loadVertex(nis);

    for (int j = 1; j < size - 1; j++) {
        float x, y;
        int isControl;
        x = fileReader->read<float>();
        y = fileReader->read<float>();
        isControl = fileReader->read<int>();
        c->loadVertex(x, y, -1, isControl == 1);

        for (int j = 3; j < nParamsCurvePoints; j++) {
            fileReader->read<float>();
        }
    }

    end = fileReader->read<int>();
    for (int j = 1; j < nParamsCurveExtremities; j++) {
        fileReader->read<float>();
    }

    nis.id = end;
    c->loadVertex(nis);

    c->computeCurvilinearCoordinates();

    AreaId aid;
    aid.id = fileReader->read<int>();
    c->loadArea(aid);
    aid.id = fileReader->read<int>();
    c->loadArea(aid);
    fileReader->read<int>(); //parent
    fileReader->seekg(oldOffset, ios::beg);
    return c;
}

AreaPtr LazyGraph::loadArea(long int offset, AreaId id)
{
    assert(fileReader != NULL);
    long int oldOffset = fileReader->tellg();
    fileReader->seekg(offset, ios::beg);
    //AreaPtr a = new LazyArea(this, id);
    ptr<LazyArea> a = new LazyArea(this, id);
    int size, info, subgraph, index, orientation;
    size = fileReader->read<int>();
    info = fileReader->read<int>();
    subgraph = fileReader->read<int>();
    a->info = info;

    for (int j = 3; j < nParamsAreas; j++) {
        fileReader->read<float>();
    }

    for (int j = 0; j < size; j++) {
        index = fileReader->read<int>();
        orientation = fileReader->read<int>();

        for (int j = 2; j < nParamsAreaCurves; j++) {
            fileReader->read<float>();
        }
        CurveId cid;
        cid.id = index;
        a->loadCurve(cid, orientation);
    }
    for (int j = 0; j < nParamsSubgraphs; j++) {
        fileReader->read<float>();
    }
    fileReader->read<int>(); //parent

    if (subgraph == 0) {
        a->subgraph = NULL;
    } else {
        a->subgraph = getSubgraph(id);
    }
    fileReader->seekg(oldOffset, ios::beg);
    return a;
}

GraphPtr LazyGraph::loadSubgraph(long int offset, AreaId id)
{
    assert(fileReader != NULL);
    long int oldOffset = fileReader->tellg();
    fileReader->seekg(offset, ios::beg);
    GraphPtr g = createChild();
    g->load(fileReader, true);
    fileReader->seekg(oldOffset, ios::beg);
    return g;
}

LazyGraph::GraphCache<Node> *LazyGraph::getNodeCache()
{
    return nodeCache;
}

LazyGraph::GraphCache<Curve> *LazyGraph::getCurveCache()
{
    return curveCache;
}

LazyGraph::GraphCache<Area> *LazyGraph::getAreaCache()
{
    return areaCache;
}

NodePtr LazyGraph::getNode(NodeId id)
{
    if (id.id == NULL_ID) {
        return NULL;
    }

    map<NodeId, Node *>::iterator i = nodes.find(id);
    if (i != nodes.end()) { // if the requested resource has already been loaded
        NodePtr r = i->second;
        nodeCache->remove(r);// we remove it from the unusedResources Cache
        //r->owner = this;
        // and we return the resource
        return r;
    }
    if (Logger::DEBUG_LOGGER != NULL) {
        ostringstream os;
        os << "Loading node '" << id.id << "'";
        Logger::DEBUG_LOGGER->log("GRAPH", os.str());
    }
    // otherwise the resource is not already loaded; we first load its descriptor
    NodePtr r = NULL;
    long int offset;
    map<NodeId, long int>::iterator j = nodeOffsets.find(id);
    if (j != nodeOffsets.end()) {
        offset = j->second;
        r = loadNode(offset, id);
        nodes[id] = r.get();
        mapping->insert(make_pair(r->getPos(), r.get()));
        return r;
    }

    if (Logger::ERROR_LOGGER != NULL) {
        ostringstream os;
        os << "Loading : Missing or invalid node '" << id.id << "'";
        Logger::ERROR_LOGGER->log("GRAPH", os.str());
    }
    throw exception();
//    return NULL;
}

CurvePtr LazyGraph::getCurve(CurveId id)
{
    if (id.id == NULL_ID) {
        return NULL;
    }
    map<CurveId, Curve *>::iterator i = curves.find(id);
    if (i != curves.end()) { // if the requested resource has already been loaded
        CurvePtr r = i->second;
        curveCache->remove(r);// we remove it from the unusedResources Cache
        //r->owner = this;
        // and we return the resource
        return r;
    }
    if (Logger::DEBUG_LOGGER != NULL) {
        ostringstream os;
        os << "Loading curve '" << id.id << "'";
        Logger::DEBUG_LOGGER->log("GRAPH", os.str());
    }
    // otherwise the resource is not already loaded; we first load its descriptor
    CurvePtr r = NULL;
    long int offset;
    map<CurveId, long int>::iterator j = curveOffsets.find(id);
    if (j != curveOffsets.end()) {
        offset = j->second;
        r = loadCurve(offset, id);
        curves.insert(make_pair(id, r.get()));
        return r;
    }
    if (Logger::ERROR_LOGGER != NULL) {
        ostringstream os;
        os << "Loading : Missing or invalid curve '" << id.id << "'";
        Logger::ERROR_LOGGER->log("GRAPH", os.str());
    }
    throw exception();
//    return NULL;
}

AreaPtr LazyGraph::getArea(AreaId id)
{
    if (id.id == NULL_ID) {
        return NULL;
    }

    map<AreaId, Area *>::iterator i = areas.find(id);
    if (i != areas.end()) { // if the requested resource has already been loaded
        AreaPtr r = i->second;
        areaCache->remove(r); // we remove it from the unusedResources Cache
        //r->owner = this;
        // and we return the resource
        return r;
    }
    if (Logger::DEBUG_LOGGER != NULL) {
        ostringstream os;
        os << "Loading area '" << id.id << "'";
        Logger::DEBUG_LOGGER->log("GRAPH", os.str());
    }
    // otherwise the resource is not already loaded; we first load its descriptor
    AreaPtr r = NULL;
    long int offset;
    map<AreaId, long int>::iterator j = areaOffsets.find(id);
    if (j != areaOffsets.end()) {
        offset = j->second;
        r = loadArea(offset, id);
        areas[id] = r.get();
        return r;
    }
    if (Logger::ERROR_LOGGER != NULL) {
        ostringstream os;
        os << "Loading : Missing or invalid area '" << id.id << "'";
        Logger::ERROR_LOGGER->log("GRAPH", os.str());
    }
    throw exception();
//    return NULL;
}

GraphPtr LazyGraph::getSubgraph(AreaId id)
{
    if (subgraphOffsets.size() > 0) {
        map<AreaId, long int>::iterator i = subgraphOffsets.find(id);
        assert(i != subgraphOffsets.end());
        long int offset = i->second;
        GraphPtr r = NULL;
        r = loadSubgraph(offset, id);
        return r;
    }
    return NULL;
}

ptr<Graph::CurveIterator> LazyGraph::getChildCurves(CurveId parentId)
{
    return new LazyCurveIterator(curveOffsets, this);
}

AreaPtr LazyGraph::getChildArea(AreaId parentId)
{
    return getArea(parentId);
}

void LazyGraph::releaseNode(NodeId id)
{
    map<NodeId, Node*>::iterator i;
    i = nodes.find(id);
    if (i == nodes.end()) {
        // if this resource is not managed by this manager, we delete it
        if (Logger::ERROR_LOGGER != NULL) {
            ostringstream os;
            os << "Release : Missing or invalid node '" << id.id << "'";
            Logger::ERROR_LOGGER->log("GRAPH", os.str());
        }
        return;
    }

    if (find(nodeCache->changedResources.begin(), nodeCache->changedResources.end(), i->second) != nodeCache->changedResources.end()) {
        return;
    }

    if (nodeCache->size > 0) {
        // Else we add the resource to the cache
        nodeCache->add(i->second);
    } else {
        // if there is no cache of unused resources, then we delete resources as
        // soon as they become unused
        delete i->second;
    }
}

void LazyGraph::releaseCurve(CurveId id)
{
    map<CurveId, Curve*>::iterator i;
    i = curves.find(id);
    if (i == curves.end()) {
        if (Logger::ERROR_LOGGER != NULL) {
            ostringstream os;
            os << "Release : Missing or invalid curve '" << id.id << "'";
            Logger::ERROR_LOGGER->log("GRAPH", os.str());
        }
        return;
    }

    if (find(curveCache->changedResources.begin(), curveCache->changedResources.end(), i->second) != curveCache->changedResources.end()) {
        return;
    }

    if (curveCache->size > 0) {
        curveCache->add(i->second);
    } else {
        delete i->second;
    }
}

void LazyGraph::releaseArea(AreaId id)
{
    map<AreaId, Area*>::iterator i;
    i = areas.find(id);
    if (i == areas.end()) {
        if (Logger::ERROR_LOGGER != NULL) {
            ostringstream os;
            os << "Release : Missing or invalid area '" << id.id << "'";
            Logger::ERROR_LOGGER->log("GRAPH", os.str());
        }
        return;
    }

    if (find(areaCache->changedResources.begin(), areaCache->changedResources.end(), i->second) != areaCache->changedResources.end()) {
        return;
    }

    if (areaCache->size > 0) {
        areaCache->add(i->second);
    } else {
        delete i->second;
    }
}

void LazyGraph::remove(Node *n)
{
    removeNode(n->getId());
}

void LazyGraph::remove(Curve *c)
{
    removeCurve(c->getId());
}


void LazyGraph::remove(Area *a)
{
    removeArea(a->getId());
}

void LazyGraph::deleteNode(NodeId id)
{
    // removes this resource from the #nodes map
    map<NodeId, Node*>::iterator i;
    i = nodes.find(id);
    map<vec2d, Node *, Cmp>::iterator j = mapping->find(i->second->getPos());

    if (i != nodes.end()) {
        nodes.erase(i);
    }
    if (j != mapping->end()) {
        mapping->erase(j);
    }

    // it is not necessary to remove the resource from the unused resource cache
    // indeed this should have been done already (see #releaseNode)
}

void LazyGraph::deleteCurve(CurveId id)
{
    // removes this resource from the #curves map
    map<CurveId, Curve*>::iterator i;
    i = curves.find(id);

    if (i != curves.end()) {
        //i->second->clear();
        curves.erase(i);
    }
}

void LazyGraph::deleteArea(AreaId id)
{
    // removes this resource from the #nodes map
    map<AreaId, Area*>::iterator i;
    i = areas.find(id);

    if (i != areas.end()) {
        areas.erase(i);
    }
}

void LazyGraph::removeNode(NodeId id)
{
    NodePtr n = getNode(id);
    nodeCache->changedResources.erase(n.get());

    map<NodeId, long int>::iterator k = nodeOffsets.find(id);
    if (k != nodeOffsets.end()) {
        nodeOffsets.erase(k);
    }
    //n->owner = NULL;
    nodeCache->changedResources.erase(n.get());
}

void LazyGraph::removeCurve(CurveId id)
{
    CurvePtr c = getCurve(id);
    if (c != NULL) {
        NodePtr start = c->getStart();
        NodePtr end = c->getEnd();
        NodeId nId;
        nId.id = NULL_ID;
        c->addVertex(nId, 0);
        c->addVertex(nId, 1);
        if (start != end && start != NULL) {
            start->removeCurve(id);
            if (start->getCurveCount() == 0) {
                NodeId sid = start->getId();
                start = NULL;
                removeNode(sid);
            } else {
                nodeCache->add(start.get(), true);
            }
        }

        if (end != NULL) {
            end->removeCurve(id);
            if (end->getCurveCount() == 0) {
                NodeId eid = end->getId();
                end = NULL;
                removeNode(eid);
            } else {
                nodeCache->add(end.get(), true);
            }
        }
    }

    curveCache->changedResources.erase(c.get());
    map<CurveId, long int>::iterator k = curveOffsets.find(id);
    if (k != curveOffsets.end()) {
        curveOffsets.erase(k);
    }
}

void LazyGraph::removeArea(AreaId id)
{
    AreaPtr a = getArea(id);
    if (a != NULL) {
        for (int i = 0; i < a->getCurveCount(); i++) {
            CurvePtr c = a->getCurve(i);
            c->removeArea(a->getId());
            curveCache->add(c.get(), true);
        }
        while (a->getCurveCount()) {
            a->removeCurve(0);
        }
    }

    areaCache->changedResources.erase(a.get());
    map<AreaId, long int>::iterator k = areaOffsets.find(id);
    if (k != areaOffsets.end()) {
        areaOffsets.erase(k);
    }
}


NodePtr LazyGraph::newNode(const vec2d &p)
{
    NodeId id = nextNodeId;
    nextNodeId.id++;
    NodePtr n = new LazyNode(this, id, p.x, p.y);
    if (mapping != NULL) {
        mapping->insert(make_pair(p, n.get()));
    }
    nodes.insert(make_pair(id, n.get()));

    if (nodeOffsets.find(id) == nodeOffsets.end()) {
        nodeOffsets.insert(make_pair(id, (long int) - 1));
    }

    nodeCache->add(n.get(), true);
    return n;
}

CurvePtr LazyGraph::newCurve(CurvePtr parent, bool setParent)
{
    CurveId id = nextCurveId;
    nextCurveId.id++;
    CurvePtr c = new LazyCurve(this, id);
    curves.insert(make_pair(id, c.get()));

    if (curveOffsets.find(id) == curveOffsets.end()) {
        curveOffsets.insert(make_pair(id, (long int) - 1));
    }

    curveCache->add(c.get(), true);
    return c;
}

CurvePtr LazyGraph::newCurve(CurvePtr model, NodePtr start, NodePtr end)
{
    CurveId id = nextCurveId;
    nextCurveId.id++;
    CurvePtr c = new LazyCurve(this, id, start->getId(), end->getId());
    start->addCurve(id);
    end->addCurve(id);
    if (model != NULL) {
        map<CurveId, long int>::iterator ci = curveOffsets.find(model->getId());
        curveOffsets.insert(make_pair(id, ci->second));
    } else {
        curveOffsets.insert(make_pair(id, (long int) -1));
    }
    curves.insert(make_pair(id, c.get()));

    curveCache->add(c.get(), true);
    return c;
}

AreaPtr LazyGraph::newArea(AreaPtr parent, bool setParent)
{
    AreaId id = nextAreaId;
    nextAreaId.id++;
    AreaPtr a = new LazyArea(this, id);
    areas.insert(make_pair(id, a.get()));

    if (areaOffsets.find(id) == areaOffsets.end()) {
            areaOffsets.insert(make_pair(id, (long int) - 1));
    }
    areaCache->add(a.get(), true);

    return a;
}

void LazyGraph::movePoint(CurvePtr c, int i, const vec2d &p)
{
    if (i == 0 || i == c->getSize() - 1) {
        NodePtr n = i == 0 ? c->getStart() : c->getEnd();
        nodeCache->add(n.get(), true);
    } else {
        curveCache->add(c.get(), true);
    }
    Graph::movePoint(c, i, p);
}

void LazyGraph::clean()
{
}

void LazyGraph::readSubgraph()
{
    fileReader->read<int>();//nParams
    fileReader->read<int>();
    fileReader->read<int>();
    fileReader->read<int>();
    fileReader->read<int>();
    fileReader->read<int>();
    fileReader->read<int>();

    int nodeCount = fileReader->read<int>();
    for (int i = 0; i < nodeCount; i++) {
        for (int k = 0; k < nParamsNodes; k++) {
            fileReader->read<float>(); //x,y
        }
        int s = fileReader->read<int>();//number of curves
        for (int i = 0; i < s; i++) {
            fileReader->read<int>();//curve
        }
    }

    int curveCount = fileReader->read<int>();
    for (int i = 0; i < curveCount; i++) {
        int s = fileReader->read<int>();//size
        for (int k = 1; k < nParamsCurves; k++) {
            fileReader->read<float>(); // params
        }
        for (int k = 0; k < nParamsCurveExtremities; k++) { //start
            fileReader->read<float>();
        }

        for (int j = 1; j < s - 1; j++) {
            for (int k = 0; k < nParamsCurvePoints; k++) {
                fileReader->read<float>();//points
            }
        }
        for (int k = 0; k < nParamsCurveExtremities; k++) {//end
            fileReader->read<float>();
        }
        fileReader->read<int>();//area1
        fileReader->read<int>();//area2
        fileReader->read<int>();//parent
    }

    int areaCount = fileReader->read<int>();
    int subGraphs = 0;
    for (int i = 0; i < areaCount; i++) {
        int s = fileReader->read<int>();//size
        fileReader->read<int>();//info
        subGraphs += fileReader->read<int>();//subgraph
        for (int k = 3; k < nParamsAreas; k++) {//area
            fileReader->read<float>();
        }
        for (int j = 0; j < s; j++) {
        for (int k = 0; k < nParamsAreaCurves; k++) {//areaCurves
            fileReader->read<float>();
        }
        }
        fileReader->read<int>();//parent
        for (int k = 0; k < nParamsSubgraphs; k++) {//subgraphs
            fileReader->read<float>();
        }
    }
    for (int i = 0; i < subGraphs; ++i) {
        readSubgraph();
    }
}

void LazyGraph::load(const string &file, bool loadSubgraphs)
{
    clear();
    init();

    bool isIndexed = false;
    if (fileReader != NULL) {
        delete fileReader;
    }
    fileReader = new FileReader(file, isIndexed);
    if (isIndexed) {
        loadIndexed(loadSubgraphs);
    } else {
        load(loadSubgraphs);
    }
}

void LazyGraph::load(bool loadSubgraphs)
{
    assert(fileReader != 0);
    int nodeCount;
    int curveCount;
    int areaCount;
    vector<AreaId> subgraphedAreas;

    nParamsNodes = fileReader->read<int>();
    nParamsCurves = fileReader->read<int>();
    nParamsAreas = fileReader->read<int>();
    nParamsCurveExtremities = fileReader->read<int>();
    nParamsCurvePoints = fileReader->read<int>();
    nParamsAreaCurves = fileReader->read<int>();
    nParamsSubgraphs = fileReader->read<int>();

    checkParams(nParamsNodes, nParamsCurves, nParamsAreas, nParamsCurveExtremities, nParamsCurvePoints, nParamsAreaCurves, nParamsSubgraphs);

    nodeCount = fileReader->read<int>();
    for (int i = 0; i < nodeCount; i++) {
        NodeId nid = nextNodeId;
        nextNodeId.id++;
        nodeOffsets.insert(make_pair(nid, fileReader->tellg()));
        for (int j = 0; j < nParamsNodes; j++) {
            fileReader->read<float>();
        }
//        fileReader->read<float>(); //x
//        fileReader->read<float>(); //y
        int size = fileReader->read<int>(); //number of curves
        for (int j = 0; j < size; j++) {
            fileReader->read<int>(); //curves
        }
    }

    curveCount = fileReader->read<int>();
    for (int i = 0; i < curveCount; i++) {
        CurveId cid = nextCurveId;
        nextCurveId.id++;
        curveOffsets.insert(make_pair(cid, fileReader->tellg()));
        int s = fileReader->read<int>(); //size

        for (int j = 1; j < nParamsCurves; j++) {
            fileReader->read<float>();
        }

        for (int j = 0; j < nParamsCurveExtremities; j++) {
            fileReader->read<float>();
        }

        for (int j = 1; j < s - 1; j++) {
            for (int k = 0; k < nParamsCurvePoints; k++) {
                fileReader->read<float>();
            }
        }

        for (int j = 0; j < nParamsCurveExtremities; j++) {
            fileReader->read<float>();
        }

        fileReader->read<int>(); //area1
        fileReader->read<int>(); //area2
        fileReader->read<int>(); //parent
    }

    areaCount = fileReader->read<int>();
    int t;
    for (int i = 0; i < areaCount; i++) {
        AreaId aid = nextAreaId;
        nextAreaId.id++;
        areaOffsets.insert(make_pair(aid, fileReader->tellg()));
        int s = fileReader->read<int>(); //size
        fileReader->read<int>(); //info
        t = fileReader->read<int>(); //subgraph

        for (int j = 3; j < nParamsAreas; j++) {
            fileReader->read<float>();
        }
        if (t && loadSubgraphs) {
            subgraphedAreas.push_back(aid);
        }
        for (int j = 0; j < s; j++) {
            for (int k = 0; k < nParamsAreaCurves; k++) {
                fileReader->read<float>();
            }
        }
        fileReader->read<int>(); //parent
        for (int j = 0; j < nParamsSubgraphs; j++) {
            fileReader->read<float>();
        }
    }
    if (loadSubgraphs) {
        for (int i = 0; i < (int) subgraphedAreas.size(); ++i) {
            AreaId areaId = subgraphedAreas[i];
            subgraphOffsets.insert(make_pair(areaId, fileReader->tellg()));
            readSubgraph();
        }
    }
}

void LazyGraph::loadIndexed(bool loadSubgraphs)
{
    assert(fileReader != 0);
    int nodeCount;
    int curveCount;
    int areaCount;
    int subgraphCount;
    long int offset;

    nParamsNodes = fileReader->read<int>();
    nParamsCurves = fileReader->read<int>();
    nParamsAreas = fileReader->read<int>();
    nParamsCurveExtremities = fileReader->read<int>();
    nParamsCurvePoints = fileReader->read<int>();
    nParamsAreaCurves = fileReader->read<int>();
    nParamsSubgraphs = fileReader->read<int>();

    checkParams(nParamsNodes, nParamsCurves, nParamsAreas, nParamsCurveExtremities, nParamsCurvePoints, nParamsAreaCurves, nParamsSubgraphs);

    offset = fileReader->read<long int>();
    fileReader->seekg(offset, ios::beg);

    nodeCount = fileReader->read<int>();
    curveCount = fileReader->read<int>();
    areaCount = fileReader->read<int>();
    subgraphCount = fileReader->read<int>();

    for (int i = 0; i < nodeCount; i++) {
        NodeId nid = nextNodeId;
        nextNodeId.id++;
        offset = fileReader->read<long int>();
        nodeOffsets.insert(make_pair(nid, offset));
    }
    for (int i = 0; i < curveCount; i++) {
        CurveId cid = nextCurveId;
        nextCurveId.id++;
        offset = fileReader->read<long int>();
        curveOffsets.insert(make_pair(cid, offset));
    }
    for (int i = 0; i < areaCount; i++) {
        AreaId aid = nextAreaId;
        nextAreaId.id++;
        offset = fileReader->read<long int>();
        areaOffsets.insert(make_pair(aid, offset));
    }
    for (int i = 0; i < subgraphCount; i++) {
        AreaId aid;
        aid.id = fileReader->read<int>();
        offset = fileReader->read<long int>();
        subgraphOffsets.insert(make_pair(aid, offset));
    }
}

void LazyGraph::load(FileReader *fileReader, bool loadsubgraphs)
{
}

}
