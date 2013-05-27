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

#include "proland/rivers/graph/LazyHydroCurve.h"

#include "proland/rivers/graph/LazyHydroGraph.h"

namespace proland
{

LazyHydroCurve::LazyHydroCurve(Graph* owner, CurveId id) :
    HydroCurve(owner)
{
    this->id = id;
    this->startId.id = NULL_ID;
    this->endId.id = NULL_ID;
    this->parentId.id = NULL_ID;
}

LazyHydroCurve::LazyHydroCurve(Graph* owner, CurveId id, NodeId s, NodeId e) :
    HydroCurve(owner), startId(s), endId(e)
{
    this->id = id;
    this->parentId.id = NULL_ID;
    this->s0 = 0;
    this->s1 = 1;
}

LazyHydroCurve::~LazyHydroCurve()
{
    if (owner != NULL) {
        dynamic_cast<LazyGraph*>(owner)->deleteCurve(id);
    }
}

CurveId LazyHydroCurve::getId() const
{
    return id;
}

CurvePtr LazyHydroCurve::getParent() const
{
    return NULL;
}

NodePtr LazyHydroCurve::getStart() const
{
    if (startId.id == NULL_ID) {
        start = NULL;
    } else if (start == NULL) {
        start = owner->getNode(startId);
    }
    return start;
}

NodePtr LazyHydroCurve::getEnd() const
{
    if (endId.id == NULL_ID) {
        end = NULL;
    } else if (end == NULL) {
        end = owner->getNode(endId);
    }
    return end;
}

void LazyHydroCurve::clear()
{
    Curve::clear();
    startId.id = NULL_ID;
    endId.id = NULL_ID;
}

void LazyHydroCurve::doRelease()
{
    if (owner != NULL) {
        start = NULL;
        end = NULL;
        dynamic_cast<LazyGraph*>(owner)->releaseCurve(id);
    } else {
        delete this;
    }
}

void LazyHydroCurve::setParentId(CurveId id)
{
    this->parentId = id;
}

void LazyHydroCurve::addVertex(NodeId id, bool isEnd)
{
    dynamic_cast<LazyGraph*>(owner)->getCurveCache()->add(this, true);
    if ((start == NULL && startId.id == NULL_ID)  || isEnd == 0) {
        start = NULL;
        startId = id;
    } else {
        end = NULL;
        endId = id;
    }
}

void LazyHydroCurve::loadVertex(float x, float y, float s, bool isControl)
{
    Curve::addVertex(x, y, s, isControl);
}

void LazyHydroCurve::loadVertex(NodeId id, bool isEnd)
{
    if ((start == NULL && startId.id == NULL_ID)  || isEnd == 0) {
        start = NULL;
        startId = id;
    } else {
        end = NULL;
        endId = id;
    }
}

void LazyHydroCurve::addVertex(float x, float y, float s, bool isControl)
{
    dynamic_cast<LazyGraph*>(owner)->getCurveCache()->add(this, true);
    Curve::addVertex(x, y, s, isControl);
}

void LazyHydroCurve::addVertex(vec2d pt, int rank, bool isControl)
{
    dynamic_cast<LazyGraph*>(owner)->getCurveCache()->add(this, true);
    Curve::addVertex(pt, rank, isControl);
}

void LazyHydroCurve::addVertex(const vec2d &p, float s, float l, bool isControl)
{
    dynamic_cast<LazyGraph*>(owner)->getCurveCache()->add(this, true);
    Curve::addVertex(p, s, l, isControl);
}

void LazyHydroCurve::addVertex(const Vertex &pt)
{
    dynamic_cast<LazyGraph*>(owner)->getCurveCache()->add(this, true);
    Curve::addVertex(pt);
}

void LazyHydroCurve::addVertices(const vector<vec2d> &v)
{
    dynamic_cast<LazyGraph*>(owner)->getCurveCache()->add(this, true);
    Curve::addVertices(v);
}


void LazyHydroCurve::removeVertex(int i)
{
    dynamic_cast<LazyGraph*>(owner)->getCurveCache()->add(this, true);
    Curve::removeVertex(i);
}

void LazyHydroCurve::setIsControl(int i, bool c)
{
    dynamic_cast<LazyGraph*>(owner)->getCurveCache()->add(this, true);
    Curve::setIsControl(i, c);
}

void LazyHydroCurve::setS(int i, float s)
{
    dynamic_cast<LazyGraph*>(owner)->getCurveCache()->add(this, true);
    Curve::setS(i, s);
}

void LazyHydroCurve::setXY(int i, const vec2d &p)
{
    dynamic_cast<LazyGraph*>(owner)->getCurveCache()->add(this, true);
    Curve::setXY(i, p);
}

void LazyHydroCurve::setWidth(float width)
{
    dynamic_cast<LazyGraph*>(owner)->getCurveCache()->add(this, true);
    Curve::setWidth(width);
}

void LazyHydroCurve::setType(int type)
{
    dynamic_cast<LazyGraph*>(owner)->getCurveCache()->add(this, true);
    Curve::setType(type);
}

void LazyHydroCurve::addArea(AreaId a)
{
    dynamic_cast<LazyGraph*>(owner)->getCurveCache()->add(this, true);
    Curve::addArea(a);
}

void LazyHydroCurve::removeArea(AreaId a)
{
    dynamic_cast<LazyGraph*>(owner)->getCurveCache()->add(this, true);
    Curve::removeArea(a);
}

void LazyHydroCurve::loadArea(AreaId a)
{
    Curve::addArea(a);
}

void LazyHydroCurve::invert()
{
    dynamic_cast<LazyGraph*>(owner)->getCurveCache()->add(this, true);
    Curve::invert();
    NodeId tmp = startId;
    startId = endId;
    endId = tmp;
}

}
