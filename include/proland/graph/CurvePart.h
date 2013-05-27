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

#ifndef _PROLAND_CURVEPART_H_
#define _PROLAND_CURVEPART_H_

#include "proland/graph/Curve.h"

namespace proland
{

/**
 * A part of a curve. This part is defined by a curve, and by two indexes that
 * give the start and end points of this curve part inside the whole curve.
 * @ingroup graph
 * @author Antoine Begault
 */
PROLAND_API class CurvePart
{
public:
    /**
     * Creates a new CurvePart.
     */
    CurvePart();

    /**
     * Deletes this CurvePart.
     */
    virtual ~CurvePart();

    /**
     * Returns the original curve's Id.
     */
    virtual CurveId getId() const;

    /**
     * Returns the original curve's parent.
     */
    virtual CurveId getParentId() const;

    /**
     * Returns the original curve's type.
     */
    virtual int getType() const;

    /**
     * Returns the original curve's width.
     */
    virtual float getWidth() const;

    /**
     * Returns the original curve.
     */
    virtual CurvePtr getCurve() const;

    /**
     * Returns the end index of this curve part inside the original curve.
     */
    virtual int getEnd() const = 0;

    /**
     * Returns the coordinates of a point at a given index.
     *
     * @param i the rank of the vertex to return.
     * @return the XY coords of the i'th vertex.
     */
    virtual vec2d getXY(int i) const = 0;

    /**
     * Returns the coordinates of a point at a given index starting from a given Point.
     *
     * @param start the starting position (start or end point).
     * @param offset the rank of the vertex to return.
     * @return the XY coords of the i'th vertex.
     */
    vec2d getXY(const vec2d &start, int offset) const;

    /**
     * Checks if a point is a control Point.
     *
     * @param i the rank of the vertex to check.
     * @return true if the i'th vertex is a control point.
     */
    virtual bool getIsControl(int i) const;

    /**
     * Returns the i'th point S coordinate.
     *
     * @param i the rank of the vertex to get the coordinates from.
     * @return the pseudo curvilinear coordinate of the i'th point.
     */
    virtual float getS(int i) const = 0;

    /**
     * Returns the curve part's bounding box
     */
    virtual box2d getBounds() const = 0;

    /**
     * Checks if a point is clipable.
     *
     * @param i the rank of the point to check.
     * @return true if the i'th point is clipable.
     */
    virtual bool canClip(int i) const;

    /**
     * Returns a sub-curvePart clipped from this one.
     *
     * @param start the starting index of the new curve inside the current one.
     * @param end the ending index of the new curve inside the current one.
     * @return a new CurvePart, containing the nodes in range [start - end] from this.
     */
    virtual CurvePart* clip(int start, int end) const = 0;

    /**
     * Clips this curve part with the given rectangle. This results in a set of
     * curve parts that are added to the given list. Note that the curve width
     * is NOT taken into account.
     *
     * @param clip the clip region.
     * @param result where the resulting curve parts must be stored.
     */
    void clip(const box2d &clip, vector<CurvePart*> &result) const;

    /**
     * Clips a set of curve parts.
     *
     * @param paths the curve parts that must be clipped.
     * @param clip the clip region.
     * @param result where the resulting curve parts must be stored.
     */
    static void clip(const vector<CurvePart*> &paths, const box2d &clip, vector<CurvePart*> &result);

    /**
     * Returns true if this equals a given Curve.
     *
     * @param c the curve to compare with.
     */
    bool equals(CurvePtr c) const;
};

}

#endif
