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

#ifndef _PROLAND_LAZYCURVE_H_
#define _PROLAND_LAZYCURVE_H_

#include "proland/graph/Curve.h"

namespace proland
{

/**
 * A Curve is made of 2 nodes (start and end points) and a set of controlPoints.
 * It may be used to represent Areas, but can also be used independently.
 * This is the 'Lazy' version of Curves. Allows real-time loading for LazyGraphs.
 * Can be deleted and reloaded at any time depending on the needs.
 * @ingroup graph
 * @author Antoine Begault
 */
PROLAND_API class LazyCurve : public Curve
{
public:
    /**
     * Creates a new LazyCurve.
     *
     * @param owner the graph containing  this curve.
     * @param id this curve's id (determined by LazyGraph).
     */
    LazyCurve(Graph* owner, CurveId id);

    /**
     * Creates a new LazyCurve.
     *
     * @param owner the graph containing this curve.
     * @param c the parent curve, from which this curve was clipped.
     * @param s the start node.
     * @param e the end node.
     */
    LazyCurve(Graph* owner, CurveId id, NodeId s, NodeId e);

    /**
     * Deletes this LazyCurve.
     */
    virtual ~LazyCurve();

    /**
     * Returns this curve's Id
     * For LazyCurve, a CurveId is a unique number, assigned by the owner Graph,
     * (in opposition to Curves, for which Ids are a direct reference to the Curve).
     */
    virtual CurveId getId() const;

    /**
     * Returns this curve's parent.
     * Should allways return NULL, because LazyGraph are only used on top of
     * the graph, and thus have no parent.
     */
    virtual CurvePtr getParent() const;

    /**
     * Returns the starting Node.
     */
    virtual NodePtr getStart() const;

    /**
     * Returns the ending Node.
     */
    virtual NodePtr getEnd() const;

    /**
     * Adds a vertex to the curve.
     *
     * @param id a NodeId. Will be stored in start if start == NULL, else
     *      in end.
     * @param isEnd determines if the node is the end node. if false, it will
     *      change this->start; if true && start != NULL, this->end. Default
     *      is true.
     */
    virtual void addVertex(NodeId id, bool isEnd = 1);

    /**
     * Adds a vertex to the curve.
     *
     * @param id a NodeId. Will be stored in start if start == NULL, else
     *      in end.
     * @param isEnd determines if the node is the end node. if false, it will
     *      change this->start; if true && start != NULL, this->end. Default
     *      is true.
     */
    virtual void loadVertex(NodeId id, bool isEnd = 1);

    /**
     * Adds a vertex to the curve.
     *
     * @param x Xcoord.
     * @param y Ycoord.
     * @param s pseudo curvilinear value.
     * @param isControl if true, the vertex will be a control point of this curve.
     */
    virtual void addVertex(double x, double y, float s, bool isControl);

    /**
     * Adds a vertex to the curve.
     *
     * @param rank where to insert the Vertex.
     * @param x Xcoord.
     * @param y Ycoord.
     * @param s pseudo curvilinear value.
     * @param isControl if true, the vertex will be a control point of this curve.
     */
    virtual void addVertex(vec2d pt, int rank, bool isControl);

    /**
     * Adds a vertex to the curve.
     *
     * @param p a vector containing the x&y coordinates of the vertex.
     * @param s the s coordinate.
     * @param l the l coordinate.
     * @param isControl if true, the vertex will be a control point of this curve.
     */
    virtual void addVertex(const vec2d &p, float s, float l, bool isControl);

    /**
     * Adds a vertex to the curve.
     *
     * @param pt a model Vertex, from which the values will be copied.
     */
    virtual void addVertex(const Vertex &pt);

    /**
     * Adds a list of vertex to the curve.
     *
     * @param v the list of vertices.
     */
    virtual void addVertices(const vector<vec2d> &v);

    /**
     * Remove the i'th vertex from the list.
     *
     * @param i the rank of the vertex to remove.
     */
    virtual void removeVertex(int i);

    /**
     * Sets the state of a Vertex.
     *
     * @param i the rank of the vertex to change.
     * @param c whether the vertex must be a control point or not.
     */
    virtual void setIsControl(int i, bool c);

    /**
     * Changes the S coordinate of a Vertex.
     *
     * @param i the rank of the vertex to edit.
     * @param s the new S coordinate.
     */
    virtual void setS(int i, float s);

    /**
     * Sets the XY coords of a vertex.
     *
     * @param i the rank of the vertex to move.
     * @param p the new positions.
     */
    virtual void setXY(int i, const vec2d &p);

    /**
     * Sets this curve's width.
     *
     * @param width width of the curve.
     */
    virtual void setWidth(float width);

    /**
     * Sets this curve's type.
     *
     * @param type type of the curve.
     */
    virtual void setType(int type);

    /**
     * Adds a vertex to the curve.
     *
     * @param x Xcoord.
     * @param y Ycoord.
     * @param s pseudo curvilinear value.
     * @param isControl if true, the vertex will be a control point of this
     *      curve.
     */
    void loadVertex(float x, float y, float s, bool isControl);

    /**
     * Removes the references to this curve from its nodes.
     */
    virtual void clear();

    /**
     * Changes the orientation of this curve.
     */
    virtual void invert();

    /**
     * Adds an area to the curve. 2 Areas can be stored (one for each side
     * of the curve).
     *
     * @param a the id of the area to add.
     */
    virtual void addArea(AreaId a);

    /**
     * Adds an area to the curve. 2 Areas can be stored (one for each side
     * of the curve).
     *
     * @param a the id of the area to add.
     */
    void loadArea(AreaId a);

protected:
    /**
     * Calls the LazyGraph#releaseCurve() method.
     * See Object#doRelease().
     */
    virtual void doRelease();

private:
    /**
     * The parent Area's id. If parentId == id, there's no parent.
     */
    CurveId parentId;

    /**
     * The #start NodeId
     */
    NodeId startId;

    /**
     * The #end NodeId
     */
    NodeId endId;

    /**
     * Removes an area from the curve.
     *
     * @param a the id of the area to remove.
     */
    virtual void removeArea(AreaId a);

    /**
     * Sets the parent Id.
     * See #setParent().
     *
     * @param id a curve's Id.
     */
    virtual void setParentId(CurveId id);

    friend class LazyGraph;

};

}

#endif
