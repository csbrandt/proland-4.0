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

#ifndef _PROLAND_AREA_H_
#define _PROLAND_AREA_H_

#include "proland/graph/Curve.h"
#include "proland/math/seg2.h"

namespace proland
{

/**
 * An Area is described by 1 or more curves. It may contain a subgraph.
 * This is used to describe pastures, lakes...
 * @ingroup graph
 * @author Antoine Begault, Guillaume Piolat
 */
PROLAND_API class Area : public Object
{
public:
    // -----------------------------------------------------------------------
    // Constructor & destructor
    // -----------------------------------------------------------------------

    /**
     * Creates a new Area.
     *
     * @param owner the Graph that contains this area.
     */
    Area(Graph *owner);

    /**
     * Deletes this Area.
     */
    virtual ~Area();

    // -----------------------------------------------------------------------
    // Accessors
    // -----------------------------------------------------------------------

    /**
     * Displays the content of the area.
     * Debug only.
     */
    void print() const;

    /**
     * Returns this area's Id.
     * For Area, an AreaId is a direct reference to the Area, (in opposition to LazyAreas, for which Ids are a unique integer).
     */
    virtual AreaId getId()const;

    /**
     * Returns the parent Area of this area.
     * The parent area is the area from which this
     * area was created by clipping, or NULL is this area was not created by
     * clipping (e.g. inside the root graph tile in a GraphProducer).
     */
    virtual AreaPtr getParent() const;

    /**
     * Returns the parent Area's id.
     * See #getParent() and #getId().
     */
    virtual AreaId getParentId() const;

    /**
     * Returns this area's Ancestor Area (The furthest parent).
     * This corresponds to the area in the root graph. See #getParent().
     */
    AreaPtr getAncestor();

    /**
     * Returns the info of this Area.
     * Info is a randomly generated number.
     */
    int getInfo() const;

    /**
     * Returns the number of curves forming this area.
     */
    virtual int getCurveCount() const;

    /**
     * Returns a curve.
     *
     * @param i the rank of the curve to be returned.
     * @return the i'th curve describing this area, or NULL if i is out of range.
     */
    virtual CurvePtr getCurve(int i) const;

    /**
     * Returns a curve.
     *
     * @param i the rank of the curve to be returned.
     * @param orientation will contain the orientation of the i'th curve in this area.
     * @return the i'th curve describing this area, or NULL if i is out of range.
     */
    virtual CurvePtr getCurve(int i, int& orientation) const;

    /**
     * Returns the Graph contained in this area if any. Null otherwise.
     */
    GraphPtr getSubgraph() const;

    /**
     * Returns the bounding box of this area.
     * This box does NOT take the curve widths into account.
     */
    box2d getBounds() const;

    /**
     * Returns true if the given point is inside this area. Note that this
     * computation is NOT based on the limit curves, but on the polylines
     * based on the control points of the curves that form this area.
     *
     * @param p a point. The curve widths are not taken into account.
     */
    bool isInside(const vec2d &p) const;

    /**
     * Returns the position of the given rectangle relatively to this area.
     * Note that this computation is NOT based on the limit curves, but on
     * the polylines based on the control points of the curves that form
     * this area. The curve widths are not taken into account.
     *
     * @param r a rectangle.
     */
    Curve::position getRectanglePosition(const box2d &r) const;

    /**
     * Returns the position of the given triangle relatively to this area.
     * Note that this computation is NOT based on the limit curves, but on
     * the polylines based on the control points of the curves that form
     * this area. The curve widths are not taken into account.
     *
     * @param t points the 3 summits of the triangle.
     */
    Curve::position getTrianglePosition(const vec2d* t) const;

    /**
     * Checks if the given curve list corresponds to this area.
     *
     * @param curveList a set of curveIds.
     * @return true if all curves of this area are in the list and vice-versa.
     */
    bool equals(set<CurveId> curveList);

    /**
     * Checks if two areas are equals. Used for unit tests when comparing graphs.
     *
     * @param a another area
     * @param visitedCurves curves that were already checked (+ those checked in this function).
     * @param visitedNodes nodes that were already checked (+ those checked in this function).
     * @return true if EVERY field correspond (Except info, which is generated randomly at area creation).
     */
    bool equals(Area *a, set<CurveId> &visitedCurves, set<NodeId> &visitedNodes);

    /**
     * Sets orientation for a given curve.
     *
     * @param i rank of the curve to modify.
     * @param orientation the new orientation of the curve.
     */
    virtual void setOrientation(int i, int orientation);

    /**
     * Sets #info.
     *
     * @param info the new info.
     */
    void setInfo(int info);

    /**
     * Inverts a given curve in this area.
     *
     * @param cid the curveId of the curve to invert.
     */
    virtual void invertCurve(CurveId cid);

    void setOwner(Graph* owner);

    /**
     * Checks if this area is well defined.
     * In other words, it checks if the curves are oriented correctly, and in a correct order, forming a loop.
     */
    void check();

    /**
     * Sets the parent area.
     * The parent area is the area from which this area was created by clipping,
     * or NULL is this area was not created by clipping (e.g. inside the root graph tile in a GraphProducer).
     *
     * @param a the new parent Area.
     */
    virtual void setParent(AreaPtr a);

    /**
     * Adds a curve to this area.
     *
     * @param id the added curve's Id.
     * @param orientation the orientation to add the curve.
     */
    virtual void addCurve(CurveId id, int orientation);

    /**
     * Sets this area's subgraph.
     *
     * @param g a Graph.
     */
    virtual void setSubgraph(Graph *g);

    /**
     * Clips an area given as a list of curve parts. The given curve parts must
     * form a closed loop oriented in counter clockwise order, and their
     * orientations must be consistent with this order. The result is a new
     * area also described with a list of curve parts (it can be empty).
     *
     * @param cpaths the area to be clipped.
     * @param clip the clip region. This region must be an infinite horizontal
     * or vertical slab.
     * @param result where the clipped area must be stored.
     */
    static bool clip(vector<CurvePart*> &cpaths, const box2d &clip, vector<CurvePart*> &result);

    /**
     * Returns true if the area is oriented in clockwise order.
     */
    bool isDirect() const;

    /**
     * Returns true if the given list of points are oriented in clockwise order.
     *
     * @param points a vector of points.
     * @param start rank of the starting point.
     * @param end rank of the end point.
     */
    static bool isDirect(const vector<Vertex> &points, int start, int end);

private:
    /**
     * This area's ID (NULL_ID if it's a Basic Area, and a non-null positive int if this is a Lazy Area).
     * This is stored here because it allows to check whether the area is a lazy area
     * or not during the delete call.
     */
    AreaId id;

    /**
     * The graph containing this area.
     */
    Graph * owner;

    /**
     * The parent area. The parent area is the area from which this
     * area was created by clipping, or NULL is this area was not created by
     * clipping (e.g. inside the root graph tile in a GraphProducer).
     */
    AreaPtr parent;

    /**
     * Info about this area.
     */
    int info;

    /**
     * The list of the curves forming this area.
     */
    mutable vector<pair<CurvePtr, int> > curves;

    /**
     * Area's subgraph.
     * A BasicGraph, containing it's own nodes, curves and areas, and possibly other subgraphs.
     */
    GraphPtr subgraph;

    /**
     * Bounds of the area : x and y limits (max and min for each).
     * This helps to determine if the area will be clipped by a box or not.
     * Caution : this is only updated & used for clipping. It needs to be recomputed before using it.
     * See #getBounds().
     */
    mutable box2d *bounds;

    /**
     * Sets the parent Id.
     * Basic Area version : only sets parent to id.ref.
     * See #setParent().
     *
     * @param id the new parent's Id.
     */
    virtual void setParentId(AreaId id);

    /**
     * Switch 2 curves of this area.
     *
     * @param id1 the first curve's id.
     * @param id2 the second curve's id.
     */
    virtual void switchCurves(int curve1, int curve2);

    /**
     * Removes a curve from the curves list.
     *
     * @param the index of the curve to remove.
     */
    virtual void removeCurve(int index);

    /**
     * Resets the the value of the variable #bounds.
     */
    void resetBounds() const;

    /**
     * If necessary, reorders the curves to order them counter-clockwise, consistently with their orientations.
     */
    void build();

    /**
     * Tries to create a valid area from a starting curve.
     * a "valid area" is a list of curves that forms a loop, counter-clockwise ordered.
     *
     * @param p starting curve.
     * @param start starting node in P.
     * @param cur a second node. Will determine the orientation of the search.
     * @param excludedCurves curves we don't want to be part of the area. Can also contains curves that cannot form any area.
     * @param visited the curves that were added to this area with their orientation = 0.
     * @param visitedr the curves that were added to this area with their orientation = 1.
     * @return true if a valid area was found.
     */
    bool build(const CurvePtr p, NodePtr start, NodePtr cur, const set<CurveId> &excludedCurves, set<CurvePtr> &visited, set<CurvePtr> &visitedr);

    friend class Graph;

    friend class BasicGraph;

    friend class LazyGraph;

    friend class LazyArea;

    friend class Curve;
};

}

#endif
