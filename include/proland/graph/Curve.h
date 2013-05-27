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

#ifndef _PROLAND_CURVE_H_
#define _PROLAND_CURVE_H_

#include "proland/graph/Node.h"

namespace proland
{

/**
 * Represents a vertex inside a curve.
 * Determines the position and aspect of the curve.
 * @ingroup graph
 * @author Antoine Begault, Guillaume Piolat
 */
PROLAND_API struct Vertex : public vec2d
{
    /**
     * Pseudo curvilinear coordinate along the curve. This coordinate is
     * computed in a recursive way during curve subdivisions (see
     * Curve#flatten()). For top level curves it must be computed explicitely
     * with Curve#computeCurvilinearCoordinates(). The initial value is -1.
     */
    float s;

    /**
     * Real curvilinear coordinate along the curve. This coordinate is not
     * automatically computed. It must be computed explicitely with
     * Curve#computeCurvilinearLength(). The initial value is -1.
     */
    float l;

    /**
     * If this point is a control or regular vertex.
     */
    bool isControl;

    /**
     * Creates a new Vertex.
     *
     * @param x x coordinate.
     * @param y y coordinate.
     * @param s pseudo curvilinear coordinate.
     * @param isControl if this point is a control or regular vertex.
     */
    Vertex(double x, double y, float s, bool isControl) :
        vec2d(x, y), s(s), l(-1), isControl(isControl)
    {
    }

    /**
     * Creates a new Vertex.
     *
     * @param p XY coords
     * @param s pseudo curvilinear coordinate.
     * @param l real curvilinear coordinate.
     * @param isControl if this point is a control or regular vertex.
     */
    Vertex(vec2d p, float s, float l, bool isControl) :
        vec2d(p), s(s), l(l), isControl(isControl)
    {
    }

    /**
     * Creates a copy of the given Vertex.
     *
     * @param p a Vertex.
     */
    Vertex(const Vertex &p) :
        vec2d(p), s(p.s), l(-1), isControl(p.isControl)
    {
    }
};

/**
 * A Curve is made of 2 nodes (start and end points) and a set of vertices.
 * It may be used to represent Areas, but can also be used independently.
 * @ingroup graph
 * @authors AntoineBegault, Guillaume Piolat
 */
PROLAND_API class Curve : public Object
{

public:
    /**
     * Position of a rectangle relatively to this curve.
     * See #getRectanglePosition.
     */
    enum position
    {
        INSIDE, //!< The rectangle is inside the curve
        OUTSIDE, //!< The rectangle is outside the curve
        INTERSECT //!< The rectangle may intersect the curve
    };

    /**
     * Creates a new Curve.
     *
     * @param owner the graph containing this curve.
     */
    Curve(Graph* owner);

    /**
     * Creates a new Curve, with parameters copied from another Curve.
     *
     * @param owner the graph containing this curve.
     * @param c the copied curve, from which this curve takes its data.
     * @param s the start node.
     * @param e the end node.
     */
    Curve(Graph* owner, CurvePtr c, NodePtr s, NodePtr e);

    /**
     * Deletes this Curve.
     */
    virtual ~Curve();

    // -----------------------------------------------------------------------
    // Accessors
    // -----------------------------------------------------------------------

    /**
     * Display method.
     * For debug only.
     */
    virtual void print() const;

    /**
     * Returns this curve's Id
     * For Curve, a CurveId is a direct reference to the Curve, (in opposition
     * to LazyCurves, for which Ids are a unique integer).
     */
    virtual CurveId getId() const;

    /**
     * Returns this curve's parent.
     * The parent curve is the curve from which this curve was created by
     * clipping, or NULL is this curve was not created by clipping (e.g. inside
     * the root graph tile in a GraphProducer).
     */
    virtual CurvePtr getParent() const;

    /**
     * Returns this curve's ancestor (the furthest parent).
     */
    CurvePtr getAncestor() const;

    /**
     * Returns the parent curve's id.
     * See #getParent() and #getId().
     */
    virtual CurveId getParentId() const;

    virtual CurveId getAncestorId() const
    {
        return getAncestor()->getId();
    }

    /**
     * Returns the type of this curve.
     */
    inline int getType() const
    {
        return type;
    }

    /**
     * Returns the width of this curve.
     */
    inline float getWidth() const
    {
        return width;
    }

    /**
     * Returns the number of vertices (including the start and end nodes).
     */
    inline int getSize() const
    {
        return (int) vertices->size() + 2; // Size + start + end => size+2
    }

    /**
     * Returns a Vertex.
     *
     * @param i the rank of the vertex to return.
     * @return the corresponding Vertex (created from a node if necessary).
     */
    Vertex getVertex(int i) const;

    /**
     * Returns the rank of the Vertex that has coordinates P in this curve.
     *
     * @param p a vector of coordinates.
     * @return -1 if not found, else the rank of the Vertex. (0 <= res < getSize()).
     */
    int getVertex(vec2d p) const;

    /**
     * Returns the coordinates of a given Vertex.
     *
     * @param i the rank of the vertex to return.
     * @return the coordinates of this vertex.
     */
    vec2d getXY(int i) const;

    /**
     * Checks if a given Vertex is a control vertex.
     *
     * @param i the rank of the vertex to return.
     * @return true if the given vertex is a control vertex.
     */
    bool getIsControl(int i) const;

    /**
     * Checks if a given Vertex is "smoothed". i.e. if the two vertices around
     * it are Control vertices and if they are aligned.
     * Also returns the two points that would smooth it if return value is false.
     */
    bool getIsSmooth(int i, vec2d &a, vec2d &b) const;

    /**
     * Returns the pseudo curvilinear coordinate at the start node.
     */
    inline float getS0() const
    {
        return s0;
    }

    /**
     * Returns the pseudo curvilinear coordinate of the end node.
     */
    inline float getS1() const
    {
        return s1;
    }

    /**
     * Returns the pseudo curvilinear coordinate of a selected vertex.
     *
     * @param i rank of the vertex to return.
     * @return the pseudo curvilinear coordinate of the selected vertex.
     */
    float getS(int i) const;

    /**
     * Returns the real curvilinear coordinate of a selected vertex.
     *
     * @param i rank of the vertex to return.
     * @return the real curvilinear coordinate of the selected vertex.
     */
    float getL(int i) const;

    /**
     * Returns the bounds of the curve (min and max coords amongst all its
     * vertices).
     */
    box2d getBounds() const;

    /**
     * Returns a Vertex.
     *
     * @param start : the starting node (can be either start or end Node).
     * @param offset rank of the vertex to return.
     * @return the corresponding Vertex.
     */
    Vertex getVertex(NodePtr start, int offset) const;

    /**
     * Returns the coords of a given Vertex.
     *
     * @param start : the starting node (can be either start or end Node).
     * @param offset rank of the vertex to return.
     * @return the coordinates of the selected vertex.
     */
    vec2d getXY(NodePtr start, int offset) const;

    /**
     * Checks if a given vertex is a control or regular vertex.
     *
     * @param start : the starting node (can be either start or end Node).
     * @param offset rank of the vertex to return.
     * @return whether the Vertex is a control vertex or not.
     */
    bool getIsControl(NodePtr start, int offset) const;

    /**
     * Returns the pseudo curvilinear coordinate of a given vertex.
     *
     * @param start : the starting node (can be either start or end Node).
     * @param offset rank of the vertex to return.
     * @return the pseudo curvilinear coordinate of the vertex.
     */
    float getS(NodePtr start, int offset) const;

    /**
     * Returns the real curvilinear coordinate of a given vertex.
     *
     * @param start : the starting node (can be either start or end Node).
     * @param offset rank of the vertex to return.
     * @return the real curvilinear coordinate of the selected vertex.
     */
    float getL(NodePtr start, int offset) const;

    /**
     * Returns the starting Node.
     */
    virtual NodePtr getStart() const;

    /**
     * Returns the ending Node.
     */
    virtual NodePtr getEnd() const;

    /**
     * Returns this curve's first area.
     */
    AreaPtr getArea1() const;

    /**
     * Returns this curve's second area.
     */
    AreaPtr getArea2() const;

    /**
     * Adds a vertex to the curve.
     *
     * @param id a NodeId. Will be stored in start if start == NULL,
     *      else in end.
     * @param isEnd determines if the node is the end node. if false,
     *      it will change this->start; if true && start != NULL,
     *      this->end. Default is true.
     */
    virtual void addVertex(NodeId id, bool isEnd = 1);

    /**
     * Adds a vertex to the curve.
     *
     * @param x Xcoord.
     * @param y Ycoord.
     * @param s pseudo curvilinear value.
     * @param isControl if true, the vertex will be a control vertex of
     *      this curve.
     */
    virtual void addVertex(double x, double y, float s, bool isControl);

    /**
     * Adds a vertex to the curve.
     *
     * @param rank where to insert the Vertex.
     * @param x Xcoord.
     * @param y Ycoord.
     * @param s pseudo curvilinear value.
     * @param isControl if true, the vertex will be a control vertex of
     *      this curve.
     */
    virtual void addVertex(vec2d pt, int rank, bool isControl);

    /**
     * Adds a vertex to the curve.
     *
     * @param p a vector containing the x&y coordinates of the vertex.
     * @param s the s coordinate.
     * @param l the l coordinate.
     * @param isControl if true, the vertex will be a control vertex of
     *      this curve.
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
     * Merge vertices in Curves which are too near from one another.
     */
    void decimate(float minDistance);

    /**
     * Returns the opposite of the given extremity. If the given extremity is
     * the start of the curve, returns the end, and vice versa.
     *
     * @param n the start or end of this curve.
     */
    NodePtr getOpposite(const NodePtr n) const;

    /**
     * Returns the next curve after this one at the given node, in
     * clockwise order. The curves connected to the given node (and that are
     * in the given set of curves) are ordered by using their first control
     * point after this node to compute an angle (relatively to this curve),
     * which is then used to order them.
     *
     * @param n the start or end of this curve.
     * @param excludedCurves the curves that must NOT be taken into account.
     * @param rerverse determines in which order the curve must be selected.
     *      Default value is false, which means clockwise order.
     */
    CurvePtr getNext(const NodePtr n, const std::set<CurveId> &excludedCurves, bool reverse = false) const;

    /**
     * Computes the curvilinear length corresponding to the given s
     * coordinate.
     *
     * @param s a pseudo curvilinear coordinate (see Vertex#s).
     * @param p where to store the x,y coordinates corresponding to s, or NULL
     * if these coordinates are not needed.
     * @param n where to store the normal to the curve at s, or NULL if this
     * normal is not needed.
     */
    float getCurvilinearLength(float s, vec2d *p, vec2d *n) const;

    /**
     * Computes the pseudo curvilinear coordinate corresponding to the given l
     * coordinate.
     *
     * @param l a curvilinear coordinate (see Vertex#l).
     * @param p where to store the x,y coordinates corresponding to l, or NULL
     * if these coordinates are not needed.
     * @param n where to store the normal to the curve at l, or NULL if this
     * normal is not needed.
     */
    float getCurvilinearCoordinate(float l, vec2d *p, vec2d *n) const;

    /**
     * Returns the position of the given rectangle relatively to this curve.
     * The given width and cap parameters are used to define a stroked curve,
     * i.e., an area. This area is then used to compute the position of the
     * given rectangle (inside, outside or intersecting this area). Note that
     * these computations are NOT based on the limit curve, but on the
     * polyline defined by the curve control points.
     *
     * @param width the width to be used to define the stroked curve.
     * @param cap the length that must be removed at both extremities to
     *      define the stroked curve (the cap style is always "butt").
     * @param r the rectangle whose position must be computed.
     * @param coords where to put the precise position of r, or NULL is this
     *      information is not needed. If not NULL, coords must point to an array
     *      of six floats. If r is inside the curve, then these floats are set
     *      with the coordinates of the curve segment (a,b) inside which r
     *      resides, as follows: a.x, a.y, b.x-a.x, b.y-a.y, a.s, b.s.
     */
    position getRectanglePosition(float width, float cap, const box2d &r, double *coords) const;

    /**
     * Returns the position of the given triangle relatively to this curve.
     * The given width and cap parameters are used to define a stroked curve,
     * i.e., an area. This area is then used to compute the position of the
     * given triangle (inside, outside or intersecting this area). Note that
     * these computations are NOT based on the limit curve, but on the
     * polyline defined by the curve control points.
     *
     * @param width the width to be used to define the stroked curve.
     * @param cap the length that must be removed at both extremities to
     *      define the stroked curve (the cap style is always "butt").
     * @param t the triangle whose position must be computed.
     * @param r a bounding rectangle for coarse detection.
     * @param coords where to put the precise position of t, or NULL is this
     *      information is not needed. If not NULL, coords must point to an array
     *      of six floats. If t is inside the curve, then these floats are set
     *      with the coordinates of the curve segment (a,b) inside which t
     *      resides, as follows: a.x, a.y, b.x-a.x, b.y-a.y, a.s, b.s.
     */
    position getTrianglePosition(float width, float cap, const vec2d* t, const box2d &r, double *coords) const;

    /**
     * Returns true if the given point is inside this curve. Note that this
     * computation is NOT based on the limit curves, but on the polyline
     * defined by the curve control points.
     *
     * @param p the point to check.
     */
    bool isInside(const vec2d &p) const;

    /**
     * Returns true if this curve is clockwise ordered
     */
    bool isDirect() const;

    // -----------------------------------------------------------------------
    // Mutators
    // -----------------------------------------------------------------------

    /**
     * Sets the state of a Vertex.
     *
     * @param i the rank of the vertex to change.
     * @param c whether the vertex must be a control or regular vertex.
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
     * Subdivides this curve where necessary to satisfy the given maximum
     * error bound. The extremities are not changed, but the intermediate
     * points are replaced with new points, computed by subdividing the
     * Bezier arcs recursively, until the distance between the limit curve
     * and the actual curve is less than sqrt(squareFlatness). The s
     * coordinate of the new points is automatically computed, but their l
     * coordinate is reset to -1.
     *
     * @param squareFlatness square of the maximum allowed distance between
     * the limit curve and its polyline approximation.
     */
    virtual void flatten(float squareFlatness);

    /**
     * Computes the Vertex#s coordinates for every Vertex of this curve.
     * This method also computes #s0 and #s1.
     */
    void computeCurvilinearCoordinates();

    /**
     * Computes the Vertex#l coordinates for every Vertex of this curve.
     * This method also computes #l.
     */
    float computeCurvilinearLength();

    /**
     * Checks if two curves are identicals.
     * Used for comparing graphs.
     *
     * @param c another curve.
     * @param visited List of nodes that were already checked + those
     *      checked during this call.
     */
    bool equals(Curve *c, set<NodeId> &visited) const;

    /**
     * Removes the references to this curve from its nodes.
     */
    virtual void clear();

    /**
     * Changes the orientation of this curve.
     */
    virtual void invert();

    /**
     * Sets the owner Graph value.
     */
    void setOwner(Graph* owner);

    /**
     * Sets the parent curve.
     * The parent curve is the curve from which this curve was created
     * by clipping, or NULL is this curve was not created by clipping (e.g.
     * inside the root graph tile in a GraphProducer).
     *
     * @param c the new parent Curve.
     */
    inline void setParent(CurvePtr c)
    {
        parent = c;
    }

    /**
     * Adds an area to the curve. 2 Areas can be stored (one for each side of
     * the curve).
     *
     * @param a the id of the area to add.
     */
    virtual void addArea(AreaId a);

    inline Graph *getOwner() const
    {
        return owner;
    }

    /**
     * Basic runtime checking of the integrity of the Curve.
     * Currently: verify that each point is different from itsd neighbours.
     */
    void check();

    /**
     * Remove duplicate vertices (coming from bad sources).
     */
    void removeDuplicateVertices();

protected:
    /**
     * This curve's id. NULL_ID if this Curve is not a lazycurve.
     */
    CurveId id;

    /**
     * The graph containing this curve.
     */
    Graph* owner;

    /**
     * The parent curve.
     */
    CurvePtr parent;

    /**
     * Type of the curve.
     */
    int type;

    /**
     * Width of the curve.
     */
    float width;

    /**
     * Pseudo curvilinear coordinate at rank 0.
     */
    float s0;

    /**
     * Pseudo curvilinear coordinate at end point.
     */
    float s1;

    /**
     * Length of the curve (== number of nodes).
     */
    float l;

    /**
     * Start node.
     */
    mutable NodePtr start;

    /**
     * End node.
     */
    mutable NodePtr end;

    /**
     * List of vertices describing the curve.
     */
    vector<Vertex> *vertices;

    /**
     * The XY min & max values of this curve.
     * Used for clipping.
     */
    mutable box2d *bounds;

    /**
     * First area.
     */
    mutable AreaId area1;

    /**
     * Second area.
     */
    mutable AreaId area2;

    /**
     * Sets the parent Id.
     * Basic Curve version : only sets parent to id.ref.
     * See #setParent().
     *
     * @param id a curve's Id.
     */
    inline void setParentId(CurveId id)
    {
        parent = id.ref;
    }

    /**
     * Resets #bounds.
     */
    void resetBounds() const;

    /**
     * Removes an area from the curve.
     *
     * @param a the id of the area to remove.
     */
    void removeArea(AreaId a);

    friend class BasicCurvePart;

    friend class FlatteningCurveIterator;

    friend class Graph;

    friend class BasicGraph;

    friend class LazyGraph;

    friend class Node;

    friend class Area;

    friend class LazyCurve;
};

}

#endif
