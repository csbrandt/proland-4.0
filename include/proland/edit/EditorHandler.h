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

#ifndef _PROLAND_EDITOR_HANDLER_H_
#define _PROLAND_EDITOR_HANDLER_H_

#include <vector>

#include "ork/math/vec3.h"
#include "ork/math/vec4.h"
#include "ork/scenegraph/SceneNode.h"
#include "ork/ui/EventHandler.h"
#include "proland/terrain/TerrainNode.h"

using namespace std;

using namespace ork;

namespace proland
{

/**
 * Abstract interface of objects that can be edited with an EditorHandler.
 * An Editor is associated with a %terrain, and is used to %edit some
 * attributes of this %terrain via painting (elevation, texture, etc).
 * @ingroup edit
 * @authors Eric Bruneton, Antoine Begault, Guillaume Piolat
 */
PROLAND_API class Editor
{
public:
    Editor(string name="Editor", string group="defaultGroup", bool active = true);

    virtual ~Editor();

    /**
     * Returns the SceneNode defining the position of the %terrain edited by
     * this Editor.
     */
    virtual SceneNode *getTerrain() = 0;

    /**
     * Returns the TerrainNode defining the %terrain edited by this Editor.
     */
    virtual TerrainNode *getTerrainNode() = 0;

    /**
     * Notifies this Editor of a new position of the paint brush.
     *
     * @param pencil a position in world space, and a radius (in world space).
     * @param brushColor value that will be written by the brush.
     * @param paint true if the user is currently painting.
     */
    virtual void setPencil(const vec4f &pencil, const vec4f &brushColor, bool paint) = 0;

    /**
     * Returns the current brush color.
     */
    virtual vec4f getBrushColor() = 0;

    /**
     * Notifies this Editor that a new stroke has been added to the
     * shape being painted on the %terrain.
     *
     * @param strokes the samples that have been taken along the shape
     *      that the user is currently painting on the %terrain. Each
     *      sample contains a position and a radius (in world space).
     */
    virtual void edit(const vector<vec4d> &strokes) = 0;

    /**
     * Notifies this Editor that the user has finished painting a shape
     * on the %terrain.
     */
    virtual void update() = 0;

    virtual void reset() = 0;

    bool isActive();

    void setActive(bool active);

    string getName();

    void setName(string name);

    string getGroup();

    void setGroup(string group);

protected:
    void swap(Editor *e);

private:
    string name;

    string group;

    bool active;
};

/**
 * An EventHandler to paint on terrains and pass %edit events
 * to one or more Editor objects.
 */
PROLAND_API class EditorHandler : public EventHandler
{
public:
    /**
     * Paint brush radius relatively to the screen size.
     */
    float relativeRadius;

    /**
     * Value that will be written by the brush at each stroke.
     */
    float brushColor[4];

    /**
     * Creates a new EditorHandler.
     *
     * @param relativeRadius size of the paint brush relatively
     *      to the screen size.
     */
    EditorHandler(float relativeRadius);

    /**
     * Returns true if this EditorHandler has associated Editor.
     */
    bool hasEditors();

    int getEditorCount();

    Editor *getEditor(int index);

    /**
     * Adds an Editor to this EditorHandler. The given Editor will
     * receive %edit events via its Editor#edit and Editor#update
     * methods.
     *
     * @param editor an abstract Editor.
     */
    void addEditor(Editor *editor);

    /**
     * Removes an Editor from this EditorHandler. The given Editor will
     * no longer receive %edit events via its Editor#edit and Editor#update
     * methods.
     *
     * @param editor an abstract Editor.
     */
    void removeEditor(Editor *editor);

    /**
     * Activates or deactivates this EditorHandler. This method show or
     * hides the pencil depending on the status of this EditorHandler.
     */
    void setActive(bool active);

    /**
     * Activates or deactivates the Editors that belong to 'group' inside this
     * EditorHandler.
     */
    void setActive(string group, bool active);

    /**
     * True if any of the Editors that belong to 'group' is activated.
     */
    bool isActive(string group);

    /**
     * Cancels all editing operations performed on the active Editors.
     */
    void reset();

    virtual void redisplay(double t, double dt);

    virtual bool mouseClick(button b, state s, modifier m, int x, int y);

    virtual bool mouseWheel(wheel b, modifier m, int x, int y);

    virtual bool mouseMotion(int x, int y);

    virtual bool mousePassiveMotion(int x, int y);

    /**
     * Returns the world space point corresponding to the given screen coordinates.
     *
     * @param x horizontal screen coordinate, in pixels.
     * @param y vertical screen coordinate, in pixels.
     */
    vec3d getPosition(int x, int y);

private:
    /**
     * The editors associated with this EditorHandler.
     */
    vector<Editor*> editors;

    /**
     * Paint brush radius in world space.
     */
    float radius;

    /**
     * True if the current edition is over and the editors must be updated during redisplay.
     */
    bool update;

    /**
     * True if the user is currently painting.
     */
    bool paint;

    /**
     * The strokes of the currently painted shape. When
     * the user moves the mouse while in painting mode,
     * strokes are sampled along the drawn path, at minimal
     * distance between each other, and stored in this vector.
     * Each stroke contains a world position and a radius
     * (in world space).
     */
    vector<vec4d> strokes;

    /**
     * Number of new strokes since last call to redisplay.
     */
    int newStrokes;

    /**
     * Last position of mouse in screen space.
     */
    vec2i lastPos;

    /**
     * A copy of the depth buffer when the user started to paint.
     */
    float *depthBuffer;
};

}

#endif
