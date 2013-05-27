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

#ifndef _PROLAND_HYDROCURVE_H_
#define _PROLAND_HYDROCURVE_H_

#include "proland/graph/Curve.h"

namespace proland
{

/**
 * A Curve with additional, river specific data.
 * An HydroCurve can be of 2 different types:
 * - A River, which is the same as a Curve
 * - A Bank, which contains links to Rivers and potential values.
 * @ingroup rivergraph
 * @author Antoine Begault
 */
PROLAND_API class HydroCurve : public Curve
{
public:
    /**
     * HydroCurve Type.
     */
    enum HYDRO_CURVE_TYPE
    {
        AXIS,
        CLOSING_SEGMENT,
        BANK
    };

    /**
     * Creates a new HydroCurve.
     *
     * @param owner the graph containing this curve.
     */
    HydroCurve(Graph *owner);

    /**
     * Creates a new HydroCurve, with parameters copied from another Curve.
     *
     * @param owner the graph containing this curve.
     * @param c the copied curve, from which this curve takes its data.
     * @param s the start node.
     * @param e the end node.
     */
    HydroCurve(Graph *owner, CurvePtr c, NodePtr s, NodePtr e);

    /**
     * Deletes this HydroCurve.
     */
    virtual ~HydroCurve();

    /**
     * Returns the width of this curve.
     */
    float getWidth() const;

    /**
     * Returns this Curve's potential.
     * Returns -1 if this Curve is a river axis.
     */
    float getPotential() const;

    /**
     * Sets this curve's potential.
     */
    void setPotential(float potential);

    /**
     * Returns the id of the river axis associated to this HydroCurve.
     * NULL_ID if this Curve is a river axis.
     */
    CurveId getRiver() const;

    /**
     * Returns the river axis associated to this HydroCurve.
     * NULL if this Curve is a river.
     */
    CurvePtr getRiverPtr() const;

    /**
     * Sets the river axis associated to this HydroCurve.
     *
     * @param river the Curve's Id.
     */
    void setRiver(CurveId river);

    /**
     * Display method. For debug only.
     */
    virtual void print() const;

protected:
    /**
     * River axis associated to this HydroCurve.
     */
    CurveId river;

    /**
     * Value used to determine the flow between two banks.
     */
    float potential;

    friend class HydroGraph;
};

}

#endif
