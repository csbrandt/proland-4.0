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

#include "proland/graph/Area.h"

#include "proland/graph/LineCurvePart.h"

namespace proland
{

Area::Area(Graph* owner) :
    Object("Area"), owner(owner), parent(NULL), info(rand()), subgraph(NULL), bounds(NULL)
{
    id.id = NULL_ID;
}

Area::~Area()
{
    subgraph = NULL;
    // Cleaning the list of curves

    if (id.id == NULL_ID) {
        for (int i = 0; i < getCurveCount(); i++) {
            getCurve(i)->removeArea(getId());
        }
    }
    curves.clear();
}

void Area::print() const {
    printf("%d %d %d\n", getCurveCount(), info, subgraph == NULL ? 0 : 1);
    int orientation;
    CurvePtr c;
    for (int i = 0; i < getCurveCount(); ++i) {
        c = getCurve(i, orientation);
        c->print();
        //printf("%d %d\n", c->getId(), orientation);
    }
}

AreaId Area::getId() const
{
    AreaId i;
    i.ref = (Area *)this;
    return i;
}

AreaPtr Area::getParent() const
{
    return parent;
}

AreaId Area::getParentId() const
{
    if (getParent() == NULL) {
        AreaId id;
        id.id = NULL_ID;
        return id;
    }
    return getParent()->getId();
}

AreaPtr Area::getAncestor()
{
    if (getParent() == NULL) {
        return this;
    } else {
        return getParent()->getAncestor();
    }
}

int Area::getInfo() const
{
    return info;
}

CurvePtr Area::getCurve(int i) const
{
    return curves[i].first;
}

CurvePtr Area::getCurve(int i, int& orientation) const
{
    orientation = curves[i].second;
    return curves[i].first;
}

int Area::getCurveCount() const
{
    return (int) curves.size();
}

GraphPtr Area::getSubgraph() const
{
    return subgraph;
}

box2d Area::getBounds() const
{
    if (bounds == NULL) {

        double xmin = INFINITY;
        double xmax = -INFINITY;
        double ymin = INFINITY;
        double ymax = -INFINITY;
        // Getting the bounds of each curve and keeping only the largest components
        for (int i = 0; i < getCurveCount(); ++i) {
            box2d b = getCurve(i)->getBounds();
            xmin = min(xmin, b.xmin);
            xmax = max(xmax, b.xmax);
            ymin = min(ymin, b.ymin);
            ymax = max(ymax, b.ymax);
        }

        bounds = new box2d(xmin, xmax, ymin, ymax);
    }
    return *bounds;
}

void Area::setOrientation(int i, int orientation)
{
    curves[i].second = orientation;
}

void Area::invertCurve(CurveId cid)
{
    vector< pair<CurvePtr, int> >::iterator it = curves.begin();
    while (it != curves.end()) {
        if (it->first == owner->getCurve(cid)) {
            break;
        }
        it++;
    }
    if (it != curves.end()) {
        it->second = it->second != 1;
    }
}

bool Area::isInside(const vec2d &p) const
{
    box2d bounds = getBounds();
    if (!bounds.contains(p)) {
        return false;
    }

    int intersectionCount = 0;
    vec2d a;
    // For each curve
    for (int i = 0; i < getCurveCount(); ++i) {
        int orientation;
        CurvePtr curve = getCurve(i, orientation);
        int n = curve->getSize();
        int cur, incr;
        if (orientation == 0) {
            if (i == 0) {
                a = curve->getStart()->getPos();
            }
            cur = 1;
            incr = 1;
        } else {
            if (i == 0) {
                a = curve->getEnd()->getPos();
            }
            cur = n - 2;
            incr = -1;
        }
        for (int j = 1; j < n; ++j) {
            // We check the number of intersections with the box
            // If intersectionCount %2 == 0 -> it's outside (intersections int and out)
            vec2d b = curve->getXY(cur);
            if (p.y >= min(a.y, b.y) && p.y <= max(a.y, b.y)) {
                /* assert(y != a.y && y != b.y);*/
                if (a.y != b.y && p.y != a.y) {
                    float xi = a.x + (p.y - a.y) / (b.y - a.y) * (b.x - a.x);
                    if (xi > p.x) {
                        ++intersectionCount;
                    }
                }
            }
            cur += incr;
            a = b;
        }
    }
    return intersectionCount % 2 != 0;
}

Curve::position Area::getRectanglePosition(const box2d &r) const
{
    box2d bounds = getBounds();
    if (r.xmin > bounds.xmax || r.xmax < bounds.xmin || r.ymin > bounds.ymax || r.ymax < bounds.ymin) {
        return Curve::OUTSIDE;
    }
    if (r.xmin < bounds.xmin && r.xmax > bounds.xmax && r.ymin < bounds.ymin && r.ymax > bounds.ymax) {
        return Curve::INTERSECT;
    }
    vec2d a = vec2d(0.f, 0.f);
    bool init = false;
    for (int i = 0; i < getCurveCount(); ++i) {
        int orientation;
        CurvePtr curve = getCurve(i, orientation);
        int n = curve->getSize();
        if (orientation == 0) {
            for (int j = 0; j < n; ++j) {
                vec2d b = curve->getXY(j);
                if (init && (a.x != b.x || a.y != b.y)) {
                    if (seg2d(a, b).intersects(r)) {
                        return Curve::INTERSECT;
                    }
                }
                a = b;
                init = true;
            }
        } else {
            for (int j = n - 1; j >= 0; --j) {
                vec2d b = curve->getXY(j);
                if (init && (a.x != b.x || a.y != b.y)) {
                    if (seg2d(a, b).intersects(r)) {
                        return Curve::INTERSECT;
                    }
                }
                a = b;
                init = true;
            }
        }
    }
    return isInside(r.center()) ? Curve::INSIDE : Curve::OUTSIDE;
}

Curve::position Area::getTrianglePosition(const vec2d* t) const
{
    bool b1, b2, b3;
    b1 = isInside(t[0]);
    b2 = isInside(t[1]);
    b3 = isInside(t[2]);
    if (b1 && b2 && b3) {
        return Curve::INSIDE;
    } else if (b1 || b2 || b3) {
        return Curve::INTERSECT;
    } else {
        return Curve::OUTSIDE;
    }
}

void Area::addCurve(CurveId id, int orientation)
{
    curves.push_back(make_pair(id.ref, orientation));
}

void Area::switchCurves(int curve1, int curve2)
{
    pair<CurvePtr, int> cq = curves[curve1];
    curves[curve1] = curves[curve2];
    curves[curve2] = cq;
}

void Area::removeCurve(int index)
{
    curves.erase(curves.begin() + index);
}

void Area::resetBounds() const
{
    if (bounds != NULL) {
        delete bounds;
        bounds = NULL;
    }
}

void Area::setInfo(int info)
{
    this->info = info;
}

void Area::setParent(AreaPtr parent)
{
    this->parent = parent;
}

void Area::setParentId(AreaId id)
{
    parent = id.ref;
}

void Area::setOwner(Graph * owner)
{
    this->owner = owner;
}

void Area::setSubgraph(Graph *g)
{
    this->subgraph = g;
}

bool Area::isDirect() const
{
    vector<Vertex> pts;
    for (int i = 0; i < getCurveCount(); ++i) {
        int orientation;
        CurvePtr p = getCurve(i, orientation);
        if (orientation == 0) {
            for (int j =  1; j < p->getSize(); ++j) {
                pts.push_back(p->getVertex(j));
            }
        } else {
            for (int j = p->getSize() - 2; j >= 0; --j) {
                pts.push_back(p->getVertex(j));
            }
        }
    }
    return isDirect(pts, 0, pts.size() - 1);
}

bool Area::isDirect(const vector<Vertex> &points, int start, int end)
{
    float ymin = 0.0f;
    int min = -1;
    int i;

    for (i = start; i <= end; ++i) {
        Vertex p = points[i];
        if (min == -1 || p.y < ymin) {
            min = i;
            ymin = p.y;
        }
    }
    int pred = min > start ? min - 1 : end;
    int succ = min < end ? min + 1 : start;
    Vertex pp = points[pred];
    Vertex pc = points[min];
    Vertex ps = points[succ];
    return cross(ps - pc, pp - pc) > 0.0f;
}

void Area::build()
{
    assert(getCurveCount() > 0);
    int o;
    CurvePtr cp = getCurve(0, o);
    NodePtr cur = o == 0 ? cp->getEnd() : cp->getStart();

    for (int i = 1; i < getCurveCount(); ++i) {
        bool ok = false;
        for (int j = i; j < getCurveCount(); ++j) {
            cp = getCurve(j, o);
            NodePtr start = cp->getStart();
            NodePtr end = cp->getEnd();
            if (start == cur || end == cur) {
                if (ok) {
                    vec2d prev = getCurve(i - 1, o)->getXY(cur, 1);
                    vec2d pi = getCurve(i, o)->getXY(cur, 1);
                    vec2d pj = getCurve(j, o)->getXY(cur, 1);
                    vec2d pc = cur->getPos();
                    float ai = angle(prev - pc, pi - pc);
                    float aj = angle(prev - pc, pj - pc);
                    if (ai < aj) {
                        switchCurves(i, j);
                    }
                } else {
                    switchCurves(i, j);
                    ok = true;
                }
            }
        }

        assert(ok);
        cp = getCurve(i, o);
        NodePtr start = cp->getStart();
        NodePtr end = cp->getEnd();
        cur = cur == start ? end : start;

    }
    check();
}

bool Area::build(const CurvePtr c, NodePtr start, NodePtr cur, const set<CurveId> &excludedCurves, set<CurvePtr> &visited, set<CurvePtr> &visitedr)
{
    CurvePtr p = c;
    int cc = 0;
    while (p != NULL && cur != start) {
        p = p->getNext(cur, excludedCurves);
        if (p != NULL) {
            for (int i = 0; i < getCurveCount(); ++i) {
                int orientation;
                if (getCurve(i, orientation) == p) {
                    return false;
                }
            }
            if (cur == p->getStart()) {
                addCurve(p->getId(), 0);
                cur = p->getEnd();
            } else {
                addCurve(p->getId(), 1);
                cur = p->getStart();
            }
            ++cc;
        }
    }
    if (cur != start) {
        return false;
    }
    check();
    if (!isDirect()) {
        return false;
    }
    for (int i = 0; i < getCurveCount(); ++i) {
        int orientation;
        p = getCurve(i, orientation);
        if (orientation == 0) {
            visited.insert(p);
        } else {
            visitedr.insert(p);
        }
        p->addArea(getId());
    }
    return true;
}

void Area::check()
{
    NodePtr cur = NULL;
    for (int i = 0; i < getCurveCount(); ++i) {
        int o;
        CurvePtr c = getCurve(i, o);
        if (c->getStart() == c->getEnd()) {
            continue;
        }
        if (cur != NULL) {
            if (cur == c->getStart()) {
                setOrientation(i, 0);
                o = 0;
            } else {
                assert(cur == c->getEnd());
                setOrientation(i, 1);
                o = 1;
            }
        }
        cur = o == 0 ? c->getEnd() : c->getStart();
    }
}

bool Area::clip(vector<CurvePart*> &cpaths, const box2d &clip, vector<CurvePart*> &result)
{
    for (int i = 0; i < (int) cpaths.size(); ++i) {
        CurvePart *cp = cpaths[i];
        if (clipRectangle(clip, cp->getBounds())) {
            cp->clip(clip, result);
        }
    }
    if (result.size() > 0) {
        bool firstPointIsInside = false;
        vector<vec2d> exteriorPoints;
        for (int i = 0; i < (int) result.size(); ++i) {
            CurvePart *cp = result[i];
            vec2d ps = cp->getXY(0);
            vec2d pe = cp->getXY(cp->getEnd());
            if (i == 0) {
                firstPointIsInside = clipPoint(clip, ps);
            }
            if (!clipPoint(clip, ps)) {
                exteriorPoints.push_back(ps);
            }
            if (!clipPoint(clip, pe)) {
                exteriorPoints.push_back(pe);
            }
        }
        // create missing paths
        int n = exteriorPoints.size();

        assert(n % 2 == 0);
        for (int i = firstPointIsInside ? 0 : 1; i < n; i += 2) {
            vec2d p0 = exteriorPoints[i];
            vec2d p1 = exteriorPoints[(i + 1) % n];
            if (p0.x != p1.x || p0.y != p1.y) {
                CurvePart *cp = new LineCurvePart(p0, p1);
                result.push_back(cp);
            }
        }
        // reorder paths
        CurvePart *cp = result[0];
        vec2d cur = cp->getXY(cp->getEnd());
        for (int i = 1; i < (int) result.size(); ++i) {
            bool ok = false;
            for (int j = i; j < (int) result.size(); ++j) {
                cp = result[j];
                vec2d start = cp->getXY(0);
                vec2d end = cp->getXY(cp->getEnd());
                if (start == cur || end == cur) {
                    if (ok) {
                        vec2d prev = result[i - 1]->getXY(cur, 1);
                        vec2d pi = result[i]->getXY(cur, 1);
                        vec2d pj = result[j]->getXY(cur, 1);
                        float ai = angle(prev - cur, pi - cur);
                        float aj = angle(prev - cur, pj - cur);
                        if (ai < aj) {
                            CurvePart *cq = result[i];
                            result[j] = cq;
                            result[i] = cp;
                        }
                    } else {
                        CurvePart *cq = result[i];
                        result[j] = cq;
                        result[i] = cp;
                        ok = true;
                    }
                }
            }
            if (!ok) {
                assert(false);
                return false;
            }
            cp = result[i];
            vec2d start = cp->getXY(0);
            vec2d end = cp->getXY(cp->getEnd());
            cur = cur == start ? end : start;
        }
    }

    return true;
}

bool Area::equals(set<CurveId> curveList)
{
//    if ((int)curveList.size() < getCurveCount()) {
//        return false;
//    }

    for (int i = 0; i < getCurveCount(); i++) {
        CurvePtr c = getCurve(i);
        if (curveList.find(c->getId()) == curveList.end()) {
            return false;
        }
    }
    return true;
}

bool Area::equals(Area *a, set<CurveId> &visitedCurves, set<NodeId> &visitedNodes)
{
    if (a == NULL) {
        return false;
    }

    if (getCurveCount() != a->getCurveCount()) {
        return false;
    }
    /*check();
    a->check();*/

    bool curveFound;
    for (int i = 0; i < getCurveCount(); i++) {
        CurvePtr c = getCurve(i);
        curveFound = false;
        for (int j = 0; j < a->getCurveCount(); j++) {
            if (c->equals(a->getCurve(j).get(), visitedNodes)) {
                visitedCurves.insert(c->getId());
                curveFound = true;
                break;
            }
        }
        if (! curveFound) {
            return false;
        }
    }
    if (getSubgraph() != NULL) {
        if (a->getSubgraph() == NULL) {
            return false;
        }
        if (getSubgraph()->equals(a->getSubgraph().get()) == false) {
            return false;
        }
    } else {
        if (a->getSubgraph() != NULL) {
            return false;
        }
    }

    return true;
}

}
