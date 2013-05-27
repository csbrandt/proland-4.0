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

#ifndef _PROLAND_LAZYNODE_H_
#define _PROLAND_LAZYNODE_H_

#include "proland/graph/Node.h"

namespace proland
{

/**
 * A Node is described by it's XY coordinates.
 * It is used to represent start and end points of curves, intersections...
 * Several Curves can share the same Node.
 * This is the 'Lazy' version of Nodes. Allows real-time loading for LazyGraphs.
 * Can be deleted and reloaded at any time depending on the needs.
 * @ingroup graph
 * @author Antoine Begault
 */
PROLAND_API class LazyNode : public Node
{
public:
    /**
     * Creates a new Node.
     *
     * @param owner the graph containing this node.
     * @param id this Node's id (determined by LazyGraph).
     * @param x this node's X coord.
     * @param y this node's Y coord.
     */
    LazyNode(Graph* owner, NodeId id, double x, double y);

    /**
     * Deletes this LazyNode.
     */
    virtual ~LazyNode();

    /**
     * Returns this Node's Id.
     * For LazyNode, a NodeId is a unique number, assigned by the owner Graph,
     * (in opposition to Nodes, for which Ids are a direct reference to the Node).
     */
    virtual NodeId getId() const;

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

protected:
    /**
     * Calls the LazyGraph#releaseNode() method.
     * See Object#doRelease().
     */
    virtual void doRelease();

private:
    /**
     * This node's Id.
     */
    NodeId id;

    /**
     * Adds a Curve to the Curves list, but doesn't change the owner's cache.
     *
     * @param c the id of the Curve to add.
     */
    void loadCurve(CurveId c);

    friend class LazyGraph;
};

}

#endif
