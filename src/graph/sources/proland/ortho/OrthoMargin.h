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

#ifndef _PROLAND_ORTHOMARGIN_H_
#define _PROLAND_ORTHOMARGIN_H_

#include "proland/graph/Margin.h"

namespace proland
{

/**
 * A Margin to clip a Graph for an OrthoGPUProducer layer. This margin
 * enlarges the clip region so that it also includes the ortho tile borders.
 * @ingroup ortho
 * @author Antoine Begault
 */
PROLAND_API class OrthoMargin : public Margin
{
public:
    /**
     * Creates an uninitialized OrthoMargin.
     */
    OrthoMargin();

    /**
     * Creates a new OrthoMargin.
     *
     * @param samplesPerTile number of pixels per ortho tile (without borders).
     * @param borderFactor size of the tile borders in percentage of tile size.
     * @param widthFactor width of drawn curves in percentage of curve width.
     */
    OrthoMargin(int samplesPerTile, float borderFactor, float widthFactor);

    /**
     * Deletes this OrthoMargin.
     */
    virtual ~OrthoMargin();

    virtual double getMargin(double clipSize);

    virtual double getMargin(double clipSize, CurvePtr p);

protected:
    /**
     * Number of pixels per elevation tile, without borders.
     */
    int samplesPerTile;

    /**
     * Size of the tile borders in percentage of tile size.
     */
    float borderFactor;

    /**
     * Width of drawn curves in percentage of curve width.
     */
    float widthFactor;
};

}

#endif
