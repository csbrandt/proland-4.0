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

#ifndef _PROLAND_DRAW_PLANTS_TASK_H_
#define _PROLAND_DRAW_PLANTS_TASK_H_

#include <vector>

#include "ork/scenegraph/AbstractTask.h"

#include "proland/producer/TileProducer.h"
#include "proland/plants/Plants.h"

using namespace std;

using namespace ork;

namespace proland
{

/**
 * TODO.
 * @ingroup plants
 * @author Eric Bruneton
 */
PROLAND_API class DrawPlantsTask : public AbstractTask
{
public:
    /**
     * Creates a new DrawPlantsTask.
     *
     * @param terrain used to determine which subNodes are pointing to the Terrain SceneNodes.
     * @param plants the Plants that contains the patterns & models used for our vegetation.
     */
    DrawPlantsTask(const string &terrain, ptr<Plants> plants);

    /**
     * Deletes a DrawPlantsTask.
     */
    virtual ~DrawPlantsTask();

    /**
     * Initializes #terrainInfos and creates the actual task that will draw plants.
     */
    virtual ptr<Task> getTask(ptr<Object> context);

protected:
    /**
     * Plant models and amplification parameters.
     */
    ptr<Plants> plants; // plant models and amplification parameters

    /**
     * Creates a new DrawPlantsTask.
     */
    DrawPlantsTask();

    /**
     * Initializes the field of a DrawPlantsTask.
     *
     * See #DrawPlantsTask().
     */
    void init(const string &terrain, ptr<Plants> plants);

    void swap(ptr<DrawPlantsTask> t);

private:
    /**
     * Name of the terrain to be amplified.
     */
    string terrain; // name of the terrain to be amplified

    vector< ptr<TileProducer> > producers;

    // Uniforms (renderPlantProg)
    ptr<Uniform3f> cameraPosU;
    ptr<Uniform4f> clipPlane0U;
    ptr<Uniform4f> clipPlane1U;
    ptr<Uniform4f> clipPlane2U;
    ptr<Uniform4f> clipPlane3U;
    ptr<UniformMatrix4f> localToTangentFrameU;
    ptr<UniformMatrix4f> tangentFrameToScreenU;
    ptr<UniformMatrix4f> tangentFrameToWorldU;
    ptr<UniformMatrix3f> tangentSpaceToWorldU;
    ptr<Uniform3f> tangentSunDirU;
    ptr<Uniform3f> focalPosU;
    ptr<Uniform1f> plantRadiusU;

    ptr<Query> q;

    void drawPlants(ptr<SceneNode> context);

    class Impl : public Task
    {
    public:
        ptr<DrawPlantsTask> owner;

        ptr<SceneNode> context;

        Impl(ptr<DrawPlantsTask> owner, ptr<SceneNode> context);

        virtual ~Impl();

        virtual bool run();
    };
};

}

#endif
