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

#ifndef _PROLAND_DRAW_OCEAN_FFT_TASK_H_
#define _PROLAND_DRAW_OCEAN_FFT_TASK_H_

#include "ork/render/FrameBuffer.h"
#include "ork/scenegraph/AbstractTask.h"

using namespace ork;

namespace proland
{

/**
 * An AbstractTask to draw a flat or spherical %ocean.
 * @ingroup ocean
 * @authors Eric Bruneton, Antoine Begault, Guillaume Piolat
 */
PROLAND_API class DrawOceanFFTTask : public AbstractTask
{
public:
    /**
     * Creates a new DrawOceanFFTTask.
     *
     * @param radius the radius of the planet for a spherical ocean, or
     *      0 for a flat ocean.
     * @param zmin the maximum altitude at which the ocean must be
     *      displayed.
     * @param brdfShader the Shader used to render the ocean surface.
     */
    DrawOceanFFTTask(float radius, float zmin,
        ptr<Program> fftInit, ptr<Program> fftx, ptr<Program> ffty, ptr<Program> variances,
        ptr<Module> brdfShader);

    /**
     * Deletes this DrawOceanFFTTask.
     */
    virtual ~DrawOceanFFTTask();

    virtual ptr<Task> getTask(ptr<Object> context);

protected:
    /**
     * Creates an uninitialized DrawOceanFFTTask.
     */
    DrawOceanFFTTask();

    /**
     * Initializes this DrawOceanFFTTask.
     *
     * @param radius the radius of the planet for a spherical ocean, or
     *      0 for a flat ocean.
     * @param zmin the maximum altitude at which the ocean must be
     *      displayed.
     * @param brdfShader the Shader used to display the ocean surface.
     */
    void init(float radius, float zmin,
        ptr<Program> fftInit, ptr<Program> fftx, ptr<Program> ffty, ptr<Program> variances,
        ptr<Module> brdfShader);

    void swap(ptr<DrawOceanFFTTask> t);

private:
    /**
     * The radius of the planet for a spherical ocean, or 0 for a flat ocean.
     */
    float radius;

    /**
     * The maximum altitude at which the ocean must be displayed.
     */
    float zmin;

    /**
     * Color of the seabed.
     */
    vec3f seaColor;

    // -------

    ptr<Program> fftInit;

    ptr<Program> fftx;

    ptr<Program> ffty;

    ptr<Program> variances;

    ptr<Texture2D> spectrum12;

    ptr<Texture2D> spectrum34;

    ptr<Texture3D> slopeVariances;

    ptr<Texture2DArray> ffta;

    ptr<Texture2DArray> fftb;

    ptr<FrameBuffer> fftFbo1;

    ptr<FrameBuffer> fftFbo2;

    ptr<FrameBuffer> variancesFbo;

    // -------

    /**
     * Number of pixels per cell to use for the screen space grid
     * used to display the ocean surface.
     */
    int resolution;

    /**
     * Current width of the viewport, in pixels.
     */
    int screenWidth;

    /**
     * Current height of the viewport, in pixels.
     */
    int screenHeight;

    /**
     * The mesh used to display the ocean surface.
     */
    ptr< Mesh<vec2f, unsigned int> > screenGrid;

    // -------

    mat4d oldLtoo;

    vec3d offset;

    // -------

    /**
     * The Shader used to render the ocean surface.
     */
    ptr<Module> brdfShader;

    ptr<UniformMatrix4f> cameraToOceanU;

    ptr<UniformMatrix4f> screenToCameraU;

    ptr<UniformMatrix4f> cameraToScreenU;

    ptr<UniformMatrix4f> oceanToWorldU;

    ptr<UniformMatrix3f> oceanToCameraU;

    ptr<Uniform3f> oceanCameraPosU;

    ptr<Uniform3f> oceanSunDirU;

    ptr<Uniform3f> horizon1U;

    ptr<Uniform3f> horizon2U;

    ptr<Uniform1f> radiusU;

    ptr<Uniform1f> heightOffsetU;

    ptr<Uniform2f> gridSizeU;

    void simulateFFTWaves(float t);

    class Impl : public Task
    {
    public:
        ptr<SceneNode> n;

        ptr<DrawOceanFFTTask> o;

        Impl(ptr<SceneNode> n, ptr<DrawOceanFFTTask> owner);

        virtual ~Impl();

        virtual bool run();
    };
};

}

#endif
