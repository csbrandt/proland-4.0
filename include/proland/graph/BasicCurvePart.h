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

#ifndef _PROLAND_BASICCURVEPART_H_
#define _PROLAND_BASICCURVEPART_H_

#include "proland/graph/CurvePart.h"

namespace proland
{

/**
 * A part of a curve. This part is defined by a curve, and by two indexes that
 * give the start and end points of this curve part inside the whole curve.
 * @ingroup graph
 * @author Antoine Begault
 */
PROLAND_API class BasicCurvePart : public CurvePart
{
public:
    /**
     * Creates a new CurvePart.
     *
     * @param p the whole curve.
     * @param start start of the interval in p to which this part corresponds.
     * @param end end of the interval in p to which this part corresponds.
     */
    BasicCurvePart(CurvePtr p, int start, int end);

    /**
     * Creates a new CurvePart.
     *
     * @param p the whole curve.
     * @param orientation the curve part orientation (see #orientation).
     * @param start start of the interval in p to which this part corresponds.
     * @param end end of the interval in p to which this part corresponds.
     */
    BasicCurvePart(CurvePtr p, int orientation, int start, int end);

    /**
     * Returns the original curve Id.
     */
    virtual CurveId getId() const;

    /**
     * Returns the original curve parentId.
     */
    virtual CurveId getParentId() const;

    /**
     * Returns the original curve.
     */
    CurvePtr getCurve() const;

    /**
     * Returns the corresponding index of i in the original curve.
     *
     * @param i rank of the point
     */
    int getCurveIndex(int i) const;

    /**
     * Returns the original curve type.
     */
    virtual int getType() const;

    /**
     * Returns the original curve width.
     */
    virtual float getWidth() const;

    /**
     * Returns the length if this curvepart.
     */
    virtual int getEnd() const;

    /**
     * Returns the XY coords of the i'th point from this curve.
     *
     * @param i rank of the point to get the coords from.
     */
    virtual vec2d getXY(int i) const;

    /**
     * Returns the value isControl of the i'th point from this curve.
     *
     * @param i rank of the point to get the boolean from.
     */
    virtual bool getIsControl(int i) const;

    /**
     * Returns the S coords of the i'th point from this curve.
     *
     * @param i rank of the point to get the coord from.
     */
    virtual float getS(int i) const;

    /**
     * Returns the bounds of this curvePart.
     */
    virtual box2d getBounds() const;

    /**
     * Returns false if the i'th point is a controlPoint, else true.
     *
     * @param i rank of the point.
     */
    virtual bool canClip(int i) const;

    /**
     * Clips this curvePart.
     *
     * @param start starting point.
     * @param end end point.
     * @return a new CurvePart, result of the clipping of this CurvePart.
     */
    virtual CurvePart *clip(int start, int end) const;

protected:
    /**
     * The original curve.
     */
    CurvePtr curve;

    /**
     * The orientation of this curve part. 0 means that the curve's start
     * and end are given by #start and #end respectively. 1 means that the
     * curve's start and end are given by #end and #start respectively (note
     * that #end is always greater than #start).
     */
    int orientation;

    /**
     * Start of the interval, inside #path, to which this curve part
     * corresponds (inclusive).
     */
    int start;

    /**
     * End of the interval, inside #path, to which this curve part
     * corresponds (inclusive).
     */
    int end;
};

}

#endif
