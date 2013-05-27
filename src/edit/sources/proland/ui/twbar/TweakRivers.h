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

#ifndef _PROLAND_TWEAKRIVERS_H_
#define _PROLAND_TWEAKRIVERS_H_

#include "proland/ui/twbar/TweakBarHandler.h"
#include "proland/rivers/DrawRiversTask.h"

namespace proland
{

/**
 * A TweakBarHandler to control %rivers rendering and animation.
 * @ingroup twbar
 * @author Antoine Begault
 */
PROLAND_API class TweakRivers : public TweakBarHandler
{
public:
    /**
     * Creates a new TweakRivers.
     *
     * @param drawer the task that draws and animate rivers.
     * @param active true if this TweakBarHandler must be initialy active.
     */
    TweakRivers(ptr<DrawRiversTask> drawer, bool active);

    /**
     * Deletes this TweakRivers.
     */
    virtual ~TweakRivers();

    virtual void redisplay(double t, double dt, bool &needUpdate);

    virtual void updateBar(TwBar *bar);

protected:
    /**
     * Creates an uninitialized TweakRivers.
     */
    TweakRivers();

    /**
     * Initializes this TweakRivers.
     * See #TweakRivers.
     */
    virtual void init(ptr<DrawRiversTask> drawer, bool active);

private:
    /**
     * The task that draws and animate rivers.
     */
    ptr<DrawRiversTask> drawer;

    /**
     * The bar that currently contains the TweakBar data for this TweakRivers.
     * Stored to retrieve #barStates at each changes.
     */
    TwBar * currentBar;

    /**
     * Stores the opened state of each groups.
     */
    int *barStates;
};

}

#endif
