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

#ifndef _PROLAND_CURVEDATA_H_
#define _PROLAND_CURVEDATA_H_

#include "proland/graph/Area.h"
#include "proland/producer/TileProducer.h"

namespace proland
{

class GraphLayer;

/**
 * Contains data about a Curve. This data will be used when drawing any
 * child (and sub-levels) of this curve.
 * Contains profile and general aspect of the curve.
 * Used to compute stripes, elevations, etc...
 * @ingroup producer
 * @author Antoine Begault, Guillaume Piolat
 */
PROLAND_API class CurveData
{
public:
    /**
     * Determines the side of the boundary we are currently treating.
     */
    enum curveBoundary
    {
        RIGHT_BOUNDARY = 0,
        LEFT_BOUNDARY = 1
    };

    /**
     * Creates a new CurveData.
     *
     * @param id the id of the curve for which we need to store the data.
     * @param flattenCurve the curve associated with this CurveData.
     */
    CurveData(CurveId id, CurvePtr flattenCurve);

    /**
     * Deletes this CurveData.
     */
    virtual ~CurveData();

    /**
     * Returns the curvilinear length of the curve (at point size - 1).
     */
    float getCurvilinearLength();

    /**
     * Computes the curvilinear length corresponding to the given s
     * coordinate.
     *
     * @param s a pseudo curvilinear coordinate (see Curve::Vertex#s).
     * @param p where to store the x,y coordinates corresponding to s, or NULL
     * if these coordinates are not needed.
     * @param n where to store the normal to the curve at s, or NULL if this
     * normal is not needed.
     */
    float getCurvilinearLength(float s, vec2d *p = NULL, vec2d *n = NULL);

    /**
     * Computes the pseudo curvilinear coordinate corresponding to the given l
     * coordinate.
     *
     * @param l a curvilinear coordinate (see Curve::Vertex#l).
     * @param p where to store the x,y coordinates corresponding to l, or NULL
     * if these coordinates are not needed.
     * @param n where to store the normal to the curve at l, or NULL if this
     * normal is not needed.
     */
    float getCurvilinearCoordinate(float l, vec2d *p = NULL, vec2d *n = NULL);

    /**
     * Returns the cap length from the begining of the curve.
     *
     */
    float getStartCapLength();

    /**
     * Returns the cap length from the end of the curve.
     *
     */
    float getEndCapLength();

    /**
     * Returns the id of curve corresponding to this CurveData.
     */
    CurveId getId();

    /**
     * Returns the list of used tiles.
     *
     * @param[out] tiles the list of usedTiles.
     * @param rootSampleLength Curve sample length at level 0.
     */
    virtual void getUsedTiles(set<TileCache::Tile::Id> &tiles, float rootSampleLength);

    /**
     * Returns this Curve's flattened Size.
     */
    int getSize();

    /**
     * Returns the pseudo curvilinear coordinate of a selected vertex in the flattened Curve.
     *
     * @param i rank of the vertex to return.
     * @return the pseudo curvilinear coordinate of the selected vertex.
     */
    float getS(int rank);

    CurvePtr getFlattenCurve();

protected:
    /**
     * The CurveId of the described Curve.
     */
    CurveId id;

    /**
     * Flattened version of the described Curve.
     */
    CurvePtr flattenCurve;

    /**
     * Used Tiles. i.e. tiles on which this curve is defined, at its maximum level of detail.
     */
     set<TileCache::Tile::Id> usedTiles;

    /**
     * Curve length.
     */
    float length;

    /**
     * Cap length at extremities of the curve.
     */
    float startCapLength, endCapLength;

    /**
     * Computes the cap length at a given extremity.
     *
     * @param p the Node from which to compute the cap length.
     * @param q a point determining the direction of the cap length.
     */
    virtual float getCapLength(NodePtr p, vec2d q);
};

}

#endif /*CURVEDATA_H_*/
