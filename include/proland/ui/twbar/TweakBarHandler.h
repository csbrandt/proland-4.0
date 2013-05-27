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

#ifndef _PROLAND_TWEAKBARHANDLER_H_
#define _PROLAND_TWEAKBARHANDLER_H_

#include "AntTweakBar.h"

#include "ork/core/Object.h"
#include "ork/ui/EventHandler.h"

using namespace ork;

namespace proland
{

/**
 * Provides a group of tweak bar controls as well as event handler methods to
 * control some aspects of a scene. A TweakBarHandler can add controls to an
 * existing tweak bar. It also provides EventHandler like methods to control
 * the scene directly with the mouse and keyboard (by default these methods
 * delegate their work to an external EventHandler).
 * A TweakBarHandler is intended to be used with a TweakBarManager. The manager
 * can activate or deactivate a TweakBarHandler, and is responsible to call the
 * EventHandler like methods of the TweakBarHandler that are active. When
 * inactive, these methods are not called, and the tweak bar controls provided
 * by a TweakBarHandler do not appear in the tweak bar.
 * @ingroup twbar
 * @authors Eric Bruneton, Antoine Begault
 */
PROLAND_API class TweakBarHandler : public Object
{
public:
    /**
     * Creates a new TweakBarHandler.
     *
     * @param name the name of this TweakBarHandler.
     * @param eventHandler the EventHandler to which the EventHandler like
     *      methods of this TweakBarHandler delegate their work. Maybe NULL.
     * @param active true if this TweakBarHandler must be initialy active.
     */
    TweakBarHandler(const char *name, ptr<EventHandler> eventHandler, bool active);

    /**
     * Deletes this TweakBarHandler.
     */
    virtual ~TweakBarHandler();

    /**
     * Returns the name of this TweakBarHandler.
     */
    const char *getName();

    /**
     * Returns true if this TweakBarHandler is active.
     */
    bool isActive();

    /**
     * Activates or deactivates this TweakBarHandler.
     *
     * @param active true to activate this TweakBarHandler, false otherwise.
     */
    virtual void setActive(bool active);

    /**
     * This method is called when a window must be redrawn.
     * The default implementation of this method delegates to #eventHandler
     * if it is not NULL, and sets needUpdate to #needUpdate (which is then
     * reset to false).
     *
     * @param[out] needUpdate true if the tweakbar must be updated.
     */
    virtual void redisplay(double t, double dt, bool &needUpdate);

    /**
     * This method is called when a window is resized.
     * The default implementation of this method delegates to #eventHandler
     * if it is not NULL, and sets needUpdate to false.
     *
     * @param x the new window width.
     * @param y the new window height.
     * @param[out] needUpdate true if the tweakbar must be updated.
     */
    virtual void reshape(int x, int y, bool &needUpdate);

    /**
     * This method is called periodically when no event occur.
     * The default implementation of this method delegates to #eventHandler
     * if it is not NULL, and sets needUpdate to false.
     *
     * @param damaged true if the window (partial) visibility has changed since
     *      the last call to this method. This occurs if the window is minized,
     *      maximized or if it is partially covered by another window.
     * @param[out] needUpdate true if the tweakbar must be updated.
     */
    virtual void idle(bool damaged, bool &needUpdate);

    /**
     * This method is called when a mouse click occurs.
     * The default implementation of this method delegates to #eventHandler
     * if it is not NULL, and sets needUpdate to false.
     *
     * @param b the button that was clicked.
     * @param s the %state of this button.
     * @param m the modifiers that were pressed when the click occured.
     * @param x the mouse x coordinate in pixels when the click occured.
     * @param y the mouse y coordinate in pixels when the click occured.
     * @param[out] needUpdate true if the tweakbar must be updated.
     * @return true if the event has been handled by this method.
     */
    virtual bool mouseClick(EventHandler::button b, EventHandler::state s, EventHandler::modifier m, int x, int y, bool &needUpdate);

    /**
     * This method is called when a mouse wheel event occurs.
     * The default implementation of this method delegates to #eventHandler
     * if it is not NULL, and sets needUpdate to false.
     *
     * @param b the wheel event that occured (WHEEL_UP or WHEEL_DOWN)
     * @param m the modifiers that were pressed when the wheel event occured.
     * @param x the mouse x coordinate in pixels when the wheel event occured.
     * @param y the mouse y coordinate in pixels when the wheel event occured.
     * @param[out] needUpdate true if the tweakbar must be updated.
     * @return true if the event has been handled by this method.
     */
    virtual bool mouseWheel(EventHandler::wheel b, EventHandler::modifier m, int x, int y, bool &needUpdate);

    /**
     * This method is called periodically when the mouse is moved.
     * The default implementation of this method delegates to #eventHandler
     * if it is not NULL, and sets needUpdate to false.
     *
     * @param x the mouse x coordinate in pixels.
     * @param y the mouse y coordinate in pixels.
     * @param[out] needUpdate true if the tweakbar must be updated.
     * @return true if the event has been handled by this method.
     */
    virtual bool mouseMotion(int x, int y, bool &needUpdate);

    /**
     * This method is called periodically when the mouse is moved.
     * The default implementation of this method delegates to #eventHandler
     * if it is not NULL, and sets needUpdate to false.
     *
     * @param x the mouse x coordinate in pixels.
     * @param y the mouse y coordinate in pixels.
     * @param[out] needUpdate true if the tweakbar must be updated.
     * @return true if the event has been handled by this method.
     */
    virtual bool mousePassiveMotion(int x, int y, bool &needUpdate);

    /**
     * This method is called when a regular key is typed.
     * The default implementation of this method delegates to #eventHandler
     * if it is not NULL, and sets needUpdate to false.
     *
     * @param c the typed character.
     * @param m the modifiers that were pressed when the key was typed.
     * @param x the mouse x coordinate in pixels when the key was typed.
     * @param y the mouse y coordinate in pixels when the key was typed.
     * @param[out] needUpdate true if the tweakbar must be updated.
     * @return true if the event has been handled by this method.
     */
    virtual bool keyTyped(unsigned char c, EventHandler::modifier m, int x, int y, bool &needUpdate);

    /**
     * This method is called when a regular key is released.
     * The default implementation of this method delegates to #eventHandler
     * if it is not NULL, and sets needUpdate to false.
     *
     * @param c the released character.
     * @param m the modifiers that were pressed when the key was released.
     * @param x the mouse x coordinate in pixels when the key was released.
     * @param y the mouse y coordinate in pixels when the key was released.
     * @param[out] needUpdate true if the tweakbar must be updated.
     * @return true if the event has been handled by this method.
     */
    virtual bool keyReleased(unsigned char c, EventHandler::modifier m, int x, int y, bool &needUpdate);

    /**
     * This method is called when a special key is typed.
     * The default implementation of this method delegates to #eventHandler
     * if it is not NULL, and sets needUpdate to false.
     *
     * @param k the typed key.
     * @param m the modifiers that were pressed when the key was typed.
     * @param x the mouse x coordinate in pixels when the key was typed.
     * @param y the mouse y coordinate in pixels when the key was typed.
     * @param[out] needUpdate true if the tweakbar must be updated.
     * @return true if the event has been handled by this method.
     */
    virtual bool specialKey(EventHandler::key  k, EventHandler::modifier m, int x, int y, bool &needUpdate);

    /**
     * This method is called when a special key is released.
     * The default implementation of this method delegates to #eventHandler
     * if it is not NULL, and sets needUpdate to false.
     *
     * @param k the released key.
     * @param m the modifiers that were pressed when the key was released.
     * @param x the mouse x coordinate in pixels when the key was released.
     * @param y the mouse y coordinate in pixels when the key was released.
     * @param[out] needUpdate true if the tweakbar must be updated.
     * @return true if the event has been handled by this method.
     */
    virtual bool specialKeyReleased(EventHandler::key  k, EventHandler::modifier m, int x, int y, bool &needUpdate);

    /**
     * Adds controls to the given tweak bar.
     */
    virtual void updateBar(TwBar *bar) = 0;

protected:
    /**
     * The EventHandler to which the EventHandler like methods of this
     * TweakBarHandler delegate their work. Maybe NULL.
     */
    ptr<EventHandler> eventHandler;

    /**
     * Creates an uninitialized TweakBarHandler.
     */
    TweakBarHandler();

    /**
     * Initializes this TweakBarHandler.
     * See #TweakBarHandler.
     */
    virtual void init(const char *name, ptr<EventHandler> eventHandler, bool active);

    void swap(ptr<TweakBarHandler> t);

private:
    /**
     * The name of this TweakBarHandler.
     */
    const char *name;

    /**
     * True if this TweakBarHandler is active.
     */
    bool active;

    /**
     * True if the tweakbar must be updated.
     */
    bool needUpdate;
};

}

#endif
