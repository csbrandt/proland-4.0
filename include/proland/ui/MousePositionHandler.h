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

#ifndef _PROLAND_MOUSEPOSITIONHANDLER_H_
#define _PROLAND_MOUSEPOSITIONHANDLER_H_

#include <vector>

#include "ork/scenegraph/SceneNode.h"
#include "ork/ui/EventHandler.h"

#include "proland/terrain/TerrainNode.h"

using namespace std;

using namespace ork;

namespace proland
{

/**
 * An EventHandler that can determine the position of the mouse in world space.
 * It can determine on which TerrainNode the cursor is, and the position inside it.
 * This EventHandler is only for debug purpose, since it requires costly operations.
 * (DepthBuffer read...). It then uses the ShowInfoTask to display the mouse position.
 * @ingroup proland_ui
 * @author Antoine Begault
 */
PROLAND_API class MousePositionHandler : public EventHandler
{
public:
    /**
     * Creates a new MousePositionhandler.
     *
     * @param terrains the list of SceneNodes contained in the scene mapped to their TerrainNodes.
     *      These SceneNodes are used to determine the transformation matrices, and the TerrainNodes
            are used to determine if the point is inside the terrain.
     * @param next the EventHandler that must handle the events.
     */
    MousePositionHandler(map<ptr<SceneNode>, ptr<TerrainNode> > terrains, ptr<EventHandler> next);

    /**
     * Deletes this MousePositionHandler.
     */
    virtual ~MousePositionHandler();

    virtual void redisplay(double t, double dt);

    virtual void reshape(int x, int y);

    virtual void idle(bool damaged);

    /**
     * Finds the TerrainQuad that contains the given coordinates.
     */
    ptr<TerrainQuad> findTile(float x, float y, ptr<TerrainQuad> quad);

    /**
     * Determines the terrain and the terrain tile that contains the given coordinates.
     * It will set #mousePosition, #currentTerrain, #terrainPosition, and #tile.
     */
    void getWorldCoordinates(int x, int y);

    virtual bool mouseClick(button b, state s, modifier m, int x, int y);

    virtual bool mouseMotion(int x, int y);

    virtual bool mousePassiveMotion(int x, int y);

    virtual bool mouseWheel(wheel b, modifier m, int x, int y);

    virtual bool keyTyped(unsigned char c, modifier m, int x, int y);

    virtual bool keyReleased(unsigned char c, modifier m, int x, int y);

    virtual bool specialKey(key k, modifier m, int x, int y);

    virtual bool specialKeyReleased(key k, modifier m, int x, int y);

protected:
    /**
     * The list of SceneNodes contained in the scene mapped to their TerrainNodes.
     * These SceneNodes are used to determine the transformation matrices, and the TerrainNodes
     * are used to determine if the point is inside the terrain.
     */
    map<ptr<SceneNode>, ptr<TerrainNode> > terrains;

    /**
     *Creates a new MousePositionHandler.
     */
    MousePositionHandler();

    /**
     * Initializes this MousePositionHandler.
     * See #MousePositionHandler.
     */
    void init(map<ptr<SceneNode>, ptr<TerrainNode> > terrains, ptr<EventHandler> next);

    void swap(ptr<MousePositionHandler> mousePositionHandler);

private:
    /**
     * Displayed mouse position.
     * Retrieved when the mouse moves.
     */
    vec2i mousePosition;

    float mousePositionZ;

    /**
     * The terrain pointed by the cursor.
     * If no terrain is under the cursor, it will be set to -1 and no terrain coordinates
     * will be displayed, only mouse coordinates.
     */
    int currentTerrain;

    /**
     * Local position inside the terrain pointer by the cursor.
     */
    vec3d terrainPosition;

    /**
     * Coordinates of the tile (level, tx, ty) pointed inside the terrain.
     */
    vec3i tile;

    /**
     * The EventHandler that must handle the events.
     */
    ptr<EventHandler> next;

};

}

#endif
