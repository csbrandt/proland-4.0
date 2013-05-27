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

#ifndef _PROLAND_COMPOSEDMARGIN_H_
#define _PROLAND_COMPOSEDMARGIN_H_

#include "proland/graph/Margin.h"

namespace proland
{

/**
 * A Margin is used to determine what to clip in a graph.
 * It enables to take into account the width of a curve when clipping a box.
 * For example, when drawing a road that would go along a border, the curve would be in only one tile,
 * but considering the curve's width, the drawing would be cut on the tile border. That is why we need margins.
 * This class enables to check multiple Margins at the same time.
 * @ingroup graph
 * @author Antoine Begault
 */
PROLAND_API class ComposedMargin : public Margin
{
public:
    /**
     * Creates a new ComposedMargin.
     */
    ComposedMargin();

    /**
     * Deletes this ComposedMargin.
     */
    virtual ~ComposedMargin();

    /**
     * Returns the maximum margin from contained margins (max borderSize).
     *
     * @param clipSize size of the clipping box.
     */
    virtual double getMargin(double clipSize);

    /**
     * Returns the maximum margin from contained margins.
     *
     * @param clipSize size of the clipping box.
     * @param p the curve to get the margin from.
     */
    virtual double getMargin(double clipSize, CurvePtr p);

    /**
     * Returns the maximum margin from contained margins.
     *
     * @param clipSize size of the clipping box.
     * @param a the area to get the margin from.
     */
    virtual double getMargin(double clipSize, AreaPtr a);

    /**
     * Adds a margin.
     *
     * @param m a margin.
     */
    void addMargin(Margin *m);

    /**
     * Removes a margin.
     *
     * param m a margin.
     */
    void removeMargin(Margin *m);

private:
    /**
     * list of margins.
     */
    vector<Margin*> margins;
};

}

#endif
