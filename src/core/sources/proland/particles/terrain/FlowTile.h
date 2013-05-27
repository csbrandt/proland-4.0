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

#ifndef _PROLAND_FLOWTILE_H
#define _PROLAND_FLOWTILE_H

#include "ork/core/Object.h"
#include "ork/math/vec2.h"

using namespace ork;

namespace proland
{

/**
 * Computes the velocity of a flow at a given point. Can be used for any
 * moving elements. In this class, the word Tile will refer to a 2D region
 * on a terrain. It may correspond to a TileProducer Tiles, but can also
 * be completely independent from that. You just need to define the
 * considered region on that terrain.
 * @ingroup terrainl
 * @author Antoine Begault
 */
PROLAND_API class FlowTile : public Object
{
public:
    /**
     * Determines the type of data at a point.
     */
    enum DATATYPE
    {
        UNKNOWN = 0,
        INSIDE = 1,
        LEAVING = 2,
        NEAR = 3,
        OUTSIDE = 4,
        LEAVING_DOMAIN = 5,
        OUTSIDE_DOMAIN = 6,
        ON_SKY = 7
    };

    /**
     * Creates a new FlowTile.
     *
     * @param ox the x coordinate of the lower left corner of this Tile.
     * @param oy the y coordinate of the lower left corner of this Tile.
     * @param size width of this Tile.
     */
    FlowTile(float ox, float oy, float size);

    /**
     * Deletes this FlowTile.
     */
    virtual ~FlowTile();

    /**
     * Returns a defined type for a given position in the tile.
     *
     * @param pos a position inside the tile's viewport.
     */
    virtual int getType(vec2d &pos);

    /**
     * Returns the velocity at a given point, depending on the data contained in this FlowTile.
     *
     * @param pos a XY position inside the viewport of this FlowTile.
     * @param[out] velocity a vec2f containing the 2D velocity at given coordinates.
     * @param[out] type type of data at given coordinates. See #dataType
     */
    virtual void getVelocity(vec2d &pos, vec2d &velocity, int &type) = 0;

    /**
     * Returns the data type at a given point. Simplified version of #getVelocity().
     * @param pos a XY position inside the viewport of this FlowTile.
     * @param[out] type type of data at given coordinates. See #dataType.
     */
    virtual void getDataType(vec2d &pos, int &type);

    /**
     * Returns true if the given point is inside this FlowTile's viewport.
     *
     * @param x x coordinate.
     * @param y y coordinate.
     */
    bool contains(float x, float y);

protected:
    /**
     * The x coordinate of the lower left corner of this Tile.
     */
    float ox;

    /**
     * The y coordinate of the lower left corner of this Tile.
     */
    float oy;

    /**
     * Width of this Tile.
     */
    float size;
};

}

#endif
