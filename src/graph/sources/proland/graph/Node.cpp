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

#include "proland/graph/Node.h"

#include "proland/graph/Curve.h"

namespace proland
{


Node::Node(Graph* owner, double x, double y) :
    Object("Node"), owner(owner), pos(x, y)
{
}

Node::~Node()
{
    curves.clear();
}

NodeId Node::getId() const
{
    NodeId i;
    i.ref = (Node*) this;
    return i;
}

int Node::getCurveCount() const
{
    return curves.size();
}

int Node::getCurveCount(const set<CurveId> &includedCurves) const
{
    int total = 0;
    for (int i = 0; i < getCurveCount(); ++i) {
        if (includedCurves.find(curves[i]) != includedCurves.end()) {
            ++total;
        }
    }
    return total;
}

CurvePtr Node::getCurve(int i) const
{
    return owner->getCurve(curves[i]);
}

CurvePtr Node::getCurve(const NodePtr n) const
{
    CurvePtr result = NULL;
    for (int i = 0; i < getCurveCount(); ++i) {
        CurvePtr c = getCurve(i);
        if (c->getOpposite((Node*) this) == n) {
            assert(result == NULL);
            result = c;
        }
    }
    return result;
}

NodePtr Node::getOpposite(const NodePtr n) const
{
    assert(getCurveCount() == 2);
    CurvePtr c0 = getCurve(0);
    CurvePtr c1 = getCurve(1);
    NodePtr c0a = c0->getStart();
    NodePtr c0b = c0->getEnd();
    NodePtr c1a = c1->getStart();
    NodePtr c1b = c1->getEnd();
    if (c0a != this && c0a != n) {
        return c0a;
    } else if (c0b != this && c0b != n) {
        return c0b;
    } else if (c1a != this && c1a != n) {
        return c1a;
    } else if (c1b != this && c1b != n) {
        return c1b;
    }
    return NULL;
}

void Node::setPos(const vec2d &p)
{
    pos = p;
    for (int i = 0; i < getCurveCount(); ++i) {
        getCurve(i)->resetBounds();
    }
}

void Node::addCurve(CurveId c)
{
    if (find(curves.begin(), curves.end(), c) == curves.end()) {
        curves.push_back(c);
    }
}

void Node::removeCurve(CurveId c)
{
    vector<CurveId>::iterator i = find(curves.begin(), curves.end(), c);
    if (i != curves.end()) {
        curves.erase(i);
    }
}

void Node::setOwner(Graph* owner)
{
    this->owner = owner;
}

}
