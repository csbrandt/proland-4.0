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

#ifndef _PROLAND_TWEAKBARMANAGER_H_
#define _PROLAND_TWEAKBARMANAGER_H_

#include <vector>

#include "AntTweakBar.h"

#include "ork/scenegraph/AbstractTask.h"
#include "proland/ui/twbar/TweakBarHandler.h"

using namespace std;

using namespace ork;

namespace proland
{

/**
 * Provides a modular tweak bar made of several TweakBarHandler.
 * Each TweakBarHandler provides controls to control and edit some
 * aspects of a scene. Each TweakBarHandler can be activated or
 * deactivated. The TweakBarHandler can be permanently active. If not,
 * they can be mutually exclusive with other TweakBarHandler. In this
 * case the TweakBarManager ensures that at most one such TweakBarHandler
 * is active at the same time.
 * A TweakBarManager is an EventHandler. The events are first sent to
 * the tweak bar. Unhandled events are then forwarded to all active
 * TweakBarHandler and then, if still unhandled, to an external EventHandler.
 * @ingroup twbar
 * @authors Eric Bruneton, Antoine Begault
 */
PROLAND_API class TweakBarManager : public EventHandler
{
public:
    /**
     * A TweakBarHandler with additional options.
     */
    struct BarData
    {
    public:
        /**
         * The TweakBarManager to which this BarData belongs.
         */
        TweakBarManager *owner;

        /**
         * A TweakBarHandler.
         */
        ptr<TweakBarHandler> bar;

        /**
         * True if this TweakBarHandler must not be active at the same time
         * as the other exclusive TweakBarHandler.
         */
        bool exclusive;

        /**
         * True if this TweakBarHandler must always be active.
         */
        bool permanent;

        /**
         * Shortcut key to activate or deactive this TweakBarHandler, or 0.
         */
        char k;

        /**
         * Creates a new BarData.
         *
         * @param owner the TweakBarManager to which this BarData belongs.
         * @param bar a TweakBarHandler.
         * @param exclusive true if this TweakBarHandler must not be active
         *      at the same time as the other exclusive TweakBarHandler.
         * @param permanent true if this TweakBarHandler must always be active.
         * @param key shortcut key to activate or deactive this
         *      TweakBarHandler, or 0.
         */
        BarData(TweakBarManager *owner, ptr<TweakBarHandler> bar, bool exclusive, bool permanent, char k);

        /**
         * Activates or deactivates this TweakBarHandler.
         *
         * @param active true to activate this TweakBarHandler, false otherwise.
         */
        void setActive(bool active);
    };

    /**
     * Creates a new TweakBarManager.
     *
     * @param bars the TweakBarHandler managed by this manager.
     * @param minimized true if the tweak bar must be initially minimized.
     */
    TweakBarManager(vector<BarData> bars, bool minimized = true);

    /**
     * Deletes this TweakBarManager.
     */
    virtual ~TweakBarManager();

    /**
     * Returns the EventHandler to which the events not handled by this
     * TweakBarManager must be forwarded.
     */
    ptr<EventHandler> getNext();

    /**
     * Sets the EventHandler to which the events not handled by this
     * TweakBarManager must be forwarded.
     *
     * @param an EventHandler.
     */
    void setNext(ptr<EventHandler> next);

    virtual void redisplay(double t, double dt);

    virtual void reshape(int x, int y);

    virtual void idle(bool damaged);

    virtual bool mouseClick(button b, state s, modifier m, int x, int y);

    virtual bool mouseWheel(wheel b, modifier m, int x, int y);

    virtual bool mouseMotion(int x, int y);

    virtual bool mousePassiveMotion(int x, int y);

    virtual bool keyTyped(unsigned char c, modifier m, int x, int y);

    virtual bool keyReleased(unsigned char c, modifier m, int x, int y);

    virtual bool specialKey(key  k, modifier m, int x, int y);

    virtual bool specialKeyReleased(key  k, modifier m, int x, int y);

    /**
     * Deactivates all the exclusive TweakBarHandler.
     */
    void resetStates();

protected:
    /**
     * Creates an uninitialized TweakBarManager.
     */
    TweakBarManager();

    /**
     * Initializes this TweakBarManager.
     * See #TweakBarManager.
     */
    void init(vector<BarData> bars, bool minimized = true);

    void swap(ptr<TweakBarManager> t);

private:
    /**
     * The tweak bar managed by this TweakBarManager.
     */
    TwBar *selectBar;

    /**
     * The TweakBarHandler managed by this TweakBarManager.
     */
    vector<BarData> bars;

    /**
     * The EventHandler to which the events not handled by this
     * TweakBarManager must be forwarded.
     */
    ptr<EventHandler> next;

    /**
     * True if the tweak bar must be initially minimized.
     */
    int minimized;

    /**
     * True if the tweak bar has been initialized.
     */
    bool initialized;

    /**
     * True if the tweak bar must be updated.
     */
    bool updated;

    /**
     * Initializes the tweak bar managed by this manager. This method
     * clears the bar and calls TweakBarHandler#updateBar on each
     * TweakBarHandler so that it can add its own controls to the bar.
     */
    void initBar();
};

}

#endif
