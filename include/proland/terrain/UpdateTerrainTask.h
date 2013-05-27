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

#ifndef _PROLAND_UPDATE_TERRAIN_TASK_H_
#define _PROLAND_UPDATE_TERRAIN_TASK_H_

#include "ork/scenegraph/AbstractTask.h"
#include "proland/terrain/TerrainNode.h"

using namespace ork;

namespace proland
{

/**
 * An AbstractTask to update a %terrain quadtree. This task simply calls
 * the TerrainNode#update method. In fact this method is called directly
 * by the #getTask method, which returns an empty task graph. Indeed the
 * %terrain quadtree must be up to date in order to get a correct task
 * graph for the proland::TileProducer tasks for the newly
 * created quads (these task graphs are computed by UpdateTileSamplersTask).
 * @ingroup terrain
 * @authors Eric Bruneton, Antoine Begault
 */
PROLAND_API class UpdateTerrainTask : public AbstractTask
{
public:
    /**
     * Creates a new UpdateTerrainTask.
     *
     * @param terrain The %terrain whose quadtree must be updated. The first part
     *      of this "node.name" qualified name specifies the scene node containing
     *      the TerrainNode field. The second part specifies the name of this
     *      TerrainNode field.
     */
    UpdateTerrainTask(const QualifiedName &terrain);

    /**
     * Deletes this UpdateTerrainTask.
     */
    virtual ~UpdateTerrainTask();

    virtual ptr<Task> getTask(ptr<Object> context);

protected:
    /**
     * Creates an uninitialized UpdateTerrainTask.
     */
    UpdateTerrainTask();

    /**
     * Initializes this UpdateTerrainTask.
     *
     * @param terrain The %terrain whose quadtree must be updated. The first part
     *      of this "node.name" qualified name specifies the scene node containing
     *      the TerrainNode field. The second part specifies the name of this
     *      TerrainNode field.
     */
    void init(const QualifiedName &terrain);

    void swap(ptr<UpdateTerrainTask> t);

private:
    /**
     * The %terrain whose quadtree must be updated. The first part of this "node.name"
     * qualified name specifies the scene node containing the TerrainNode
     * field. The second part specifies the name of this TerrainNode field.
     */
    QualifiedName terrain;
};

}

#endif
