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

#include "proland/rivers/graph/HydroGraph.h"

#include "ork/core/Logger.h"
#include "proland/graph/CurvePart.h"

namespace proland
{

HydroGraph::HydroGraph()
{
}

HydroGraph::~HydroGraph()
{
}

CurvePtr HydroGraph::newCurve(CurvePtr parent, bool setParent)
{
    CurvePtr c = new HydroCurve(this);
    if (setParent) {
        c->setParent(parent);
        curves.insert(make_pair(c->getParentId(), c));
    } else {
        curves.insert(make_pair(c->getId(), c));
    }
    return c;
}

CurvePtr HydroGraph::newCurve(CurvePtr model, NodePtr start, NodePtr end)
{
    CurvePtr c = new HydroCurve(this, model, start, end);
    start->addCurve(c->getId());
    end->addCurve(c->getId());
    curves.insert(make_pair(c->getId(), c));
    return c;
}

void HydroGraph::movePoint(CurvePtr c, int i, const vec2d &p, set<CurveId> &changedCurves)
{
    Graph::movePoint(c, i, p, changedCurves);
    // TODO CHANGE THE SIZE OF A RIVER AXIS CURVE WHEN MOVING A BANK
//    if (c->getType() == HydroCurve::BANK) {
//        CurvePtr h = c.cast<HydroCurve>()->getRiverPtr();
//        float w = h->getWidth();
//        float minDistance = 1e8;
//        float maxDistance = 0.f;
//        vec2f prev = h->getXY(0);
//        vec2f cur;
//        for (int j = 1; j < h->getSize(); j++) {
//            cur = h->getXY(j);
//            float d = seg2f(prev, cur).segmentDistSq(p);
//            if (d < minDistance) {
//                minDistance = d;
//            }
//            if (d > maxDistance) {
//                maxDistance = d;
//            }
//            prev = cur;
//        }
//        if (w < minDistance) {
//            h->setWidth(ceil(minDistance));
//            changedCurves.insert(h->getId());
//        }
//    }
}

NodePtr HydroGraph::addNode(CurvePtr c, int i, Graph::Changes &changed)
{
    CurveId riverId = c.cast<HydroCurve>()->getRiver();
    float potential = c.cast<HydroCurve>()->getPotential();
    NodePtr n = Graph::addNode(c, i, changed);
    for (int i = 0; i < 2; i++) {
        CurvePtr cc = n->getCurve(i);
        if (!(cc->getId() == c->getId())) {
            cc.cast<HydroCurve>()->setRiver(riverId);
            cc.cast<HydroCurve>()->setPotential(potential);
        }
    }
    return n;
}

Graph *HydroGraph::createChild()
{
    return new HydroGraph();
}

CurvePtr HydroGraph::addCurvePart(CurvePart &cp, set<CurveId> *addedCurves, bool setParent)
{
    vec2d v = cp.getXY(0);
    NodePtr start = findNode(v);
    if (start == NULL) {
        start = newNode(v);
    }
    v = cp.getXY(cp.getEnd());
    NodePtr end = findNode(v);
    if (end == NULL) {
        end = newNode(v);
    }

    for (int i = 0; i < start->getCurveCount(); ++i) {
        CurvePtr c = start->getCurve(i);
        if (c->getOpposite(start) == end) {
            if ((cp.getId().id != NULL_ID && c->getParentId() == cp.getId()) || cp.equals(c)) {
                return c;
            }
        }
    }
    ptr<HydroCurve> c = NULL;

    if (cp.getId().id != NULL_ID){
        ptr<HydroCurve> parent = cp.getCurve().cast<HydroCurve>();
        c = newCurve(parent, setParent).cast<HydroCurve>();
        c->setPotential(parent->getPotential());
        c->setRiver(parent->getRiver());
    } else {
        c = newCurve(NULL, false).cast<HydroCurve>();
    }
    c->setWidth(cp.getWidth());
    c->setType(cp.getType());

    if (cp.getS(0) < cp.getS(cp.getEnd())) {
        c->addVertex(start->getId());
        c->setS(0, cp.getS(0));
        for (int i = 1; i < cp.getEnd(); ++i) {
            c->addVertex(cp.getXY(i), cp.getS(i), -1, cp.getIsControl(i));
        }
        c->addVertex(end->getId());
        c->setS(c->getSize() - 1, cp.getS(cp.getEnd()));
    } else {
        c->addVertex(end->getId());
        c->setS(0, cp.getS(cp.getEnd()));
        for (int i = cp.getEnd() - 1; i >= 1; --i) {
            c->addVertex(cp.getXY(i), cp.getS(i), -1, cp.getIsControl(i));
        }
        c->addVertex(start->getId());
        c->setS(c->getSize() - 1, cp.getS(0));
    }
    start->addCurve(c->getId());
    end->addCurve(c->getId());

    if (addedCurves != NULL) {
        addedCurves->insert(c->getId());
    }
    return c;
}

void HydroGraph::load(FileReader * fileReader, bool loadSubgraphs)
{
    assert(fileReader != 0);

    int nodeCount;
    int curveCount;
    int areaCount;

    nParamsNodes = fileReader->read<int>();
    nParamsCurves = fileReader->read<int>();
    nParamsAreas = fileReader->read<int>();
    nParamsCurveExtremities = fileReader->read<int>();
    nParamsCurvePoints = fileReader->read<int>();
    nParamsAreaCurves = fileReader->read<int>();
    nParamsSubgraphs = fileReader->read<int>();

    checkParams(nParamsNodes, nParamsCurves, nParamsAreas, nParamsCurveExtremities, nParamsCurvePoints, nParamsAreaCurves, nParamsSubgraphs);

    nodeCount = fileReader->read<int>();
    vector<NodePtr> nodesTMP(nodeCount);
    for (int i = 0; i < nodeCount; i++) {
        float x, y;
        x = fileReader->read<float>();
        y = fileReader->read<float>();
        for (int k = 2; k < nParamsNodes; k++) {
            fileReader->read<float>();
        }
        nodesTMP[i] = newNode(vec2d(x, y));
        int size = fileReader->read<int>();
        for (int i = 0; i < size; i++) {
            fileReader->read<int>();
        }
    }

    curveCount = fileReader->read<int>();
    vector<CurvePtr> curvesTMP(curveCount);
    CurveId nullCid;
    nullCid.id = NULL_ID;

    int riverId;
    map<ptr<HydroCurve>, int> associatedRivers;

    for (int i = 0; i < curveCount; i++) {
        int size;
        float width;
        int type;
        float potential = -1.f;
        size = fileReader->read<int>();
        width = fileReader->read<float>();
        type = fileReader->read<int>();
        if (nParamsCurves >= 5) {
            potential = fileReader->read<float>();
            riverId = fileReader->read<int>();
        } else {
            potential = -1.f;
            riverId = -1;
        }
        for (int j = 5; j < nParamsCurves; j++) {
            fileReader->read<float>();
        }
        for (int k = 5; k < nParamsCurves; k++) {
            fileReader->read<float>();
        }

        int start = fileReader->read<int>();
        for (int k = 1; k < nParamsCurveExtremities; k++) {
            fileReader->read<float>();
        }
        vector<Vertex> v;
        for (int j = 1; j < size - 1; j++) {
            float x, y;
            int isControl;
            x = fileReader->read<float>();
            y = fileReader->read<float>();
            isControl = fileReader->read<int>();
            v.push_back(Vertex(x, y, -1, isControl == 1));
            for (int k = 3; k < nParamsCurvePoints; k++) {
                fileReader->read<float>();
            }
        }
        int end = fileReader->read<int>();
        for (int k = 1; k < nParamsCurveExtremities; k++) {
            fileReader->read<float>();
        }

        fileReader->read<int>();
        fileReader->read<int>();
        CurveId parentId;
        parentId.id= (unsigned int) fileReader->read<int>();

        ptr<HydroCurve> c;
        if (getParent() != NULL) {
            c = newCurve(getParent()->getCurve(parentId), parentId.id != NULL_ID).cast<HydroCurve>();
        } else {
            c = newCurve(NULL, false).cast<HydroCurve>();
        }
        c->setWidth(width);
        c->setType(type);
        c->addVertex(nodesTMP[start]->getId());
        c->addVertex(nodesTMP[end]->getId());
        c->setPotential(potential);
        associatedRivers.insert(make_pair(c, riverId));
        nodesTMP[start]->addCurve(c->getId());
        nodesTMP[end]->addCurve(c->getId());
        for (vector<Vertex>::iterator j = v.begin(); j != v.end(); j++) {
            c->addVertex(*j);
        }

        c->computeCurvilinearCoordinates();
        curvesTMP[i] = c;
    }
    for (map<ptr<HydroCurve>, int> ::iterator i = associatedRivers.begin(); i != associatedRivers.end(); i++) {
        CurveId river;
        if (i->second == -1) {
            river.id = NULL_ID;
        } else {
            river = curvesTMP[i->second]->getId();
        }
        i->first->setRiver(river);
    }

    areaCount = fileReader->read<int>();
    vector<AreaPtr> areasTMP(areaCount);
    for (int i = 0; i < areaCount; i++) {
        int size;
        int info;
        int subgraph;
        size = fileReader->read<int>();
        info = fileReader->read<int>();
        subgraph = fileReader->read<int>();
        for (int k = 3; k < nParamsAreas; k++) {
            fileReader->read<float>();
        }

        vector<pair<int, int> > v;
        for (int j = 0; j < size; j++) {
            int index;
            int orientation;
            index = fileReader->read<int>();
            orientation = fileReader->read<int>();
            for (int k = 2; k < nParamsAreaCurves; k++) {
                fileReader->read<float>();
            }
            v.push_back(make_pair(index, orientation));
        }
        AreaId parentId;
        for (int k = 0; k < nParamsSubgraphs; k++) {
            fileReader->read<float>();
        }
        parentId.id= (unsigned int) fileReader->read<int>();

        AreaPtr a;
        if (getParent() != NULL) {
            a = newArea(getParent()->getArea(parentId), parentId.id != NULL_ID);
        } else {
            a = newArea(NULL, false);
        }
        a->setInfo(info);
        a->setSubgraph(loadSubgraphs ? (subgraph == 1 ? createChild() : NULL) : NULL);
        for (vector<pair<int, int> >::iterator j = v.begin(); j != v.end(); j++) {
            a->addCurve(curvesTMP[j->first]->getId(), j->second);
            curvesTMP[j->first]->addArea(a->getId());
        }

        areasTMP[i] = a;
    }
    for (int i = 0; i < areaCount; ++i) {
        AreaPtr a = areasTMP[i];
        if (a->getSubgraph() != NULL) {
            a->getSubgraph()->load(fileReader, loadSubgraphs);
        }
    }

    curvesTMP.clear();
}

void HydroGraph::loadIndexed(FileReader *fileReader, bool loadSubgraphs)
{
    assert(fileReader != 0);
    int nodeCount;
    int curveCount;
    int areaCount;
    int subgraphCount;
    //long int offset, begin;

//    offset = fileReader->read<long int>();
//    begin = fileReader->tellg();
//    fileReader->seekg(offset, ios::beg);
    nParamsNodes = fileReader->read<int>();
    nParamsCurves = fileReader->read<int>();
    nParamsAreas = fileReader->read<int>();
    nParamsCurveExtremities = fileReader->read<int>();
    nParamsCurvePoints = fileReader->read<int>();
    nParamsAreaCurves = fileReader->read<int>();
    nParamsSubgraphs = fileReader->read<int>();

    checkParams(nParamsNodes, nParamsCurves, nParamsAreas, nParamsCurveExtremities, nParamsCurvePoints, nParamsAreaCurves, nParamsSubgraphs);
    long int offset = fileReader->read<long int>();
    long int begin = fileReader->tellg();

    fileReader->seekg(offset, ios::beg);
    nodeCount = fileReader->read<int>();
    curveCount = fileReader->read<int>();
    areaCount = fileReader->read<int>();
    subgraphCount = fileReader->read<int>();


    fileReader->seekg(begin, ios::beg);

    vector<NodePtr> nodesTMP(nodeCount);
    for (int i = 0; i < nodeCount; i++) {
        float x, y;
        x = fileReader->read<float>();
        y = fileReader->read<float>();
        nodesTMP[i] = newNode(vec2d(x, y));
        for (int j = 2; j < nParamsNodes; j++) {
            fileReader->read<float>();
        }
        int size = fileReader->read<int>();
        for (int j = 0; j < size; j++) {
            fileReader->read<int>();
        }
    }

    CurveId nullCid;
    nullCid.id = NULL_ID;
    vector<CurvePtr> curvesTMP(curveCount);
    map<ptr<HydroCurve>, int> associatedRivers;
    // For each curve that has a non-null river parameter, we store it and add it at the end of the loading part.
    // This is used when a curve references another curve that is defined later in the file.
    for (int i = 0; i < curveCount; i++) {
        int size;
        float width;
        int type;
        float potential;
        int riverId;
        size = fileReader->read<int>();
        width = fileReader->read<float>();
        type = fileReader->read<int>();
        if (nParamsCurves >= 5) {
            potential = fileReader->read<float>();
            riverId = fileReader->read<int>();
        } else {
            potential = -1.f;
            riverId = -1;
        }
        for (int j = 5; j < nParamsCurves; j++) {
            fileReader->read<float>();
        }

        int start = fileReader->read<int>();
        for (int j = 1; j < nParamsCurveExtremities; j++) {
            fileReader->read<float>();
        }
        vector<Vertex> v;
        for (int j = 1; j < size - 1; j++) {
            float x, y;
            int isControl;
            x = fileReader->read<float>();
            y = fileReader->read<float>();
            isControl = fileReader->read<int>();

            for (int k = 3; k < nParamsCurvePoints; k++) {
                fileReader->read<float>();
            }
            v.push_back(Vertex(x, y, -1, isControl == 1));
        }
        int end = fileReader->read<int>();
        for (int j = 1; j < nParamsCurveExtremities; j++) {
            fileReader->read<float>();
        }
        fileReader->read<int>();
        fileReader->read<int>();
        CurveId parentId;
        parentId.id= (unsigned int) fileReader->read<int>();


        ptr<HydroCurve> c;
        if (getParent() != NULL) {
            c = newCurve(getParent()->getCurve(parentId), parentId.id != NULL_ID).cast<HydroCurve>();
        } else {
            c = newCurve(NULL, false).cast<HydroCurve>();
        }
        c->setWidth(width);
        c->setType(type);
        associatedRivers.insert(make_pair(c, riverId));
        c->setPotential(potential);
        c->addVertex(nodesTMP[start]->getId());
        c->addVertex(nodesTMP[end]->getId());
        nodesTMP[start]->addCurve(c->getId());
        nodesTMP[end]->addCurve(c->getId());
        for (vector<Vertex>::iterator j = v.begin(); j != v.end(); j++) {
            c->addVertex(*j);
        }

        c->computeCurvilinearCoordinates();
        curvesTMP[i] = c;
    }
    for (map<ptr<HydroCurve>, int> ::iterator i = associatedRivers.begin(); i != associatedRivers.end(); i++) {
        CurveId river;
        if (i->second == -1) {
            river.id = NULL_ID;
        } else {
            river = curvesTMP[i->second]->getId();
        }
        i->first->setRiver(river);
    }

    vector<AreaPtr> areasTMP(areaCount);
    for (int i = 0; i < areaCount; i++) {
        int size;
        int info;
        int subgraph;
        size = fileReader->read<int>();
        info = fileReader->read<int>();
        subgraph = fileReader->read<int>();
        for (int j = 3; j < nParamsAreas; j++) {
            fileReader->read<float>();
        }

        vector<pair<int, int> > v;
        for (int j = 0; j < size; j++) {
            int index;
            int orientation;
            index = fileReader->read<int>();
            orientation = fileReader->read<int>();

            for (int k = 2; k < nParamsAreaCurves; k++) {
                fileReader->read<float>();
            }
            v.push_back(make_pair(index, orientation));
        }
        AreaId parentId;
        for (int j = 0; j < nParamsSubgraphs; j++) {
            fileReader->read<float>();
        }
        parentId.id= (unsigned int) fileReader->read<int>();

        AreaPtr a;
        if (getParent() != NULL) {
            a = newArea(getParent()->getArea(parentId), parentId.id != NULL_ID);
        } else {
            a = newArea(NULL, false);
        }
        a->setInfo(info);
        a->setSubgraph(loadSubgraphs ? (subgraph == 1 ? createChild() : NULL) : NULL);
        for (vector<pair<int, int> >::iterator j = v.begin(); j != v.end(); j++) {
            a->addCurve(curvesTMP[j->first]->getId(), j->second);
            curvesTMP[j->first]->addArea(a->getId());
        }

        areasTMP[i] = a;
    }

    for (int i = 0; i < areaCount; ++i) {
        AreaPtr a = areasTMP[i];
        if (a->getSubgraph() != NULL) {
            a->getSubgraph()->load(fileReader, loadSubgraphs);
        }
    }
}

void HydroGraph::save(Graph *graph, FileWriter * fileWriter, bool saveAreas)
{
    assert(fileWriter != NULL);

    fileWriter->write(2); //default nParamsNodes -> doesn't count the number of potential curves and the associated count
    fileWriter->write(5); //default nParamsCurves
    fileWriter->write(3); //default nParamsAreas
    fileWriter->write(1); //default nParamsCurveExtremities
    fileWriter->write(3); //default nParamsCurvePoints
    fileWriter->write(2); //default nParamsAreaCurves
    fileWriter->write(0); //default nParamsSubgraphs

    map<NodePtr, int> nindices;
    map<CurvePtr, int> cindices;
    map<AreaPtr, int> aindices;
    vector<NodePtr> nodeList;
    vector<CurvePtr > curveList;
    vector<AreaPtr> areaList;

    int index = 0;
    ptr<Graph::NodeIterator> ni = graph->getNodes();
    while (ni->hasNext()) {
        NodePtr n = ni->next();
        nodeList.push_back(n);
        nindices[n] = index++;
    }

    index = 0;
    ptr<Graph::CurveIterator> ci = graph->getCurves();
    while (ci->hasNext()) {
        ptr<HydroCurve> c = ci->next().cast<HydroCurve>();
        if (c->getRiver().id != NULL_ID) {
            continue;
        }
        curveList.push_back(c);
        cindices[c->getAncestor()] = index;
        cindices[c] = index++;
    }
    ci = graph->getCurves();
    while (ci->hasNext()) {
        ptr<HydroCurve> c = ci->next().cast<HydroCurve>();
        if (c->getRiver().id == NULL_ID) {
            continue;
        }
        curveList.push_back(c);
        cindices[c] = index++;
    }

    cindices[NULL] = -1;

    index = 0;
    ptr<Graph::AreaIterator> ai = graph->getAreas();
    while (ai->hasNext()) {
        AreaPtr a = ai->next();
        areaList.push_back(a);
        aindices[a] = index++;
    }

    //Saving nodes
    fileWriter->write(graph->getNodeCount());
    for (int i = 0; i < (int) nodeList.size(); i++) {
    //for (map<NodePtr, int>::iterator i = nindices.begin(); i != nindices.end(); i++) {
        NodePtr n = nodeList[i];
        fileWriter->write(n->getPos().x);
        fileWriter->write(n->getPos().y);
        fileWriter->write(n->getCurveCount());
        for (int j = 0; j < n->getCurveCount(); j++) {
            fileWriter->write(cindices[n->getCurve(j).cast<HydroCurve>()]);
        }
    }
    //Saving curves
    fileWriter->write(graph->getCurveCount());

    for (int i = 0; i < (int) curveList.size(); i++) {
    //for (map<CurvePtr, int>::iterator i = cindices.begin(); i != cindices.end(); i++) {
        ptr<HydroCurve> c = curveList[i].cast<HydroCurve>();
        fileWriter->write(c->getSize());
        fileWriter->write(c->getWidth());
        fileWriter->write(c->getType());
        fileWriter->write(c->getPotential());
        fileWriter->write(cindices[c->getOwner()->getAncestor()->getCurve(c->getRiver())]);
        int s = nindices[c->getStart()];
        int e = nindices[c->getEnd()];
        fileWriter->write(s);
        for (int j = 1; j < c->getSize() - 1; ++j) {
            fileWriter->write(c->getXY(j).x);
            fileWriter->write(c->getXY(j).y);
            fileWriter->write(c->getIsControl(j) ? 1 : 0);
        }
        fileWriter->write(e);
        fileWriter->write(c->getArea1() == NULL ? -1 : aindices[c->getArea1()]);
        fileWriter->write(c->getArea2() == NULL ? -1 : aindices[c->getArea2()]);
        fileWriter->write(c->getAncestor() == c ? (int) -1 : (int) c->getAncestor()->getId().id);
    }

    //Saving areas
    fileWriter->write(graph->getAreaCount());
    for (int i = 0; i < (int) areaList.size(); i++) {
    //for (map<AreaPtr, int>::iterator i = aindices.begin(); i != aindices.end(); i++) {
        AreaPtr a = areaList[i];
        fileWriter->write(a->getCurveCount());
        fileWriter->write(a->getInfo());
        if (saveAreas) {
            fileWriter->write(a->getSubgraph() == NULL ? 0 : 1);
        } else {
            fileWriter->write(0);
        }
       for (int j = 0; j < a->getCurveCount(); ++j) {
            int o;
            CurvePtr c = a->getCurve(j, o);
            fileWriter->write(cindices[c]);
            fileWriter->write(o);
        }
        fileWriter->write(a->getAncestor() == a ? (int)-1 : (int) a->getAncestor()->getId().id);
    }
    if (saveAreas) {
        for (int i = 0; i < (int) areaList.size(); i++) {
        //for (map<AreaPtr, int>::iterator i = aindices.begin(); i != aindices.end(); i++) {
            AreaPtr a = areaList[i];
            GraphPtr sg = a->getSubgraph();
            if (sg != NULL) {
                sg->save(fileWriter, saveAreas);
            }
        }
    }
}

void HydroGraph::indexedSave(Graph *graph, FileWriter * fileWriter, bool saveAreas)
{
    assert(fileWriter != NULL);
    //long int offset = 0;
    //long int subgraphCountoffset  = 0;
    vector<long int> offsets;

    map<NodePtr, int> nindices;
    map<CurvePtr, int> cindices;
    map<AreaPtr, int> aindices;

    vector<NodePtr> nodeList;
    vector<CurvePtr > curveList;
    vector<AreaPtr> areaList;

    fileWriter->write(2); //default nParamsNodes -> doesn't count the number of potential curves and the associated count
    fileWriter->write(5); //default nParamsCurves
    fileWriter->write(3); //default nParamsAreas
    fileWriter->write(1); //default nParamsCurveExtremities
    fileWriter->write(3); //default nParamsCurvePoints
    fileWriter->write(2); //default nParamsAreaCurves
    fileWriter->write(0); //default nParamsSubgraphs

    int index = 0;
    ptr<Graph::NodeIterator> ni = graph->getNodes();
    while (ni->hasNext()) {
        NodePtr n = ni->next();
        nodeList.push_back(n);
        nindices[n] = index++;
    }

    index = 0;
    ptr<Graph::CurveIterator> ci = graph->getCurves();
    while (ci->hasNext()) {
        CurvePtr c = ci->next();
        if (c->getType() == HydroCurve::BANK) {
            continue;
        }
        curveList.push_back(c);
        cindices[c->getAncestor()] = index;
        cindices[c] = index++;
    }

    ci = graph->getCurves();
    while (ci->hasNext()) {
        CurvePtr c = ci->next();
        if (c->getType() != HydroCurve::BANK) {
            continue;
        }
        curveList.push_back(c);
        cindices[c] = index++;
    }
    cindices[NULL] = -1;

    index = 0;
    ptr<Graph::AreaIterator> ai = graph->getAreas();
    while (ai->hasNext()) {
        AreaPtr a = ai->next();
        areaList.push_back(a);
        aindices[a] = index++;
    }

    long int offsetInit = fileWriter->tellp();

    //fileWriter->seekp(offsetInit, ios::beg);
    fileWriter->width(10);
    fileWriter->write(offsetInit);
    fileWriter->width(1);

    for (int i = 0; i < (int) nodeList.size(); i++) {
    //for (map<NodePtr, int>::iterator i = nindices.begin(); i != nindices.end(); i++) {
        NodePtr n = nodeList[i];
        offsets.push_back(fileWriter->tellp());
        fileWriter->write(n->getPos().x);
        fileWriter->write(n->getPos().y);
        fileWriter->write(n->getCurveCount());
        for (int j = 0; j < n->getCurveCount(); j++) {
            fileWriter->write(cindices[n->getCurve(j)]);
        }
    }


    for (int i = 0; i < (int) curveList.size(); i++) {
    //for (map<CurvePtr, int>::iterator i = cindices.begin(); i != cindices.end(); i++) {
        ptr<HydroCurve> c = curveList[i].cast<HydroCurve>();
        offsets.push_back(fileWriter->tellp());
        fileWriter->write(c->getSize());
        fileWriter->write(c->getWidth());
        fileWriter->write(c->getType());
        fileWriter->write(c->getPotential());
        fileWriter->write(cindices[c->getOwner()->getAncestor()->getCurve(c->getRiver())]);

        int s = nindices[c->getStart()];
        int e = nindices[c->getEnd()];
        fileWriter->write(s);
        for (int j = 1; j < c->getSize() - 1; ++j) {
            fileWriter->write(c->getXY(j).x);
            fileWriter->write(c->getXY(j).y);
            fileWriter->write(c->getIsControl(j) ? 1 : 0);
        }
        fileWriter->write(e);
        fileWriter->write(c->getArea1() == NULL ? -1 : aindices[c->getArea1()]);
        fileWriter->write(c->getArea2() == NULL ? -1 : aindices[c->getArea2()]);
        fileWriter->write(c->getAncestor() == c ? (int)-1 : (int) c->getAncestor()->getId().id);
    }


    for (int i = 0; i < (int) areaList.size(); i++) {
    //for (map<AreaPtr, int>::iterator i = aindices.begin(); i != aindices.end(); i++) {
        AreaPtr a = areaList[i];
        offsets.push_back(fileWriter->tellp());
        fileWriter->write(a->getCurveCount());
        fileWriter->write(a->getInfo());
        if (saveAreas) {
            fileWriter->write(a->getSubgraph() == NULL ? 0 : 1);
        } else {
            fileWriter->write(0);
        }
        for (int j = 0; j < a->getCurveCount(); ++j) {
            int o;
            CurvePtr c = a->getCurve(j, o);
            fileWriter->write(cindices[c]);
            fileWriter->write(o);
        }
        fileWriter->write(a->getAncestor() == a ? (int)-1 : (int) a->getAncestor()->getId().id);
    }

    map<int, long int> graphOffsets;

    int subgraphCount = 0;
    if (saveAreas) {
        for (int i = 0; i < (int) areaList.size(); i++) {
        //for (map<AreaPtr, int>::iterator i = aindices.begin(); i != aindices.end(); i++) {
            AreaPtr a = areaList[i];
            GraphPtr sg = a->getSubgraph();
            if (sg != NULL) {
                subgraphCount++;
                graphOffsets[aindices[a]]=fileWriter->tellp();
                sg->save(fileWriter, true);
            }
        }
    }
    long int indexOffset = fileWriter->tellp();
    fileWriter->write(graph->getNodeCount());
    fileWriter->write(graph->getCurveCount());
    fileWriter->write(graph->getAreaCount());
    fileWriter->write(subgraphCount);

    for (int i = 0; i < (int) offsets.size(); i++) {
        fileWriter->write(offsets[i]);
    }
    map<int, long int>::iterator it = graphOffsets.begin();
    while (it != graphOffsets.end()) {
        fileWriter->write(it->first);
        fileWriter->write(it->second);
        it++;
    }
    fileWriter->seekp(offsetInit, ios::beg);
    fileWriter->width(10);
    fileWriter->write(indexOffset);
}

void HydroGraph::save(FileWriter *fileWriter, bool saveAreas)
{
    HydroGraph::save(this, fileWriter, saveAreas);
}

void HydroGraph::indexedSave(FileWriter *fileWriter, bool saveAreas)
{
    HydroGraph::indexedSave(this, fileWriter, saveAreas);
}

void HydroGraph::checkParams(int nodes, int curves, int areas, int curveExtremities, int curvePoints, int areaCurves, int subgraphs)
{
    if (nodes < 2 || curves < 5 || areas < 3 || curveExtremities < 1 || curvePoints < 3 || areaCurves < 2) {
        Graph::checkDefaultParams(nodes, curves, areas, curveExtremities, curvePoints, areaCurves, subgraphs);
        if (Logger::ERROR_LOGGER != NULL) {
            Logger::ERROR_LOGGER->log("RIVER", "Can't load file : Graph is not a HydroGraph. It will be considered as a basic Graph");
        }
        return;
    }
}

void HydroGraph::print(bool detailed)
{
    printf("Areas %d\n", getAreaCount());
    if (detailed) {
        ptr<Graph::AreaIterator> ai = getAreas();
        while (ai->hasNext()) {
            AreaPtr a = ai->next();
            printf("%d %d %d %d : ", a->getId().id, a->getCurveCount(), a->getInfo(), a->getSubgraph() == NULL ? 0 : 1);
            for (int i = 0; i < a->getCurveCount(); i++) {
                int orientation;
                CurveId id = a->getCurve(i, orientation)->getId();
                printf("%d:%d ", id.id, orientation);
            }
            printf("\n");
            if (a->getSubgraph() != NULL) {
                a->getSubgraph()->print(detailed);
            }
        }
    }

    printf("Curves %d\n", getCurveCount());
    if (detailed) {
        ptr<Graph::CurveIterator> ci = getCurves();
        while (ci->hasNext()) {
            ptr<HydroCurve> c = ci->next().cast<HydroCurve>();
            //ptr<HydroCurve> c = ci->next().cast<HydroCurve>();
            int cpt = c->getArea2() == NULL ? c->getArea1() == NULL ? 0 : 1 : 2;
            vec2d v1 = c->getXY(0), v2 = c->getXY(c->getSize() - 1);
            printf("%d %d %d %f %d %f:%f -> %f:%f  (%d:%d) [%f][%d]\n", c->getId().id, cpt, c->getSize(), c->getWidth(), c->getType(), v1.x, v1.y, v2.x, v2.y, c->getStart()->getId().id, c->getEnd()->getId().id, c->getPotential(), c->getRiver().id);
        }
    }

    printf("Nodes %d\n", getNodeCount());
    if (detailed) {
        ptr<Graph::NodeIterator> ni = getNodes();
        while (ni->hasNext()) {
            NodePtr n = ni->next();
            vec2d v = n->getPos();
            printf("%d %d %f:%f (", n->getId().id, n->getCurveCount(), v.x, v.y);
            for (int i = 0; i < n->getCurveCount(); i++) {
                printf("%d,", n->getCurve(i)->getId().id);
            }
            printf(")\n");
        }
    }
}

}
