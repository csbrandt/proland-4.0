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

#ifndef _PROLAND_NODE_H_
#define _PROLAND_NODE_H_

#include "proland/graph/Graph.h"

namespace proland
{

/**
 * A Node is described by it's XY coordinates.
 * It is used to represent start and end points of curves, intersections...
 * Several Curves can share the same Node.
 * @ingroup graph
 * @author Antoine Begault
 */
PROLAND_API class Node : public Object
{
public:
    /**
     * Creates a new Node.
     *
     * @param owner the graph containing this node.
     * @param x this node's X coord.
     * @param y this node's Y coord.
     */
    Node(Graph* owner, double x, double y);

    /**
     * Deletes this Node.
     */
    virtual ~Node();

    /**
     * Returns this Node's Id.
     * For Node, a NodeId is a direct reference to the Node (in opposition to
     * LazyNodes, for which Ids are a unique integer).
     */
    virtual NodeId getId() const;

    /**
     * Returns the XY coordinates of this Node.
     */
    inline vec2d getPos() const
    {
        return pos;
    }

    /**
     * Returns the number of Curves using this Node.
     */
    virtual int getCurveCount() const;

    /**
     * Returns the number of curves of a given set that use this Node.
     *
     * @param includedCurves a set of Curves.
     * @return the number of Curves that uses this Node amongst the given set.
     */
    int getCurveCount(const set<CurveId> &includedCurves) const;

    /**
     * Gets a curve at a given index.
     *
     * @param i the index of the curve to return.
     * @return the i'th curve, or NULL if i is out of range.
     */
    virtual CurvePtr getCurve(int i) const;

    /**
     * Returns a curve that connects this node to the given node. There may be
     * zero or more such curve. This method returns only one of them, or NULL
     * is there is no such curve.
     *
     * @param n a graph node.
     */
    CurvePtr getCurve(const NodePtr n) const;

    /**
     * Returns the opposite of the given curve extremity. This method must be
     * used only for nodes that connect exactly two curves. Given the
     * extremity of one of these two curves, it returns the extremity of the
     * other curve (here "the extremity of a curve" is the extremity that is
     * not equal to this node).
     *
     * @param n the extremity of one of the two curves connected to this node.
     */
    NodePtr getOpposite(const NodePtr n) const;

    /**
     * Sets the coordinates of this Node.
     *
     * @param v a vector containing X&Y coordinates.
     */
    void setPos(const vec2d &v);

    void setOwner(Graph *owner);

    /**
     * Adds a Curve to the Curves list.
     *
     * @param c the id of the Curve to add.
     */
    virtual void addCurve(CurveId c);

    /**
     * Removes a Curve from the Curves list.
     *
     * @param c the id of the Curve to remove.
     */
    virtual void removeCurve(CurveId c);

private:
    /**
     * The graph containing this Node.
     */
    Graph* owner;

    /**
     * X & Y coordinates of this Node.
     */
    vec2d pos;

    /**
     * List of Curves that use this Node.
     */
    mutable vector<CurveId> curves;

    friend class Graph;

    friend class BasicGraph;

    friend class LazyGraph;

    friend class LazyNode;

    friend class Curve;

    friend class LazyCurve;
};

}

#endif
