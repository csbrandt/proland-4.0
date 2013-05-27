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

#ifndef _PROLAND_TWEAKGRAPHLAYER_H_
#define _PROLAND_TWEAKGRAPHLAYER_H_

#include "proland/ui/twbar/TweakBarHandler.h"
#include "proland/edit/EditGraphOrthoLayer.h"

namespace proland
{

/**
 * A TweakBarHandler providing %graph edition tools.
 * @ingroup twbar
 * @author Antoine Begault, Guillaume Piolat
 */
PROLAND_API class TweakGraphLayer : public TweakBarHandler
{
public:
    enum CONTEXT_MENU_MODE
    {
        HIDDEN = 0,
        CLOSE = 1,
        CLICK = 2,
        DISPLAY_MENU = 3
    };

    TweakGraphLayer(bool active);

    virtual ~TweakGraphLayer();

    virtual void setActive(bool active);

    virtual void redisplay(double t, double dt, bool &needUpdate);

    virtual bool mouseClick(EventHandler::button b, EventHandler::state s, EventHandler::modifier m, int x, int y, bool &needUpdate);

    /**
     * Context Menu. Appears when right clicking on a curve.
     * CTRL + right click will open the menu without changing the current selection.
     * Useful when you don't see the curve you are editing or when there are lots of nearly curves.
     */
    static TwBar *contextBar;

protected:
    TweakGraphLayer();

    virtual void init(bool active);

    virtual void updateBar(TwBar *bar);

    /**
     * Creates a new Edition Tweakbar.
     */
    virtual void createTweakBar();

    /**
     * Displays currently selected Curve Info in TwBar b (including width, type... and points info).
     *
     * @param b the TwBar in which to display the Curve Infos.
     * @param curveData the TwEditBarData containing informations about the current selection.
     */
    virtual void displayCurveInfo(TwBar *b, EditGraphOrthoLayer::SelectionData* curveData);

    /**
     * Hides the context menu.
     */
    void closeMenu();

    /**
     * Opens a context menu, whose content will depend of the current selection.
     */
    void displayMenu(int mousePosX, int mousePosY);

    /**
     * Contains data on the current selection. see TWEditBarData class.
     */
    EditGraphOrthoLayer::SelectionData selectedCurveData;

    /**
     * True if the context menu is opened.
     */
    int displayContext;

    bool initialized;

    vec2i menuPos;

    int lastActiveGraph;

};

}

#endif
