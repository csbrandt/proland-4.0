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

#ifndef _PROLAND_TWEAKVIEWHANDLER_H_
#define _PROLAND_TWEAKVIEWHANDLER_H_

#include "proland/ui/BasicViewHandler.h"
#include "proland/ui/twbar/TweakBarHandler.h"

namespace proland
{

/**
 * A TweakBarHandler to control a BasicViewHandler. This class provides
 * tweak bar buttons corresponding to predefined positions, and allows the
 * users to go instantly or smoothly to one of these predefined positions.
 * Another button prints the current position.
 * @ingroup twbar
 * @authors Eric Bruneton, Antoine Begault
 */
PROLAND_API class TweakViewHandler : public TweakBarHandler
{
public:
    /**
     * A BasicViewHandler::Position together with a name and a shortcut key.
     */
    class Position : public BasicViewHandler::Position {
    public:
        /**
         * The TweakViewHandler to which this predefined position belongs.
         */
        TweakViewHandler *owner;

        /**
         * The name of this predefined position.
         */
        const char *name;

        /**
         * The shortcut key for this predefined position, or 0.
         */
        char key;

        /**
         * Goes to this position instantly or smoothly, depending on
         * TweakViewHandler#animate.
         */
        void go();
    };

    /**
     * Creates a new TweakViewHandler.
     *
     * @param viewHandler the BasicViewHandler to be controlled by this
     *      TweakViewHandler.
     * @param views a list of predefined positions with names and
     *      shortcut keys.
     * @param animate true to go smoothly to target positions, false to
     *      move instantly to them.
     * @param active true if this TweakBarHandler must be initialy active.
     */
    TweakViewHandler(ptr<BasicViewHandler> viewHandler, const std::vector<Position> &views, bool animate, bool active);

    /**
     * Deletes this TweakViewHandler.
     */
    virtual ~TweakViewHandler();

protected:
    /**
     * Creates an uninitialized TweakViewHandler.
     */
    TweakViewHandler();

    /**
     * Initializes this TweakViewHandler.
     * See #TweakViewHandler.
     */
    virtual void init(ptr<BasicViewHandler> viewHandler, const std::vector<Position> &views, bool animate, bool active);

    virtual void updateBar(TwBar *bar);

    void swap(ptr<TweakViewHandler> o);

private:
    /**
     * The BasicViewHandler to be controlled by this TweakViewHandler.
     */
    ptr<BasicViewHandler> viewHandler;

    /**
     * A list of predefined positions with names and shortcut keys.
     */
    std::vector<Position> views;

    /**
     * True to go smoothly to target positions, false to move instantly to
     * them.
     */
    bool animate;

    friend class Position;
};

}

#endif
