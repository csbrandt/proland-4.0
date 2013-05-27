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

#ifndef _PROLAND_MARGIN_H_
#define _PROLAND_MARGIN_H_

#include "proland/graph/Graph.h"

namespace proland
{

/**
 * Determines how a clip region must be extended to clip a Graph, depending
 * on its content. In order to take curve widths into account during
 * clipping, the clip area is enlarged with a margin to avoid clipping curves
 * whose axis is outside the clip region, but not their borders
 * (see \ref sec-graph).
 * @ingroup graph
 * @author Antoine Begault
 */
PROLAND_API class Margin
{
public:
    /**
     * Returns the margin for the given clip region. This margin will
     * be added to the clip region in order to clip a graph.
     *
     * @param clipSize size of the clip region (width or height).
     */
    virtual double getMargin(double clipSize) = 0;

    /**
     * Returns the margin for the given curve. This margin will
     * be added to the clip region in order to clip this curve.
     *
     * @param clipSize size of the clip region (width or height).
     * @param p a curve.
     */
    virtual double getMargin(double clipSize, CurvePtr p) = 0;

    /**
     * Returns the margin for the given area. This margin will
     * be added to the clip region in order to clip this area.
     *
     * @param clipSize size of the clip region (width or height).
     * @param a an area.
     */
    virtual double getMargin(double clipSize, AreaPtr a);
};

}

#endif
