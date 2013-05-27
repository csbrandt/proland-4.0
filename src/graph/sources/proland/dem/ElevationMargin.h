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

#ifndef _PROLAND_ELEVATIONMARGIN_H_
#define _PROLAND_ELEVATIONMARGIN_H_

#include "proland/graph/Margin.h"

namespace proland
{

/**
 * A Margin to clip a Graph for an ElevationGraphLayer. This margin
 * enlarges the clip region so that it also includes the elevation
 * tile borders.
 * @ingroup dem
 * @authors Antoine Begault, Eric Bruneton
 */
PROLAND_API class ElevationMargin : public Margin
{
public:
    /**
     * Creates an uninitialized ElevationMargin.
     */
    ElevationMargin();

    /**
     * Creates a new ElevationMargin.
     *
     * @param samplesPerTile number of pixels per elevation tile (without borders).
     * @param borderFactor size of the tile borders in percentage of tile size.
     */
    ElevationMargin(int samplesPerTile, float borderFactor);

    /**
     * Deletes this ElevationMargin.
     */
    virtual ~ElevationMargin();

    virtual double getMargin(double clipSize);

protected:
    /**
     * Number of pixels per elevation tile, without borders.
     */
    int samplesPerTile;

    /**
     * Size of the tile borders in percentage of tile size.
     */
    float borderFactor;
};

}

#endif
