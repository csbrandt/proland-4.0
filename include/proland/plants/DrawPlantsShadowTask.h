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

#ifndef _PROLAND_DRAW_PLANTS_SHADOW_TASK_H_
#define _PROLAND_DRAW_PLANTS_SHADOW_TASK_H_

#include <vector>

#include "ork/scenegraph/AbstractTask.h"

#include "proland/producer/TileProducer.h"
#include "proland/plants/Plants.h"

using namespace std;

using namespace ork;

namespace proland
{

#define MAX_SHADOW_MAPS 4

/**
 * TODO.
 * @ingroup plants
 * @author Eric Bruneton
 */
PROLAND_API class DrawPlantsShadowTask : public AbstractTask
{
public:
    /**
     * Creates a new DrawPlantsTask.
     *
     * @param terrain used to determine which subNodes are pointing to the Terrain SceneNodes.
     * @param plants the Plants that contains the patterns & models used for our vegetation.
     */
    DrawPlantsShadowTask(const string &terrain, ptr<Plants> plants);

    /**
     * Deletes a DrawPlantsTask.
     */
    virtual ~DrawPlantsShadowTask();

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
    DrawPlantsShadowTask();

    /**
     * Initializes the field of a DrawPlantsTask.
     *
     * See #DrawPlantsShadowTask().
     */
    void init(const string &terrain, ptr<Plants> plants);

    void swap(ptr<DrawPlantsShadowTask> t);

private:
    /**
     * Name of the terrain to be amplified.
     */
    string terrain; // name of the terrain to be amplified

    vector< ptr<TileProducer> > producers;

    bool initialized;

    ptr<FrameBuffer> frameBuffer;

    // Uniforms (renderPlantsShadowProg)
    ptr<Uniform3f> cameraPosU;
    ptr<UniformMatrix4f> localToTangentFrameU;
    ptr<UniformMatrix4f> tangentFrameToScreenU;
    ptr<Uniform4f> shadowLimitU;
    ptr<Uniform4f> shadowCutsU;
    ptr<UniformMatrix4f> tangentFrameToShadowU[MAX_SHADOW_MAPS];
    ptr<Uniform3f> tangentSunDirU;
    ptr<Uniform3f> focalPosU;
    ptr<Uniform1f> plantRadiusU;

    void drawPlantsShadow(ptr<SceneNode> context);

    class Impl : public Task
    {
    public:
        ptr<DrawPlantsShadowTask> owner;

        ptr<SceneNode> context;

        Impl(ptr<DrawPlantsShadowTask> owner, ptr<SceneNode> context);

        virtual ~Impl();

        virtual bool run();
    };
};

}

#endif
