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

// fitCubicCurve code adapted from:
/*
An Algorithm for Automatically Fitting Digitized Curves
by Philip J. Schneider
from "Graphics Gems", Academic Press, 1990
*/
// See http://tog.acm.org/resources/GraphicsGems/gems/FitCurves.c
//
// Quote from http://tog.acm.org/resources/GraphicsGems/:
// "The Graphics Gems code is copyright-protected. In other words, you
// cannot claim the text of the code as your own and resell it. Using
// the code is permitted in any program, product, or library,
// non-commercial or commercial. Giving credit is not required, though
// is a nice gesture. The code comes as-is, and if there are any flaws
// or problems with any Gems code, nobody involved with Gems - authors,
// editors, publishers, or webmasters - are to be held responsible.
// Basically, don't be a jerk, and remember that anything free comes
// with no guarantee.

#include "ork/core/Logger.h"

#include <set>
#include <iterator>

#include "proland/graph/Area.h"
#include "proland/graph/Margin.h"
#include "proland/graph/BasicCurvePart.h"
#include "proland/graph/BasicGraph.h"
#include "proland/graph/GraphListener.h"

namespace proland
{

Graph::Graph() :
    Object("Graph"), parent(NULL)
{
    mapping = new map<vec2d, Node*, Cmp> ();
    version = 0;
}

Graph::~Graph()
{
    if (mapping != NULL) {
        delete mapping;
    }

    listeners.clear();
}

bool Graph::Changes::empty()
{
    return removedCurves.empty() && addedCurves.empty() && removedAreas.empty() && addedAreas.empty();
}

void Graph::Changes::clear()
{
    changedArea.clear();
    removedCurves.clear();
    removedAreas.clear();
    addedCurves.clear();
    addedAreas.clear();
}

void Graph::Changes::insert(Graph::Changes c)
{
    removedCurves.insert(c.removedCurves.begin(), c.removedCurves.end());
    addedCurves.insert(c.addedCurves.begin(), c.addedCurves.end());
    removedAreas.insert(c.removedAreas.begin(), c.removedAreas.end());
    addedAreas.insert(c.addedAreas.begin(), c.addedAreas.end());
    if ((int)changedArea.size() == 0) {
        changedArea = c.changedArea;
    }
}

bool Graph::Changes::equals(Graph::Changes c) const
{
    if (removedCurves.size() != c.removedCurves.size() || removedAreas.size() != c.removedAreas.size() || addedCurves.size() != c.addedCurves.size() || addedAreas.size() != c.addedAreas.size()) {
        return false;
    }

    list<AreaId>::const_iterator j = c.changedArea.begin();
    for (list<AreaId>::const_iterator i = changedArea.begin(); i != changedArea.end(); i++) {
        if (!(*i == *j)) {
            return false;
        }
        j++;
    }

    for (set<CurveId>::const_iterator i = addedCurves.begin(); i != addedCurves.end(); i++) {
        if (c.addedCurves.find(*i) == c.addedCurves.end()) {
            return false;
        }
    }
    for (set<CurveId>::const_iterator i = removedCurves.begin(); i != removedCurves.end(); i++) {
        if (c.removedCurves.find(*i) == c.removedCurves.end()) {
            return false;
        }
    }
    for (set<AreaId>::const_iterator i = removedAreas.begin(); i != removedAreas.end(); i++) {
        if (c.removedAreas.find(*i) == c.removedAreas.end()) {
            return false;
        }
    }
    for (set<AreaId>::const_iterator i = addedAreas.begin(); i != addedAreas.end(); i++) {
        if (c.addedAreas.find(*i) == c.addedAreas.end()) {
            return false;
        }
    }


    return true;
}


void Graph::Changes::print() const
{
    printf("Added Curves :\n");
    for (set<CurveId>::const_iterator i = addedCurves.begin(); i != addedCurves.end(); i++) {
        printf("%d ", i->id);
    }
    printf("\nRemoved Curves :\n");
    for (set<CurveId>::const_iterator i = removedCurves.begin(); i != removedCurves.end(); i++) {
        printf("%d ", i->id);
    }
    printf("\nAdded Areas :\n");
    for (set<AreaId>::const_iterator i = addedAreas.begin(); i != addedAreas.end(); i++) {
        printf("%d ", i->id);
    }
    printf("\nRemoved Areas :\n");
    for (set<AreaId>::const_iterator i = removedAreas.begin(); i != removedAreas.end(); i++) {
        printf("%d ", i->id);
    }
    printf("\nChanged Areas :\n");
    for (list<AreaId>::const_iterator i = changedArea.begin(); i != changedArea.end(); i++) {
        printf("->%d", i->id);
    }
    printf("\n");
}

void Graph::print(bool detailed)
{
    printf("Areas %d\n", getAreaCount());
    if ( detailed ) {
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
    if ( detailed ) {
        ptr<Graph::CurveIterator> ci = getCurves();
        while (ci->hasNext()) {
            CurvePtr c = ci->next();
            int cpt = c->getArea2() == NULL ? c->getArea1() == NULL ? 0 : 1 : 2;
            vec2d v1 = c->getXY(0), v2 = c->getXY(c->getSize() - 1);
            printf("%d %d %d %f %d %f:%f -> %f:%f  (%d:%d)\n", c->getId().id, cpt, c->getSize(), c->getWidth(), c->getType(), v1.x, v1.y, v2.x, v2.y, c->getStart()->getId().id, c->getEnd()->getId().id);
        }
    }

    printf("Nodes %d\n", getNodeCount());
    if ( detailed ) {
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

void Graph::setParent(Graph* p)
{
    assert(p != this);
    parent = p;
}

// ---------------------------------------------------------------------------
// FLATTENING
// ---------------------------------------------------------------------------

void Graph::flatten(float squareFlatness)
{
    ptr<CurveIterator> ci = getCurves();
    while (ci->hasNext()) {
        ci->next()->flatten(squareFlatness);
    }
    ptr<AreaIterator> ai = getAreas();
    while (ai->hasNext()) {
        AreaPtr a = ai->next();
        if (a->getSubgraph() != NULL) {
            a->getSubgraph()->flatten(squareFlatness);
        }
    }
}

void Graph::flattenUpdate(const Changes &changes, float squareFlatness)
{
    if ((int) changes.changedArea.size() > 0) {
        Changes c = changes;
        AreaId a = *(c.changedArea.begin());
        c.changedArea.pop_front();
        return getArea(a)->getSubgraph()->flattenUpdate(c, squareFlatness);
    }
    set<CurveId>::const_iterator i = changes.addedCurves.begin();
    while (i != changes.addedCurves.end()) {
        CurvePtr c = getCurve(*i);
        c->flatten(squareFlatness);
        ++i;
    }
}


// ---------------------------------------------------------------------------
// EDIT
// ---------------------------------------------------------------------------

struct Change
{
    CurveId id;
    int i;
    vec2d p;

    Change(CurveId id, int i, const vec2d &p) : id(id), i(i), p(p)
    {
    }
};

Node *Graph::findNode(vec2d &pos) const
{
    map<vec2d, Node*, Cmp>::iterator i = mapping->find(pos);
    if (i != mapping->end()) {
        return i->second;
    } else {
        return NULL;
    }
}

void Graph::movePoint(CurvePtr c, int i, const vec2d &p)
{
    vec2d old = c->getXY(i);
    c->setXY(i, p);
    NodePtr n = NULL;
    if (i == 0) {
        n = c->getStart();
    } else if (i == c->getSize() - 1) {
        n = c->getEnd();
    }
    if (n != NULL && mapping != NULL) {
        map<vec2d, Node*, Graph::Cmp>::iterator i = mapping->find(old);
        if (i != mapping->end()) {
            mapping->erase(i);
        }
        mapping->insert(make_pair(p, n.get()));
    }
}

void Graph::moveNode(NodePtr n, const vec2d &p)
{
    assert(n != NULL);
    assert(mapping != NULL);
    vec2d old = n->getPos();

    map<vec2d, Node*, Graph::Cmp>::iterator i = mapping->find(old);
    assert(i != mapping->end());
    n->setPos(p);
    mapping->erase(i);
    mapping->insert(make_pair(p, n.get()));

    map<vec2d, Node*, Graph::Cmp>::iterator i2 = mapping->find(p);
    assert(i != mapping->end());
}

bool Graph::isOpposite(const vec2d &p, const vec2d &q, const vec2d &r)
{
    return abs(angle(p - q, r - q) - M_PI) < 0.01;
}

bool Graph::hasOppositeControlPoint(CurvePtr p, int i, int di,
    vec2d &q, CurveId &id, int &j)
{
    if (p->getIsControl(i)) {
        return false;
    }
    j = i + di;
    if (j >= 0 && j < p->getSize()) {
        if (p->getIsControl(j)) {
            q = p->getXY(i);
            id = p->getId();
            return true;
        }
    } else {
        vec2d s = p->getXY(i - di);
        assert(i == 0 || i == p->getSize() - 1);
        NodePtr pt = i == 0 ? p->getStart() : p->getEnd();
        for (int k = 0; k < pt->getCurveCount(); ++k) {
            CurvePtr r = pt->getCurve(k);
            j = r->getStart() == pt ? 1 : r->getSize() - 2;
            if (r->getIsControl(j) && isOpposite(s, pt->getPos(), r->getXY(j))) {
                q = p->getXY(i);
                id = r->getId();
                return true;
            }
            if (r->getStart() == r->getEnd()) {
                j = r->getSize() - 2;
                if (r->getIsControl(j) && isOpposite(s, pt->getPos(), r->getXY(j))) {
                    q = p->getXY(i);
                    id = r->getId();
                    return true;
                }
            }
        }
    }
    return false;
}

int Graph::enlarge(box2d &area, CurvePtr p, int i, int di)
{
    while (true) {
        if (i < 0 || i >= p->getSize()) {
            return -1;
        }
        area = area.enlarge(p->getXY(i));
        if (p->getIsControl(i)) {
            i += di;
        } else {
            return i;
        }
    }
}

struct CurveChange
{
    CurveId id;
    int i;
    vec2d p;

    CurveChange(CurveId id, int i, const vec2d &p) : id(id), i(i), p(p)
    {
    }
};

void Graph::movePoint(CurvePtr spath, int spoint, const vec2d &xy,
    set<CurveId> &changedCurves)
{
    vec2d p = spath->getXY(spoint);

    vector<CurveChange> changes;
    changes.push_back(CurveChange(spath->getId(), spoint, xy));

    if (spoint == 0 || spoint == spath->getSize() - 1) {
        NodePtr pt = spoint == 0 ? spath->getStart() : spath->getEnd();
        for (int i = 0; i < pt->getCurveCount(); ++i) {
            CurvePtr pa = pt->getCurve(i);
            vec2d q;
            CurveId id;
            int index;
            if (pa->getStart() == pt) {
                if (hasOppositeControlPoint(pa, 0, -1, q, id, index)) {
                    vec2d r = getCurve(id)->getXY(index);
                    changes.push_back(CurveChange(id, index, r + xy - p));
                }
            }
            if (pa->getEnd() == pt) {
                if (hasOppositeControlPoint(pa, pa->getSize() - 1, 1, q, id, index)) {
                    vec2d r = getCurve(id)->getXY(index);
                    changes.push_back(CurveChange(id, index, r + xy - p));
                }
            }
            changedCurves.insert(pa->getId());
        }
    } else {
        vec2d q;
        CurveId id;
        int index;
        if (spath->getIsControl(spoint)) {
            if (hasOppositeControlPoint(spath, spoint - 1, -1, q, id, index)) {
                changes.push_back(CurveChange(id, index, q * 2 - xy));
            }
            if (hasOppositeControlPoint(spath, spoint + 1, +1, q, id, index)) {
                changes.push_back(CurveChange(id, index, q * 2 - xy));
            }
        } else {
            vec2d prec = spath->getXY(spoint - 1);
            vec2d cur  = spath->getXY(spoint);
            vec2d succ = spath->getXY(spoint + 1);

            float d = ((prec + succ) * 0.5 - cur).squaredLength();
            if (spath->getIsControl(spoint - 1) && spath->getIsControl(spoint + 1) && d < 0.1f) {
                changes.push_back(CurveChange(spath->getId(), spoint - 1, spath->getXY(spoint - 1) + xy - p));
                changes.push_back(CurveChange(spath->getId(), spoint + 1, spath->getXY(spoint + 1) + xy - p));
            }
        }
    }

    for (int i = 0; i < (int) changes.size(); ++i) {
        CurveChange c = changes[i];
        movePoint(getCurve(c.id), c.i, c.p);
        changedCurves.insert(c.id);
    }
}

void Graph::getAreasFromCurves(const set<CurveId> &curves, set<AreaId> &areas)
{
    set<CurveId>::const_iterator i = curves.begin();
    while (i != curves.end()) {
        CurvePtr c = getCurve(*i);
        if (c->area1.id != NULL_ID) {
            areas.insert(c->getArea1()->getId());
            if (c->area2.id != NULL_ID) {
                areas.insert(c->getArea2()->getId());
            }
        }
        ++i;
    }
}

NodePtr Graph::addNode(CurvePtr c, int i, Graph::Changes &changed)
{
    Graph::Changes tmpChanges;
    NodePtr n = newNode(c->getXY(i));

    CurvePtr cc = newCurve(NULL, n, c->getEnd());
    cc->setWidth(c->getWidth());
    cc->setType(c->getType());
    if (c->getEnd() != c->getStart()) {
        c->getEnd()->removeCurve(c->getId());
    }
    c->addVertex(n->getId());
    n->addCurve(c->getId());

    tmpChanges.addedCurves.insert(c->getId());
    tmpChanges.addedCurves.insert(cc->getId());
    tmpChanges.removedCurves.insert(c->getId());


    while (c->getSize() > i + 2) {
        cc->addVertex(c->getVertex(i + 1));
        c->removeVertex(i + 1);
    }
    c->removeVertex(i);
    AreaPtr area1 = c->getArea1();
    AreaPtr area2 = c->getArea2();
    if (area1 != NULL) {

        bool ok1 = false;

        for (int i = 0; i < area1->getCurveCount(); i++) {

            int orientation1;

            if (area1->getCurve(i, orientation1) == c) {
                ok1 = true; // found
                area1->addCurve(cc->getId(), orientation1);
                cc->addArea(area1->getId());
                area1->build();

                tmpChanges.removedAreas.insert(area1->getId());
                tmpChanges.addedAreas.insert(area1->getId());

                if (area2 != NULL) {

                    bool ok2 = false;

                    for (int j = 0; j < area2->getCurveCount(); j++) {

                        int orientation2;

                        if (area2->getCurve(j, orientation2)->getId() == c->getId()) {
                            ok2 = true; // found
                            area2->addCurve(cc->getId(), orientation2);
                            cc->addArea(area2->getId());
                            area2->build();
                            tmpChanges.removedAreas.insert(area2->getId());
                            tmpChanges.addedAreas.insert(area2->getId());
                        }
                    }

                    if (!ok2) assert(false); // curve not found in area2, anormal
                }

                break;
            }
        }

        if (!ok1) assert(false); // curve not found in area1, anormal
    } else {
        assert(area2 == NULL); // area2 shouldn't be NULL if area1 is NULL
    }

    changed = merge(changed, tmpChanges);
    c->computeCurvilinearCoordinates();
    cc->computeCurvilinearCoordinates();

    return n;
}

void Graph::removeVertex(CurvePtr &curve, int &selectedSegment, int &selectedPoint, Graph::Changes &changed)
{
    Graph::Changes tmpChanges;
    CurveId cid = curve->getId();

    vec2d a, b;
    if (curve->getIsSmooth(selectedPoint, a, b)) {
        curve->removeVertex(selectedPoint + 1);
        curve->removeVertex(selectedPoint - 1);
        selectedPoint--;
    }

    int size = curve->getSize();
    if (curve->getSize() > 2) {
        tmpChanges.addedCurves.insert(cid);
    }
    tmpChanges.removedCurves.insert(cid);
    getAreasFromCurves(tmpChanges.addedCurves, tmpChanges.removedAreas);
    NodePtr start = curve->getStart();
    NodePtr end = curve->getEnd();
    bool isLoop = start == end;


    AreaPtr inside = NULL, outside = NULL;
    CurvePtr c = NULL;
    int orientation = 0;
    bool doDeleteCurve = false;

    if (selectedPoint != 0 && selectedPoint != curve->getSize() - 1) { // vertex
        if (size == 4 && isLoop) { // loop curve that will be flat. We verify the 2 possible areas and remake the second one if necessary
            if (curve->getArea1()->getCurveCount() == 1) {
                inside = curve->getArea1();
                outside = curve->getArea2();
            } else {
                inside = curve->getArea2();
                outside = curve->getArea1();
            }
            if (outside != NULL) {
                for (int i = 0; i < outside->getCurveCount(); i++) {
                    if (outside->getCurve(i) == curve) {
                        outside->removeCurve(i);
                        break;
                    }
                }
                curve->removeArea(outside->getId());
                tmpChanges.addedAreas.insert(outside->getId());
                outside->build();
            }
            removeArea(inside->getId());

            NodePtr n = newNode(curve->getXY(selectedPoint == 1 ? 2 : 1));
            curve->removeVertex(2);
            curve->removeVertex(1);
            n->addCurve(cid);
            curve->addVertex(n->getId());
            selectedSegment = -1;
            selectedPoint = -1;
        } else if (curve->getSize() == 3) {
            ptr<Area> a1 = curve->getArea1();
            ptr<Area> a2 = curve->getArea2();
            if (a1 != NULL && a1->getCurveCount() == 2) {
                c = a1->getCurve(0, orientation);
                if (c == curve) {
                    c = a1->getCurve(1, orientation);
                }
                if (c->getSize() == 2) {
                    inside = a1;
                    outside = a2;
                    doDeleteCurve = true;
                }
            }
            if (!doDeleteCurve && a2 != NULL  && a2->getCurveCount() == 2) {
                c = a2->getCurve(0, orientation);
                if (c == curve) {
                    c = a2->getCurve(1, orientation);
                }
                if (c->getSize() == 2) {
                    inside = a2;
                    outside = a1;
                    doDeleteCurve = true;
                }
            }
        }
        if (doDeleteCurve) {
            tmpChanges.addedCurves.insert(c->getId());
            tmpChanges.removedCurves.insert(c->getId());
            removeArea(inside->getId());
            if (c->getArea1() != NULL) {
                tmpChanges.addedAreas.insert(c->getArea1()->getId());
                tmpChanges.removedAreas.insert(c->getArea1()->getId());
            }
            if (outside != NULL) {

                bool foundCurve = false;
                for (int i = 0; i < outside->getCurveCount(); i++) {

                    CurvePtr cc = outside->getCurve(i);
                    if (cc == curve) {
                        foundCurve = true;
                        outside->removeCurve(i);

                        tmpChanges.addedAreas.insert(outside->getId());
                        c->addArea(outside->getId());
                        outside->addCurve(c->getId(), orientation);
                        outside->build();
                    }
                }
                if (!foundCurve) assert(false);

            }
            removeCurve(curve->getId());
            selectedPoint = -1;
            selectedSegment = -1;
            curve = NULL;
        } else {
            tmpChanges.addedAreas = tmpChanges.removedAreas;
            curve->removeVertex(selectedPoint);
            selectedSegment = selectedPoint - 1;
            selectedPoint = -1;
        }
    } else { //node
        if (curve->getSize() == 2) {
            removeCurve(cid, changes);
            curve = NULL;
            selectedPoint= -1;
            selectedSegment = -1;
            return;
        }
        if (isLoop) {
            removeArea(curve->getArea1()->getId());
        }
        if (selectedPoint == 0 || isLoop) {
            if (!isLoop) {
                start->removeCurve(cid);
                if (start->getCurveCount() == 0) {
                    removeNode(start->getId());
                }
            }
            NodePtr n = newNode(curve->getXY(1));
            n->addCurve(cid);
            curve->addVertex(n->getId(), false);
            curve->removeVertex(1);
            size--;
        }
        if (selectedPoint == size - 1 || isLoop) {
            end->removeCurve(cid);
            if (end->getCurveCount() == 0) {
                removeNode(end->getId());
            }
            NodePtr n = newNode(curve->getXY(size - 2));
            n->addCurve(cid);
            curve->addVertex(n->getId(), true);
            curve->removeVertex(size - 2);
        }
        curve = NULL;
        selectedPoint = -1;
        selectedSegment = -1;
    }
    changed = merge(changed, tmpChanges);
}

CurvePtr Graph::removeNode(CurvePtr first, CurvePtr second, const vec2d &p, Graph::Changes &changed, int &selectedPoint)
{
    int ind1 = 0;
    int ind2 = 0;
    int i;
    int orientation;
    Graph::Changes tmpChanges;

    // Getting the orientation of the curves
    if (p == first->getEnd()->getPos()) {
        ind1 = 1;
    }
    if (p == second->getEnd()->getPos()) {
        ind2 = 1;
    }
    NodePtr start;
    NodePtr end;

    if (selectedPoint != -1) {
        selectedPoint = first->getSize() - 1;
    }
    if (ind1 == 1) { // If possible, we just add the second curve's points into the first curve's list
        first->addVertex(p, -1, -1, false);

        if (ind2 == 0) { // Case first->end == second->start
            start = second->getStart();
            end = second->getEnd();
        } else { // Case first->end == second->end
            start = second->getEnd();
            end = second->getStart();
        }
        for (i = 1; i < (int)second->getSize() - 1; i++) {
            first->addVertex(second->getVertex(start, i));
        }

        first->addVertex(end->getId());
        end->removeCurve(second->getId());
        end->addCurve(first->getId());
        start->removeCurve(first->getId());
    } else {
        if (ind2 == 0) { // Case first->start == second->start -> we create a whole new Curve
            second->invert();
        }  // Case first->end == second->start -> we call the function with inverted parameters
        return removeNode(second, first, p, changed, selectedPoint);
    }
    AreaPtr a1 = first->getArea1();
    AreaPtr a2 = first->getArea2();
    if (a1 != NULL) { // Removing the second curve
        for (i = 0; i < a1->getCurveCount(); i++) {
            if (a1->getCurve(i, orientation) == second) {
                a1->removeCurve(i);
                break;
            }
        }
        second->removeArea(a1->getId());
        a1->build();
        tmpChanges.removedAreas.insert(a1->getId());
        tmpChanges.addedAreas.insert(a1->getId());
        if (a2 != NULL) {
            for (i = 0; i < a2->getCurveCount(); i++) {
                if (a2->getCurve(i, orientation) == second) {
                    a2->removeCurve(i);
                    break;
                }
            }
            second->removeArea(a2->getId());
            a2->build();
            tmpChanges.removedAreas.insert(a2->getId());
            tmpChanges.addedAreas.insert(a2->getId());
        }

    }
    tmpChanges.removedCurves.insert(first->getId());
    tmpChanges.removedCurves.insert(second->getId());
    tmpChanges.addedCurves.insert(first->getId());
    removeCurve(second->getId());
    changed = merge(changed, tmpChanges);
    //removeNode(start->getId());
    first->resetBounds();
    first->computeCurvilinearCoordinates();

    return first;
}

CurvePtr Graph::addCurve(vec2d start, vec2d end, Graph::Changes &changed)
{
    NodePtr n1 = newNode(start);
    NodePtr n2 = newNode(end);
    CurvePtr c = newCurve(NULL, n1, n2);
    changed.addedCurves.insert(c->getId());
    return c;
}

CurvePtr Graph::addCurve(NodeId start, vec2d end, Graph::Changes &changed)
{
    NodePtr n = newNode(end);
    CurvePtr c = newCurve(NULL, getNode(start), n);
    changed.addedCurves.insert(c->getId());

    return c;
}

void Graph::getPointsFromCurves(vector<CurveId> &curves, map<CurveId, int> orientations, vector<Vertex> &points)
{
    for (vector<CurveId>::iterator i = curves.begin(); i != curves.end(); i++) {
        CurvePtr curve = getCurve(*i);
        if (orientations[*i] == 0) {
            for (int j =  1; j < curve->getSize(); ++j) {
                points.push_back(curve->getVertex(j));
            }
        } else {
            for (int j = curve->getSize() - 2; j >= 0; --j) {
                points.push_back(curve->getVertex(j));
            }
        }
    }
}

bool Graph::buildArea(CurvePtr begin, set<CurveId> &excluded, vector<CurveId> &used, map<CurveId, int> &orientations, int orientation)
{
    CurvePtr next = NULL;
    CurvePtr cur = begin;
    CurvePtr prev = NULL;
    NodePtr start, end;
    //int curInd = orientation ? 1 : cur->getSize() - 2, nextInd;

    NodePtr curNode = orientation ? begin->getStart() : begin->getEnd();
    bool found = false;
    used.clear();
    orientations.clear();
    set<CurveId> visited;
    while (true) {
        next = cur->getNext(curNode, excluded);
        if (next == NULL) {
            orientations.clear();
            for (int i = 0; i < (int) used.size(); i++) {
                excluded.erase(used[i]);
            }
            excluded.insert(cur->getId());
            used.clear();
            if (cur == begin) {
                break; // no new area
            }
            cur = begin;
            curNode = orientation ? begin->getStart() : begin->getEnd();
        } else {
            start = cur->getStart();
            end = cur->getEnd();
            used.push_back(cur->getId());
            if (start == end) {
                if (prev == NULL) {
                    orientations[cur->getId()] = orientation;
                } else {

                    vec2d s = start->getPos();
                    vec2d v = prev->getXY(start, 1);
                    float angleStart = angle(v - s, cur->getXY(1) - s);
                    float angleEnd   = angle(v - s, cur->getXY(cur->getSize() - 2) - s);
                    orientations[cur->getId()] = angleStart > angleEnd;
                }
            } else {
                orientations[cur->getId()] = curNode == start ? 1 : 0;
            }
            visited.insert(cur->getId());
            if (cur != begin) {
                excluded.insert(cur->getId());
            }
            prev = cur;
            cur = next;
            if (cur->getStart() == cur->getEnd() && (int) used.size() > 0) {
                excluded.insert(cur->getId());
            } else {
            //curInd = nextInd == 1 ? next->getSize() - 2 : 1;
                curNode = cur->getOpposite(curNode);
            }
            if  (next == begin) {
                found = true;
                break; //area was found
            }
        }
    }
    for (int i = 0; i < (int) used.size(); i++) {
        excluded.erase(used[i]);
    }
    visited.clear();

    if (found) {
        vector<Vertex> points;
        getPointsFromCurves(used, orientations, points);
        if (Area::isDirect(points, 0, points.size() - 1)) {
            found = false;
            used.clear();
            orientations.clear();
        }
    }

    return found;
}

CurvePtr Graph::addCurve(NodeId start, NodeId end, Graph::Changes &changed)
{
    Graph::Changes tmpChanges;

    NodePtr s = getNode(start);
    NodePtr e = getNode(end);
    CurvePtr c = newCurve(NULL, s, e);
    tmpChanges.addedCurves.insert(c->getId());

    set<CurveId> excluded;
    set<CurveId> changedCurves;
    vector<CurveId> used1, used2;
    map<CurveId, int> orientations1, orientations2;

    int nbAreas = buildArea(c, excluded, used1, orientations1, 0);

    copy(used1.begin(), used1.end(), inserter(changedCurves, changedCurves.begin()));

    nbAreas += buildArea(c, excluded, used2, orientations2, 1);
    copy(used2.begin(), used2.end(), inserter(changedCurves, changedCurves.begin()));

    tmpChanges.addedCurves.insert(changedCurves.begin(), changedCurves.end());
    tmpChanges.removedCurves.insert(changedCurves.begin(), changedCurves.end());
    getAreasFromCurves(changedCurves, tmpChanges.removedAreas);
    getAreasFromCurves(changedCurves, tmpChanges.addedAreas);

    bool doCreate = true;

    if (doCreate) {
        AreaPtr a1, a2, a;
        if (nbAreas == 2) { // We clipped an Area that should be deleted
            for (set<CurveId>::iterator i = changedCurves.begin(); i != changedCurves.end(); i++) {
                CurvePtr c = getCurve(*i);
                a1 = c->getArea1();
                a2 = c->getArea2();
                a = NULL;
                if (a1 != NULL && a1->equals(changedCurves)) {
                    a = a1;
                } else if (a2 != NULL && a2->equals(changedCurves)) {
                    a = a2;
                }
                if (a != NULL) {
                    tmpChanges.addedAreas.erase(a->getId());
                    removeArea(a->getId());
                    break;
                }
            }
        }
        if ((int) used1.size() > 0) {
            AreaPtr nArea = newArea(NULL, false);
            for (vector<CurveId>::iterator i = used1.begin(); i != used1.end(); i++) {
                nArea->addCurve(*i, orientations1[*i]);
                getCurve(*i)->addArea(nArea->getId());
                tmpChanges.addedAreas.insert(nArea->getId());
            }
        }
        if ((int) used2.size() > 0) {
            AreaPtr nArea = newArea(NULL, false);
            for (vector<CurveId>::iterator i = used2.begin(); i != used2.end(); i++) {
                nArea->addCurve(*i, orientations2[*i]);
                getCurve(*i)->addArea(nArea->getId());
                tmpChanges.addedAreas.insert(nArea->getId());
            }
        }
    } else {
        if (Logger::WARNING_LOGGER != NULL) {
            Logger::WARNING_LOGGER->log("GRAPH", "Invalid Curve added : splitting multiple areas");
        }
    }

    changed = merge(changed, tmpChanges);
    return c;
}

void Graph::removeCurve(CurveId id, Graph::Changes &changed)
{
    Graph::Changes tmpChanges;
    CurvePtr c = getCurve(id);
    NodePtr start = c->getStart();
    NodePtr end = c->getEnd();
    AreaPtr area1 = c->getArea1();
    AreaPtr area2 = c->getArea2();
    vector<CurveId> used;
    map<CurveId, int> orientations;
    set<CurveId> usedSet;
    set<CurveId> excludedCurves;

    CurvePtr begin = NULL;
    int orientation = 0;
    int rank = 0;
    if (area1 != NULL) {
        if (area2 != NULL) {
            if (area1->getCurveCount() > 1 && area2->getCurveCount() > 1) {// we rebuild the new area
                begin = area1->getCurve(rank++, orientation);
                while ((begin == c || (begin->getArea1() == area2 || begin->getArea2() == area2)) && rank < area1->getCurveCount()) {
                    begin = area1->getCurve(rank++, orientation);
                }
                assert(rank <= area1->getCurveCount());
                excludedCurves.insert(c->getId());
            }
            tmpChanges.removedAreas.insert(area2->getId());
            removeArea(area2->getId());
        }
        tmpChanges.removedAreas.insert(area1->getId());
        removeArea(area1->getId());
    }
    tmpChanges.removedCurves.insert(c->getId());
    if (begin != NULL) {
        assert(buildArea(begin, excludedCurves, used, orientations, orientation));// updating the existing area

        AreaPtr nArea = newArea(NULL, false);

        vector<CurveId>::iterator i;
        for (i = used.begin(); i != used.end(); i++) {
            usedSet.insert(*i);
        }

        for (i = used.begin(); i != used.end(); i++) {
            nArea->addCurve(*i, orientations[*i]);
            getCurve(*i)->addArea(nArea->getId());
        }

        tmpChanges.addedCurves.insert(usedSet.begin(), usedSet.end());
        tmpChanges.removedCurves.insert(usedSet.begin(), usedSet.end());
        getAreasFromCurves(usedSet, tmpChanges.addedAreas);
        getAreasFromCurves(usedSet, tmpChanges.removedAreas);
    }
    removeCurve(c->getId());
    changed = merge(changed, tmpChanges);
}

// ---------------------------------------------------------------------------
// CLIPPING
// ---------------------------------------------------------------------------
CurvePart *Graph::createCurvePart(CurvePtr p, int orientation, int start, int end)
{
    return new BasicCurvePart(p, orientation, start, end);
}

Graph* Graph::createChild()
{
    return new BasicGraph();
}

CurvePtr Graph::addCurvePart(CurvePart &cp, set<CurveId> *addedCurves, bool setParent)
{
    const vec2d beginPos = cp.getXY(0);
    const vec2d endPos = cp.getXY(cp.getEnd());

    NodePtr start;
    NodePtr end;

    map<vec2d, Node*, Cmp>::iterator i = mapping->find(beginPos);
    if (i == mapping->end()) {
        start = newNode(beginPos);
    } else {
        start = i->second;
    }

    i = mapping->find(endPos);
    if (i == mapping->end()) {
        end = newNode(endPos);
    } else {
        end = i->second;
    }

    for (int i = 0; i < start->getCurveCount(); ++i) {
        CurvePtr c = start->getCurve(i);
        if (c->getOpposite(start) == end) {
            if ((cp.getId().id != NULL_ID && c->getParentId() == cp.getId()) || cp.equals(c)) {
                return c;
            }
        }
    }

    CurvePtr c = NULL;

    if (cp.getId().id != NULL_ID){
        c = newCurve(cp.getCurve(), setParent);
    } else {
        c = newCurve(NULL, false);
    }
    c->width = cp.getWidth();
    c->type = cp.getType();
    if (cp.getS(0) < cp.getS(cp.getEnd())) {
        c->addVertex(start->getId());
        c->s0 = cp.getS(0);
        for (int i = 1; i < cp.getEnd(); ++i) {
            c->addVertex(cp.getXY(i), cp.getS(i), -1, cp.getIsControl(i));
        }
        c->addVertex(end->getId());
        c->s1 = cp.getS(cp.getEnd());
    } else {
        c->addVertex(end->getId());
        c->s0 = cp.getS(cp.getEnd());
        for (int i = cp.getEnd() - 1; i >= 1; --i) {
            c->addVertex(cp.getXY(i), cp.getS(i), -1, cp.getIsControl(i));
        }
        c->addVertex(start->getId());
        c->s1 = cp.getS(0);
    }
    start->addCurve(c->getId());
    end->addCurve(c->getId());
    if (addedCurves != NULL) {
        addedCurves->insert(c->getId());
    }
    return c;
}

void Graph::addCurvePart(CurvePart &cp, set<CurveId> *addedCurves, set<CurveId> &visited, AreaPtr a)
{
    CurvePtr c = addCurvePart(cp, addedCurves);

    visited.insert(cp.getId());
    if (a != NULL) {
        int o = 0;
        if (c->getStart() == c->getEnd()) {
            if (!c->isDirect()) {
                o = 1;
            }
        } else {
            vec2d cp0 = c->getStart()->getPos();
            vec2d cp1 = c->getEnd()->getPos();
            vec2d p0 = cp.getXY(0);
            vec2d p1 = cp.getXY(cp.getEnd());
            if (cp0 == p1 && cp1 == p0) {
                o = 1;
            } else {
                assert(cp0 == p0 && cp1 == p1);
            }
        }
        a->addCurve(c->getId(), o);
        c->addArea(a->getId());
    }
}

Graph *Graph::clip(const box2d &clip, Margin *margin)
{
    set<CurveId> visited;
    // We suppose here that LazyGraphs are only for the top of the Graph, and will never be used as childs
    Graph * result = createChild();
    result->parent = this;
    //result->mapping = new map<vec2d, Node*, Cmp> ();
    float w = clip.xmax - clip.xmin;
    box2d bclip = clip.enlarge(margin->getMargin(w));
    double maxAreaMargin = 0;

    ptr<AreaIterator> ai = getAreas();

    while (ai->hasNext()) { // Getting the largest Margin
        maxAreaMargin = max(maxAreaMargin, margin->getMargin(w, ai->next()));
    }
    box2d aclip = bclip.enlarge(maxAreaMargin); // Enlarging the clipping box
    ai = getAreas();
    box2d hclip = aclip; // Creation of 2 boxes : infinite on X and on Y respectively
    box2d vclip = aclip;
    hclip.xmin = -INFINITY;
    hclip.xmax = INFINITY;
    vclip.ymin = -INFINITY;
    vclip.ymax = INFINITY;

    while (ai->hasNext()) {
        AreaPtr a = ai->next(); // For each Area
        if (clipRectangle(aclip, a->getBounds())) { // If the area is clipped by the box
            vector<CurvePart*> cpaths;
            vector<CurvePart*> hpaths;
            vector<CurvePart*> vpaths;

            for (int j = 0; j < a->getCurveCount(); ++j) {
                int orientation;
                CurvePtr p = a->getCurve(j, orientation);
                cpaths.push_back(createCurvePart(p, orientation, 0, p->getSize() - 1)); // Getting all the curveParts
            }
            //Horizontal clip
            if (!Area::clip(cpaths, hclip, hpaths)) {
                assert(false);
            }
            //Vertical clip
            if (!Area::clip(hpaths, vclip, vpaths)) {
                assert(false);
            }
            if (vpaths.size() > 0) { // If new Areas/Curves have been created during the clip, we add them to the result graph
                AreaPtr ca = result->newArea(a, true);
                ca->info = a->getInfo();
                for (int j = 0; j < (int) vpaths.size(); ++j) {
                    result->addCurvePart(*vpaths[j], NULL, visited, ca);
                }
                ca->check();
                if (a->getSubgraph() != NULL) { // Clipping subgraphs
                    ca->subgraph = a->getSubgraph()->clip(clip, margin);
                    ca->subgraph->setParent(a->getSubgraph().get());
                } else {
                    ca->subgraph = NULL;
                }
            }
            //cleaning
            for (int j = 0; j < (int) cpaths.size(); ++j) {
                delete cpaths[j];
            }
            for (int j = 0; j < (int) hpaths.size(); ++j) {
                delete hpaths[j];
            }
            for (int j = 0; j < (int) vpaths.size(); ++j) {
                delete vpaths[j];
            }
        }
    }

    // Clipping the remaining curves that weren't cliped via the areas
    ptr<CurveIterator> ci = getCurves();

    vector<CurvePart*> cpaths(10);

    while (ci->hasNext()) {
        CurvePtr c = ci->next();

        if (visited.find(c->getId()) == visited.end()) {

            box2d pclip = bclip.enlarge(margin->getMargin(w, c));

            if (clipRectangle(pclip, c->getBounds())) {

                BasicCurvePart cp(c, 0, 0, c->getSize() - 1);
                cpaths.clear();

                static_cast<CurvePart*>(&cp)->clip(pclip, cpaths);

                for (int i = 0; i < (int) cpaths.size(); ++i) {
                    result->addCurvePart(*cpaths[i], NULL, visited, NULL);
                }

                for (int i = 0; i < (int) cpaths.size(); ++i) {
                    delete cpaths[i];
                }

            }
        }
    }

    return result;
}

void Graph::clipUpdate(const Changes &srcChanges, const box2d &clip, Margin *margin, Graph &result, Changes &dstChanges)
{
    if ((int) srcChanges.changedArea.size() > 0) {
        Changes c = srcChanges;
        AreaId id = *(srcChanges.changedArea.begin());
        AreaPtr a = result.getChildArea(id);
        if (a == NULL) {
            return;
        }
        dstChanges.changedArea.push_back(a->getId());
        c.changedArea.pop_front();
        return getArea(id)->getSubgraph()->clipUpdate(c, clip, margin, *(a->getSubgraph()), dstChanges);
    }

    float w = clip.xmax - clip.xmin;
    box2d bclip = clip.enlarge(margin->getMargin(w));
    map<GraphPtr, GraphPtr> subgraphs;

    set<AreaId>::const_iterator ai = srcChanges.removedAreas.begin();
    while (ai != srcChanges.removedAreas.end()) {
        AreaPtr a = result.getChildArea(*(ai++));
        if (a != NULL) {
            AreaId aid = a->getId();
            for (int j = 0; j < a->getCurveCount(); ++j) {
                int orientation;
                CurvePtr c = a->getCurve(j, orientation);

                if (c->getParentId().id == NULL_ID) {
                    CurveId cid = c->getId();
                    c->removeArea(a->getId());
                    if (c->getArea1() == NULL) {
                        result.removeCurve(cid);
                        dstChanges.removedCurves.insert(cid);
                    }
                    a->removeCurve(j--);
                }
            }
            if (a->subgraph != NULL) {
                Graph *key = a->subgraph->parent;
                assert(subgraphs.find(key) == subgraphs.end());
                subgraphs[key] = a->subgraph;
                a->subgraph = NULL;
            }
            result.removeArea(aid);
            dstChanges.removedAreas.insert(aid);
        }
    }
    set<CurveId>::const_iterator ci = srcChanges.removedCurves.begin();
    while (ci != srcChanges.removedCurves.end()) {
        ptr<CurveIterator> c = result.getChildCurves(*(ci++));
        while (c->hasNext()) {
            CurvePtr cc = c->next();
            CurveId cid = cc->getId();
            result.removeCurve(cid);
            dstChanges.removedCurves.insert(cid);
        }
    }
    set<CurveId> visited;
    double maxAreaMargin = 0;
    ai = srcChanges.addedAreas.begin();
    while (ai != srcChanges.addedAreas.end()) {
        maxAreaMargin = max(maxAreaMargin, margin->getMargin(w, getArea(*(ai++))));
    }
    box2d aclip = bclip.enlarge(maxAreaMargin);

    ai = srcChanges.addedAreas.begin();
    while (ai != srcChanges.addedAreas.end()) {
        AreaPtr a = getArea(*(ai++));
        assert(a != NULL);

        box2d hclip = aclip;
        box2d vclip = aclip;
        hclip.xmin = (float)-INFINITY;
        hclip.xmax = (float)INFINITY;
        vclip.ymin = (float)-INFINITY;
        vclip.ymax = (float)INFINITY;

        vector<CurvePart*> cpaths;
        vector<CurvePart*> hpaths;
        vector<CurvePart*> vpaths;
        for (int j = 0; j < a->getCurveCount(); ++j) {
            int orientation;
            CurvePtr p = a->getCurve(j, orientation);
            cpaths.push_back(createCurvePart(p, orientation, 0, p->getSize() - 1));
        }
        if (!Area::clip(cpaths, hclip, hpaths)) {
            assert(false);
        }
        if (!Area::clip(hpaths, vclip, vpaths)) {
            assert(false);
        }
        if (vpaths.size() > 0) {
            AreaPtr ca = result.newArea(a, true);
            ca->info = a->getInfo();

            for (int j = 0; j < (int) vpaths.size(); ++j) {
                result.addCurvePart(*vpaths[j], &dstChanges.addedCurves, visited, ca);
            }
            ca->check();
            if (a->subgraph != NULL) {
                GraphPtr key = a->subgraph;
                map<GraphPtr, GraphPtr>::iterator it = subgraphs.find(key);
                if (it != subgraphs.end()) {
                    ca->subgraph = it->second;
                    subgraphs.erase(it);
                } else {
                    ca->subgraph = a->subgraph->clip(clip, margin);
                }
            }
            dstChanges.addedAreas.insert(ca->getId());
        }

        for (int j = 0; j < (int) cpaths.size(); ++j) {
            delete cpaths[j];
        }
        for (int j = 0; j < (int) hpaths.size(); ++j) {
            delete hpaths[j];
        }
        for (int j = 0; j < (int) vpaths.size(); ++j) {
            delete vpaths[j];
        }
    }
    subgraphs.clear();
    ci = srcChanges.addedCurves.begin();
    while (ci != srcChanges.addedCurves.end()) {
        CurvePtr c = getCurve(*(ci++));
        assert(c != NULL);
        if (visited.find(c->getId()) == visited.end()) {
            box2d pclip = bclip.enlarge(margin->getMargin(w, c));
            if (clipRectangle(pclip, c->getBounds())) {
                vector<CurvePart*> cpaths;
                CurvePart *cp = createCurvePart(c, 0, 0, c->getSize() - 1);
                cp->clip(pclip, cpaths);
                for (int j = 0; j < (int) cpaths.size(); ++j) {
                    result.addCurvePart(*cpaths[j], &dstChanges.addedCurves, visited, NULL);

                    delete cpaths[j];
                  }
                delete cp;
            }
        }
    }
    result.clean();
}

// ---------------------------------------------------------------------------
// BUILD AND UNBUILD
// ---------------------------------------------------------------------------

void Graph::followHalfCurve(NodePtr prev, NodePtr cur, vector<NodePtr> &result, set<CurvePtr> &visited, bool invert, bool useType, float *width)
{

    while (true)
    {
        bool recurse = find(result.begin(), result.end(), cur) == result.end();
        if (invert) {
            result.insert(result.begin(), cur);
        } else if (result.size() < 3 || result[0] != result[result.size() - 1]) {
            result.push_back(cur);
        }
        if (recurse) {
            NodePtr next = NULL;
            if (cur->getCurveCount() == 2) {
                next = cur->getOpposite(prev);
            }
            if (next != NULL && (!useType || cur->getCurve(next)->type == 0)) {
                CurvePtr l = cur->getCurve(next);
                if (visited.find(l) == visited.end()) {
                    visited.insert(l);
                    *width = max(*width, l->width);
                    //followHalfCurve(cur, next, result, visited, invert, useType, width);
                    prev = cur;
                    cur = next;
                    continue;
                }
            }
        }
        break;
    }
}

void Graph::mergeNodes(NodeId ida, NodeId idb)
{
    NodePtr na = getNode(ida);
    NodePtr nb = getNode(idb);
    assert(na != NULL);
    assert(nb != NULL);
    assert(na != nb);

    CurvePtr cab = na->getCurve(nb);
    assert(cab != NULL);
    removeCurve(cab->getId());

    na = getNode(ida);
    nb = getNode(idb);
    if ((na == NULL) && (nb == NULL))
    {
        return;
    }

    // create a new vertex between na and nb
    NodePtr nc = newNode((na->getPos() + nb->getPos()) * 0.5f);

    set<NodePtr> toAdd; // list of nodes to link with


    vector<CurvePtr> curves_of_a;
    vector<CurvePtr> curves_of_b;

    // store curves
    if (na != NULL) {
        for (int i = 0; i < na->getCurveCount(); ++i) {
            curves_of_a.push_back(na->getCurve(i));
        }
    }

    if (nb != NULL) {
        for (int i = 0; i < nb->getCurveCount(); ++i) {
            curves_of_b.push_back(nb->getCurve(i));
        }
    }

    for (unsigned int i = 0; i < curves_of_a.size(); ++i)
    {
        CurvePtr c = curves_of_a[i];
        NodePtr n = c->getOpposite(na);
        if ((n != na) && (n != nb)&& (n != NULL)) {
            if (toAdd.find(n) == toAdd.end()) toAdd.insert(n);
        }
    }

    for (unsigned int i = 0; i < curves_of_b.size(); ++i)
    {
        CurvePtr c = curves_of_b[i];
        NodePtr n = c->getOpposite(nb);
        if ((n != na) && (n != nb) && (n != NULL))
        {
            if (toAdd.find(n) == toAdd.end()) toAdd.insert(n);
        }
    }

    set<NodePtr>::iterator i;
    for (i = toAdd.begin(); i != toAdd.end(); ++i)
    {
        newCurve(NULL, nc, *i);
    }

    // delete previous curves
    for (unsigned int i = 0; i < curves_of_a.size(); ++i) {
        removeCurve(curves_of_a[i]->getId());
    }

    for (unsigned int i = 0; i < curves_of_b.size(); ++i) {
        removeCurve(curves_of_b[i]->getId());
    }

    removeNode(ida);
    removeNode(idb);
    assert(getNode(ida) == NULL);
    assert(getNode(idb) == NULL);
}

void Graph::followCurve(CurvePtr c, bool useType, set<CurvePtr> &visited, float *width, int *type, vector<NodePtr> &result)
{
    NodePtr pa = c->getStart();
    NodePtr pb = c->getEnd();
    visited.insert(c);

    if (!useType || c->type == 0) {
        *width = c->width;
        *type = c->type;
        followHalfCurve(pb, pa, result, visited, true, useType, width);
        followHalfCurve(pa, pb, result, visited, false, useType, width);
    } else {
        result.push_back(pa);
        result.push_back(pb);
        *width = c->width;
        *type = c->type;
    }

}

void Graph::buildCurves(bool useType, GraphPtr result)
{
    map<NodePtr, NodePtr> pointClones;
    set<CurvePtr> visited;
    ptr<CurveIterator> ci = getCurves();

    while (ci->hasNext()) {
        CurvePtr c = ci->next();

        if (visited.find(c) == visited.end()) {
            float width = 0;
            int type = 0;
            vector<NodePtr> p;
            followCurve(c, useType, visited, &width, &type, p);

            CurvePtr bc = result->newCurve(NULL, false);
            bc->setWidth(width);
            bc->setType(type);
            NodePtr start;
            map<NodePtr, NodePtr>::iterator s = pointClones.find(p[0]);
            if (s == pointClones.end()) {
                start = result->newNode(p[0]->getPos());
                pointClones.insert(make_pair(p[0], start));
            } else {
                start = s->second;
            }
            bc->addVertex(start->getId());
            start->addCurve(bc->getId());

            for (int i = 1; i < (int) p.size() - 1; i++) {
                bc->addVertex(p[i]->getPos(), (float) i, -1, false);
            }

            NodePtr end;
            map<NodePtr, NodePtr>::iterator e = pointClones.find(p[p.size() - 1]);
            if (e == pointClones.end()) {
                end = result->newNode(p[p.size() - 1]->getPos());
                pointClones.insert(make_pair(p[p.size() - 1], end));
            } else {
                end = e->second;
            }
            bc->addVertex(end->getId());
            end->addCurve(bc->getId());

            bc->computeCurvilinearCoordinates();
        }
    }
}

void Graph::buildAreas()
{
    set<CurveId> includedCurves;
    set<CurveId> excludedCurves;
    ptr<CurveIterator> ci = getCurves();
    while (ci->hasNext()) {
        CurvePtr c = ci->next();
        includedCurves.insert(c->getId());
    }

    while (true) {
        bool removed = false;
        set<CurveId>::iterator i = includedCurves.begin();
        while (i != includedCurves.end()) {
            set<CurveId>::iterator j = i++;
            CurvePtr c = getCurve(*j);
            if (c->getStart()->getCurveCount(includedCurves) == 1 ||
                    c->getEnd()->getCurveCount(includedCurves) == 1) {
                includedCurves.erase(j);
                excludedCurves.insert(c->getId());
                removed = true;
            }
        }
        if (!removed) {
            break;
        }
    }

    set<CurvePtr> visited;
    set<CurvePtr> visitedr;

    ci = getCurves();
    while (ci->hasNext()) {

        CurvePtr c = ci->next();
        if (includedCurves.find(c->getId()) == includedCurves.end()) {
            continue;
        }
        if (visited.find(c) == visited.end()) {
            AreaPtr a = newArea(NULL, false);
            NodePtr start = c->getStart();
            NodePtr cur = c->getEnd();
            a->addCurve(c->getId(), 0);
            if (!a->build(c, start, cur, excludedCurves, visited, visitedr)) {
                removeArea(a->getId());
            }
        }
        if (visitedr.find(c) == visitedr.end()) {
            AreaPtr a = newArea(NULL, false);
            NodePtr start = c->getEnd();
            NodePtr cur = c->getStart();
            a->addCurve(c->getId(), 1);
            if (!a->build(c, start, cur, excludedCurves, visited, visitedr)) {
                removeArea(a->getId());
            }
        }
    }
}

void Graph::decimateCurves(float minDistance)
{
    ptr<CurveIterator> iter = getCurves();

    while(iter->hasNext())
    {
        CurvePtr c = iter->next();
        c->removeDuplicateVertices();
        c->decimate(minDistance);
    }
}

#if 0
void Graph::decimate(float minDistance)
{
    printf(">decimate\n");
    while(true)
    {
        bool found = false;

        ptr<CurveIterator> iter = getCurves();

        while(iter->hasNext())
        {
            CurvePtr c = iter->next();
            vec2d start = c->getStart()->getPos();
            vec2d end = c->getEnd()->getPos();
            if ((start - end).squaredlength() < minDistance * minDistance)
            {
                found = true;
                // merge points

                mergeNodes( c->getStart()->getId(), c->getEnd()->getId());

                break;
            }

        }
        if(!found) break; // no more curves to remove
    }
    printf("<decimate\n");

}
#endif

void Graph::build(bool useType, GraphPtr result)
{
    buildCurves(useType, result);
    result->buildAreas();
}


/*
 *  B0, B1, B2, B3 :
 *    Bezier multipliers
 */
double B0(double u)
{
    double tmp = 1.0 - u;
    return (tmp * tmp * tmp);
}

double B1(double u)
{
    double tmp = 1.0 - u;
    return (3 * u * (tmp * tmp));
}

double B2(double u)
{
    double tmp = 1.0 - u;
    return (3 * u * u * tmp);
}

double B3(double u)
{
    return (u * u * u);
}

vec2d computeLeftTangent(vector<vec2d> d, int end)
{

    return (d[end + 1] - d[end]).normalize();
}
vec2d computeRightTangent(vector<vec2d> d, int end)
{

    return (d[end - 1] - d[end]).normalize();
}

void generateBezier(vector<vec2d> input, float *u, vector<vec2d> &output, int first, int last, vec2d t0, vec2d t1)
{
    int nPts = last - first + 1;
    vector<pair<vec2d, vec2d > > A;
    double C[2][2];
    double X[2];
    double det_C0_C1, det_C0_X, det_X_C1;
    double alpha_l, alpha_r;

    for (int i = 0; i < nPts; i++) {
        vec2d v1, v2;
        v1 = t0.normalize(B1(u[i]));
        v2 = t1.normalize(B1(u[i]));
        A.push_back(make_pair(v1, v2));
    }

    C[0][0] = 0.0;
    C[0][1] = 0.0;
    C[1][0] = 0.0;
    C[1][1] = 0.0;
    X[0]    = 0.0;
    X[1]    = 0.0;

    vec2d lastP = input[last];
    vec2d firstP = input[first];
    for (int i = 0; i < nPts; i++) {
        float l = u[i];
        C[0][0] += A[i].first.dot(A[i].first);
        C[0][1] += A[i].first.dot(A[i].second);
        C[1][0] += C[0][1];
        C[1][1] += A[i].second.dot(A[i].second);
        vec2d tmp = input[first + i] - ((firstP * B0(l)) + (firstP * B1(l)) + (lastP * B2(l)) + (lastP * B3(l)));
        X[0] += A[i].first.dot(tmp);
        X[1] += A[i].second.dot(tmp);
    }

    /* Compute the determinants of C and X    */
    det_C0_C1 = C[0][0] * C[1][1] - C[1][0] * C[0][1];
    det_C0_X  = C[0][0] * X[1]    - C[0][1] * X[0];
    det_X_C1  = X[0]    * C[1][1] - X[1]    * C[0][1];

    /* Finally, derive alpha values    */
    if (det_C0_C1 == 0.0) {
        det_C0_C1 = (C[0][0] * C[1][1]) * 10e-12;
    }
    alpha_l = det_X_C1 / det_C0_C1;
    alpha_r = det_C0_X / det_C0_C1;


    /*  If alpha negative, use the Wu/Barsky heuristic (see text) */
    /* (if alpha is 0, you get coincident control points that lead to
     * divide by zero in any subsequent NewtonRaphsonRootFind() call. */
    if (alpha_l < 1.0e-6 || alpha_r < 1.0e-6) {
        double    dist = (lastP - firstP).length() / 3.0f;
        output.clear();
        output.push_back(firstP);
        output.push_back(firstP + t0.normalize(dist));
        output.push_back(lastP + t1.normalize(dist));
        output.push_back(lastP);
        return;
    }

    /*  First and last control points of the Bezier curve are */
    /*  positioned exactly at the first and last data points */
    /*  Control points 1 and 2 are positioned an alpha distance out */
    /*  on the tangent vectors, left and right, respectively */
    output.clear();
    output.push_back(firstP);
    output.push_back(firstP + t0.normalize(alpha_l));
    output.push_back(lastP + t1.normalize(alpha_r));
    output.push_back(lastP);

    A.clear();
    return;
}

vec2d bezier(int degree, vector<vec2d> b, double t)
{
    vector<vec2d> v = b;
    /* Triangle computation    */
    for (int i = 1; i <= degree; i++) {
        for (int j = 0; j <= degree - i; j++) {
            v[j].x = (1.0 - t) * v[j].x + t * v[j+1].x;
            v[j].y = (1.0 - t) * v[j].y + t * v[j+1].y;
        }
    }

    return v[0];
}

float computeMaxError(vector<vec2d> input, vector<vec2d> &output, float *u, int first, int last, int &splitPoint)
{
    splitPoint = (last - first + 1)/2;
    float maxDist = 0.0f;

    for (int i = first + 1; i < last; i++) {
        vec2d p = bezier(3, output, u[i - first]);
        vec2d v = p - input[i];
        float dist = v.squaredLength();
        if (dist >= maxDist) {
            maxDist = dist;
            splitPoint = i;
        }
    }
    return maxDist;
}

float *getParameterization(vector<vec2d> points, int first, int last)
{
    float *u = new float[last - first + 1];

    u[0] = 0.0f;
    for (int i = first + 1; i <= last; i++) {
        u[i - first] = u[i - first - 1] + (points[i] - points[i - 1]).length();
    }

    for (int i = first + 1; i <= last; i++) {
        u[i - first] = u[i - first] / u[last - first];
    }

    return u;
}

/*
 *  NewtonRaphsonRootFind :
 *    Use Newton-Raphson iteration to find better root.
 */
float NewtonRaphsonRootFind(vector<vec2d> Q, vec2d P, float u)
{
    float numerator, denominator;
    vector<vec2d> Q1, Q2;    /*  Q' and Q''            */
    vec2d Q_u, Q1_u, Q2_u; /*u evaluated at Q, Q', & Q''    */
    float uPrime;        /*  Improved u            */
    int i;

    /* Compute Q(u)    */
    Q_u = bezier(3, Q, u);

    /* Generate control vertices for Q'    */
    for (i = 0; i <= 2; i++) {
        Q1.push_back(vec2d((Q[i+1].x - Q[i].x) * 3.0, (Q[i+1].y - Q[i].y) * 3.0));
    }

    /* Generate control vertices for Q'' */
    for (i = 0; i <= 1; i++) {
        Q2.push_back(vec2d((Q1[i+1].x - Q1[i].x) * 2.0, (Q1[i+1].y - Q1[i].y) * 2.0));
    }

    /* Compute Q'(u) and Q''(u)    */
    Q1_u = bezier(2, Q1, u);
    Q2_u = bezier(1, Q2, u);

    /* Compute f(u)/f'(u) */
    numerator = (Q_u.x - P.x) * (Q1_u.x) + (Q_u.y - P.y) * (Q1_u.y);
    denominator = (Q1_u.x) * (Q1_u.x) + (Q1_u.y) * (Q1_u.y) +
                    (Q_u.x - P.x) * (Q2_u.x) + (Q_u.y - P.y) * (Q2_u.y);

    /* u = u - f(u)/f'(u) */
    uPrime = u - (numerator/denominator);
    return uPrime;
}

float *Reparameterize(vector<vec2d> d, int first, int last, float *u, vector<vec2d> tmp)
{
    int nPts = last - first + 1;
    int i;
    float *uPrime = new float[nPts];
    for (i = first; i <= last; i++) {
        uPrime[i - first] = NewtonRaphsonRootFind(tmp, d[i], u[i - first]);
    }
    return uPrime;
}

void fitCubic(vector<vec2d> input, vector<vec2d> &output, int first, int last, vec2d t0, vec2d t1, float error)
{
    float maxError;
    float iterationError = error * error;
    int splitPoint;
    float *u;
    float *uPrime;
    int maxIterations = 4;

    int nPts = last - first + 1;

    vec2d firstP = input[first];
    vec2d lastP = input[last];

    if (nPts == 2) {
        float dist = (lastP - firstP).length() / 3.0f;

        if ((int) output.size() == 0) {
            output.push_back(firstP);
        }
        output.push_back(firstP + t0.normalize(dist));
        output.push_back(lastP + t1.normalize(dist));
        output.push_back(lastP);
        return;
    }

    vector<vec2d> tmp;
    u = getParameterization(input, first, last);
    generateBezier(input, u, tmp, first, last, t0, t1);

//      Find max deviation of points to fitted curve
    maxError = computeMaxError(input, tmp, u, first, last, splitPoint);
    if (maxError < error) {
        if ((int) output.size() == 0) {
            output.push_back(tmp[0]);
        }
        for (int i = 1; i < 4; i++) {
            output.push_back(tmp[i]);
        }
        delete u;
        return;
    }

    /*  If error not too large, try some reparameterization  */
    /*  and iteration */
    if (maxError < iterationError) {
        for (int i = 0; i < maxIterations; i++) {
            uPrime = Reparameterize(input, first, last, u, tmp);
            generateBezier(input, uPrime, tmp, first, last, t0, t1);
            //bezCurve = GenerateBezier(d, first, last, uPrime, tHat1, tHat2);
            maxError = computeMaxError(input, tmp, uPrime, first, last, splitPoint);
            if (maxError < error) {
                if ((int) output.size() == 0) {
                output.push_back(tmp[0]);
                }
                for (int i = 1; i < 4; i++) {
                    output.push_back(tmp[i]);
                }
                delete u;
                return;
            }
            delete u;
            u = uPrime;
        }
    }
    delete u;

    vec2d v1 = input[splitPoint - 1] - input[splitPoint];
    vec2d v2 = input[splitPoint] - input[splitPoint + 1];
    vec2d tC = ((v1 + v2) / 2.0f).normalize();

    fitCubic(input, output, first, splitPoint, t0, tC, error);
    fitCubic(input, output, splitPoint, last, -tC, t1, error);
}

void subdiv(const vector<vec2d> &pts, vector<vec2d> &result)
{

    int NbPt = pts.size();
    for (int j = 0; j < NbPt; ++j) {
        if (j == 0 || j == NbPt-1) {
            result.push_back(pts[j]);
            if (j == 0) {
                result.push_back((pts[j] + pts[j+1]) / 2);
            }
        } else {
            result.push_back((pts[j - 1] + pts[j]*6 + pts[j+1]) / 8);
            result.push_back((pts[j] + pts[j+1]) / 2);
        }
    }
}

void Graph::fitCubicCurve(vector<vec2d> points, vector<vec2d> &output, float error)
{

    output.clear();

    vector<vec2d> pts1;
    vector<vec2d> pts2;
    subdiv(points, pts1);
    subdiv(pts1, pts2);

    vec2d t0, t1;    /*  Unit tangent vectors at endpoints */
    t0 = computeLeftTangent(pts2, 0);
    t1 = computeRightTangent(pts2, (int) pts2.size() - 1);

    fitCubic(pts2, output, 0, pts2.size() - 1, t0, t1, error);
}

void Graph::save(const string &file, bool saveAreas, bool saveBinary, bool isIndexed)
{
    FileWriter *fileWriter = new FileWriter(file, saveBinary);

    if (isIndexed) {
        fileWriter->write(1);
        indexedSave(fileWriter, saveAreas);
    } else {
        fileWriter->write(0);
        save(fileWriter, saveAreas);
    }

    delete fileWriter;
}

void Graph::save(FileWriter *fileWriter, bool saveAreas)
{
    assert(fileWriter != NULL);

    fileWriter->write(2); //default nParamsNodes -> doesn't count the number of potential curves and the associated count
    fileWriter->write(3); //default nParamsCurves
    fileWriter->write(3); //default nParamsAreas
    fileWriter->write(1); //default nParamsCurveExtremities
    fileWriter->write(3); //default nParamsCurvePoints
    fileWriter->write(2); //default nParamsAreaCurves
    fileWriter->write(0); //default nParamsSubgraphs

    map<NodePtr, int> nindices;
    map<CurvePtr, int> cindices;
    map<AreaPtr, int> aindices;
    vector<NodePtr> nodeList;
    vector<CurvePtr> curveList;
    vector<AreaPtr> areaList;

    int index = 0;
    ptr<Graph::NodeIterator> ni = getNodes();
    while (ni->hasNext()) {
        NodePtr n = ni->next();
        nodeList.push_back(n);
        nindices[n] = index++;
    }

    index = 0;
    ptr<Graph::CurveIterator> ci = getCurves();
    while (ci->hasNext()) {
        CurvePtr c = ci->next();
        curveList.push_back(c);
        cindices[c] = index++;
    }

    index = 0;
    ptr<Graph::AreaIterator> ai = getAreas();
    while (ai->hasNext()) {
        AreaPtr a = ai->next();
        areaList.push_back(a);
        aindices[a] = index++;
    }

    //Saving nodes
    fileWriter->write(getNodeCount());
    for (int i = 0; i < (int) nodeList.size(); i++) {
    //for (map<NodePtr, int>::iterator i = nindices.begin(); i != nindices.end(); i++) {
        NodePtr n = nodeList[i];
        fileWriter->write(n->getPos().x);
        fileWriter->write(n->getPos().y);
        fileWriter->write(n->getCurveCount());
        for (int j = 0; j < n->getCurveCount(); j++) {
            fileWriter->write(cindices[n->getCurve(j)]);
        }
    }
    //Saving curves
    fileWriter->write(getCurveCount());

    for (int i = 0; i < (int) curveList.size(); i++) {
    //for (map<CurvePtr, int>::iterator i = cindices.begin(); i != cindices.end(); i++) {
        CurvePtr c = curveList[i];
        fileWriter->write(c->getSize());
        fileWriter->write(c->getWidth());
        fileWriter->write(c->getType());
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
    fileWriter->write(getAreaCount());
    for (int i = 0; i < (int) areaList.size(); i++) {
    //for (map<AreaPtr, int>::iterator i = aindices.begin(); i != aindices.end(); i++) {
        AreaPtr a = areaList[i];
        fileWriter->write(a->getCurveCount());
        fileWriter->write(a->info);
        if (saveAreas) {
            fileWriter->write(a->subgraph == NULL ? 0 : 1);
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

void Graph::indexedSave(FileWriter *fileWriter, bool saveAreas)
{
    assert(fileWriter != NULL);
    //long int offset = 0;
    //long int subgraphCountoffset  = 0;
    vector<long int> offsets;

    map<NodePtr, int> nindices;
    map<CurvePtr, int> cindices;
    map<AreaPtr, int> aindices;

    vector<NodePtr> nodeList;
    vector<CurvePtr> curveList;
    vector<AreaPtr> areaList;

    fileWriter->write(2); //default nParamsNodes -> doesn't count the number of potential curves and the associated count
    fileWriter->write(3); //default nParamsCurves
    fileWriter->write(3); //default nParamsAreas
    fileWriter->write(1); //default nParamsCurveExtremities
    fileWriter->write(3); //default nParamsCurvePoints
    fileWriter->write(2); //default nParamsAreaCurves
    fileWriter->write(0); //default nParamsSubgraphs

    int index = 0;
    ptr<Graph::NodeIterator> ni = getNodes();
    while (ni->hasNext()) {
        NodePtr n = ni->next();
        nodeList.push_back(n);
        nindices[n] = index++;
    }

    index = 0;
    ptr<Graph::CurveIterator> ci = getCurves();
    while (ci->hasNext()) {
        CurvePtr c = ci->next();
        curveList.push_back(c);
        cindices[c] = index++;
    }

    index = 0;
    ptr<Graph::AreaIterator> ai = getAreas();
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
        CurvePtr c = curveList[i];
        offsets.push_back(fileWriter->tellp());
        fileWriter->write(c->getSize());
        fileWriter->write(c->getWidth());
        fileWriter->write(c->getType());

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
        fileWriter->write(a->info);
        if (saveAreas) {
            fileWriter->write(a->subgraph == NULL ? 0 : 1);
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
    fileWriter->write(getNodeCount());
    fileWriter->write(getCurveCount());
    fileWriter->write(getAreaCount());
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


bool Graph::equals(Graph* g)
{
    if (g == NULL) {
        return false;
    } else {
        // checking the number of elements
        if (getNodeCount() != g->getNodeCount() || getCurveCount() != g->getCurveCount() || getAreaCount() != g->getAreaCount()) {
            return false;
        }

        set<NodeId> visitedNodes;
        set<CurveId> visitedCurves;
        bool found;
        // Checking areas
        ptr<AreaIterator> ai = getAreas();
        ptr<AreaIterator> ai2 = g->getAreas();
        while (ai->hasNext()) {
            AreaPtr a = ai->next();
            found = false;
            if (ai2->hasNext()) {
                AreaPtr a2 = ai2->next();
                if (a->equals(a2.get(), visitedCurves, visitedNodes)) { // Checking the next curve-- minimize search time
                    found = true;
                }
            }
            if (!found) { // If it's not the next one, we check the whole list
                ai2 = g->getAreas();
                while (ai2->hasNext()) {
                    AreaPtr a2 = ai2->next();
                    if (a->equals(a2.get(), visitedCurves, visitedNodes)) {
                        found = true;
                        break;
                    }
                }
            }
            if (!found) {
                return false;
            }
        }
        // Checking curves
        ptr<CurveIterator> ci = getCurves();
        ptr<CurveIterator> ci2 = g->getCurves();
        while (ci->hasNext()) {
            CurvePtr c = ci->next();
            if (visitedCurves.find(c->getId()) == visitedCurves.end()) {
                visitedCurves.insert(c->getId());
                found = false;
                if (ci2->hasNext()) {
                    CurvePtr c2 = ci2->next();
                    if (c->equals(c2.get(), visitedNodes)) {
                        found = true;
                    }
                }
                if (found == false) {
                    ci2 = g->getCurves();
                    while (ci2->hasNext()) {
                        CurvePtr c2 = ci2->next();
                        if (c->equals(c2.get(), visitedNodes)) {
                            found = true;
                            break;
                        }
                    }
                }
                if (!found) {
                    return false;
                }
            }
        }

        // Checking nodes
        ptr<NodeIterator> ni = getNodes();
        ptr<NodeIterator> ni2 = g->getNodes();

        while (ni->hasNext()) {
            NodePtr n = ni->next();
            if (visitedNodes.find(n->getId()) == visitedNodes.end()) {
                visitedNodes.insert(n->getId());
                found = false;
                if (ni2->hasNext()) {
                    NodePtr n2 = ni2->next();
                    if (n->getPos() == n2->getPos()) {
                        found = true;
                    }
                }
                if ( found == false ) {
                    ni2 = g->getNodes();
                    while (ni2->hasNext()) {
                        NodePtr n2 = ni2->next();
                        if (n->getPos() == n2->getPos()) {
                            found = true;
                            break;
                        }
                    }
                }
                if (!found) {
                    return false;
                }
            }
        }

        if (int(visitedNodes.size()) != getNodeCount()) {
            return false;
        }
        if (int(visitedCurves.size()) != getCurveCount()) {
            return false;
        }
        return true;
    }
}

void Graph::addListener(GraphListener *p)
{
    listeners.push_back(p);
}

void Graph::removeListener(GraphListener *p)
{
    vector<GraphListener *>::iterator it = find(listeners.begin(), listeners.end(), p);
    if (it != listeners.end()) {
        listeners.erase(it);
    }
}

int Graph::getListenerCount()
{
    return listeners.size();
}

void Graph::notifyListeners()
{
    version++;
    for (int i = 0; i < getListenerCount(); i++) {
        listeners[i]->graphChanged();
    }
}

Graph::Changes Graph::merge(Graph::Changes old, Graph::Changes c)
{
    Graph::Changes res;
    set<CurveId>::iterator i;
    set<AreaId>::iterator j;
    map<AreaId, Graph::Changes>::iterator k;
    for (i = old.addedCurves.begin(); i != old.addedCurves.end(); i++) {
        if (c.removedCurves.find(*i) == c.removedCurves.end()) {
            res.addedCurves.insert(*i);
        }
    }
    for (j = old.addedAreas.begin(); j != old.addedAreas.end(); j++) {
        if (c.removedAreas.find(*j) == c.removedAreas.end()) {
            res.addedAreas.insert(*j);
        }
    }

    if (old.changedArea.size() > 0) {
        if (c.removedAreas.find(*(old.changedArea.begin())) == c.removedAreas.end()) {
            res.changedArea = old.changedArea;
        }
    }

    res.insert(c);
    res.removedAreas.insert(old.removedAreas.begin(), old.removedAreas.end());
    res.removedCurves.insert(old.removedCurves.begin(), old.removedCurves.end());
    return res;
}

void Graph::checkDefaultParams(int nodes, int curves, int areas, int curveExtremities, int curvePoints, int areaCurves, int subgraphs)
{
    assert(nParamsNodes == 2);
    assert(nParamsCurves == 3);
    assert(nParamsAreas == 3);
    assert(nParamsCurveExtremities == 1);
    assert(nParamsCurvePoints == 3);
    assert(nParamsAreaCurves == 2);
    assert(nParamsSubgraphs == 0);
}

void Graph::checkParams(int nodes, int curves, int areas, int curveExtremities, int curvePoints, int areaCurves, int subgraphs)
{
    assert(nParamsNodes >= 2);
    assert(nParamsCurves >= 3);
    assert(nParamsAreas >= 3);
    assert(nParamsCurveExtremities >= 1);
    assert(nParamsCurvePoints >= 3);
    assert(nParamsAreaCurves >= 2);
    assert(nParamsSubgraphs >= 0);
}

}
