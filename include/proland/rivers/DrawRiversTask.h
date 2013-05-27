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

#ifndef _PROLAND_DRAWRIVERSTASK_H_
#define _PROLAND_DRAWRIVERSTASK_H_

#include "ork/scenegraph/AbstractTask.h"
#include "ork/render/Mesh.h"

#include "proland/particles/ParticleProducer.h"
#include "proland/particles/LifeCycleParticleLayer.h"
#include "proland/particles/screen/ScreenParticleLayer.h"
#include "proland/particles/terrain/TerrainParticleLayer.h"
#include "proland/rivers/HydroFlowProducer.h"
#include "proland/rivers/WaveTile.h"

using namespace ork;

namespace proland
{

/**
 * @defgroup rivers rivers
 * Provides a framework to draw animated %rivers on top of a %terrain.
 * @ingroup proland
 */

/**
 * This class is used to draw multi-resolution Animated Rivers as described
 * in Qizhi Yu's Thesis [Models of Animated Rivers for the Interactive
 * Exploration of Landscapes - November 2008].
 * See http://www-evasion.imag.fr/Membres/Qizhi.Yu/phd/ for more details
 * about this work.
 * It represents Rivers on 3 different scales :
 * - Macro scale : Overall visual impression of rivers.
 * - Meso scale  : Local waves and effects applied to the river, such as
 * quasi-stationary waves caused by an obstacle, hydraulic jump caused by
 * bed topography change, surface boils...
 * - Micro scale : Small waves on river surface, which conveys the flow
 * motion of the river.
 * @ingroup rivers
 * @author Antoine Begault, Guillaume Piolat
 *
 * Known bugs:
 * - Screen flickers when using postProcess method.
 * - Bad depths retrieved for particles on some viewpoints.
 */
PROLAND_API class DrawRiversTask : public AbstractTask
{
public:
    enum MeshDisplayType {
        NONE = 0,
        PARTICLE_COVERAGE = 1,
        ADVECTED = 5,
        PRE_ADVECTED = 6,
        NON_ADVECTED = 10,
        MESH_ONLY = 11
    };

    /**
     * Information about a terrainNode, such as the corresponding
     * particleProducer, elevation cache, display informations etc..
     */
    struct TerrainInfo
    {
        /**
         * Id of this terrain.
         */
        int id;

        /**
         * SceneNode (terrainNode + texture caches).
         */
        ptr<SceneNode> tn;

        /**
         * TerrainNode of the terrain.
         */
        ptr<TerrainNode> t;

        /**
         * FlowData Factory.
         */
        ptr<TileProducer> flows;

        ptr<UniformMatrix4f> screenToLocalU;

        string name;

    };

    struct vecParticle{

        float x, y, tx, ty, ox, oy, i, r, id;

        vecParticle();

        vecParticle(float x, float y, float tx, float ty, float ox, float oy,
                float i, float r, float id);

        vecParticle(ParticleProducer *producer, ParticleStorage::Particle *p);
    };

    /**
     * Creates a new DrawRiversTask.
     *
     * @param renderTexProg the GLSL Program used to draw the pre-rendering texture.
     * @param particlesProg the GLSL Program used to draw the particles.
     * @param particles the TerrainParticleManager used to create the flow.
     * @param timeStep time step at each frame. Changes the speed of
     *      the river.
     * @param drawParticles determines whether we draw the particles or
     *      not. Particles are drawn as colored dots.
     * @param tex the texture used to advect normals.
     * @param bedTex the texture used to draw the river bed.
     * @param riverDepth river's depth.
     * @param waveSlopeFactor factor for waves' size.
     * @param useOffscreenDepth used to determine if we need to copy the DepthBuffer.
     */
    DrawRiversTask(ptr<Program> renderTexProg, ptr<Program> particlesProg, ptr<ParticleProducer> particles, float timeStep = 1.0f,
        bool drawParticles = false, ptr<WaveTile> tex = NULL, ptr<WaveTile> bedTex = NULL,
        float riverDepth = 1.0f, float waveSlopeFactor = 1.f, bool useOffscreenDepth = false);
    /**
     * Deletes this DrawRiversTask.
     */
    virtual ~DrawRiversTask();

    /**
     * Returns the task(s) to be executed for this object.
     * It checks which tiles ParticleProducer needs to produce,
     * depending on the current view, and puts it in the returned
     * TaskGraph. The corresponding TerainParticleManagers will be produced
     * <i>before</i> the #run() method call.
     *
     * @param context see Method.
     */
    virtual ptr<Task> getTask(ptr<Object> context);

    /**
     * Returns #particles.
     */
    ptr<ParticleProducer> getParticles() const;

    void setParticleRadius(float radius);

    void setSlipParameter(int id, float slip);

    void setPotentialDelta(int id, float potential);

    void setTimeStep(float timeStep);

    void displayGrid(bool display);

    void displayParticles(bool display);

    void displayVelocities(bool display);

    void displaySunEffects(bool display);

    void setRiverDepth(float depth);

    void setWaveSlopeFactor(float slopeFactor);

    void setWaveLength(float length);

    void setBedLength(float length);

    void setMeshDisplayType(MeshDisplayType type);

    float getParticleRadius() const;

    float getSlipParameter(int id) const;

    float getPotentialDelta(int id) const;

    float getTimeStep() const;

    bool displayGrid() const;

    bool displayParticles() const;

    bool displayVelocities() const;

    bool displaySunEffects() const;

    float getWaveSlopeFactor() const;

    float getRiverDepth() const;

    float getWaveLength() const;

    float getBedLength() const;

    MeshDisplayType getMeshDisplayType() const;

protected:
    /**
     * Creates a n uninitialized DrawRiversTask.
     */
    DrawRiversTask();

    /**
     * Initializes this DrawRiversTask.
     *
     * See #DrawRiversTask.
     */
    void init(ptr<Program> renderTexProg, ptr<Program> particlesProg, ptr<ParticleProducer> particles, float timeStep = 1.0f,
        bool drawParticles = false, ptr<WaveTile> tex = NULL, ptr<WaveTile> bedTex = NULL,
        float riverDepth = 1.0f, float waveSlopeFactor = 1.0f, bool useOffscreenDepth = false);

    virtual void swap(ptr<DrawRiversTask> t);

private:
    class Impl : public Task
    {
    public:
        DrawRiversTask *owner;

        Impl(DrawRiversTask *owner);

        virtual ~Impl();

        virtual bool run();
    };

    /**
     * Draws particles for a given Particle Producer.
     *
     * @param pp the particle producer to draw.
     */
    void doDrawParticles(ptr<ParticleProducer> pp);

    /**
     * Main method called for drawing rivers.
     * Calls the TerrainParticleManager#timeStep() method to update PM.
     * Updates sprites & sprites parameters tables.
     * Draws Particles if required, and finally draws 3D animated Rivers.
     */
    void drawRivers();

    /**
     * Draw Mode. Determines what will be drawn in the shader. Depends
     * on MESH_DISPLAY_TYPE and drawParticles and drawVelocities.
     */
    ptr<Uniform1f> drawModeU;

    /**
     * Determines if the particle grid is displayed or not.
     */
    ptr<Uniform1f> displayGridU;

    /**
     * Whether sun effects are enabled or not.
     */
    ptr<Uniform1f> sunEffectsU;

    /**
     * Factor for waves' size.
     */
    ptr<Uniform1f> waveSlopeFactorU;

    /**
     * River's depth.
     */
    ptr<Uniform1f> riverDepthU;

    /**
     * True if the user wants to display a bed texture under the river.
     */
    ptr<Uniform1f> useBedTexU;

    /**
     * Screen Size.
     */
    ptr<Uniform2f> screenSizeU;

    /**
     * Particles grid size.
     */
    ptr<Uniform2f> gridSizeU;

    /**
     * Table containing the particles parameters.
     */
    ptr<UniformSampler> spriteParamTableU;

    /**
     * Indirection table for particles.
     */
    ptr<UniformSampler> uniformSpriteGridU;

    /**
     * The pre-rendered texture.
     */
    ptr<UniformSampler> riverTexU;

    /**
     * Displayed Points' size.
     */
    ptr<Uniform1f> particleSizeU;

    /**
     * Displayed Points' size.
     */
    ptr<Uniform2f> sizeU;

    ptr<Uniform2f> preRenderScreenSizeU;

    ptr<UniformSampler> depthBufferU;

    /**
     * List of terrains used by the particle manager.
     */
    vector<TerrainInfo> terrainInfos;
    /**
     * The TerainParticleManager used to create the flow.
     */
    ptr<ParticleProducer> particles;

    /**
     * Sprites Param table.
     */
    ptr<Texture2D> spTable;

    /**
     * Uniform Sprite Grid.
     */
    ptr<Texture2D> usGrid;

    /**
     * An offscreen FrameBuffer used to create the advected
     * texture when in pre-advected drawing mode.
     */
    ptr<FrameBuffer> frameBuffer;

    /**
     * Used to determine if we need to copy the Depth Buffer
     * into a local texture so it will be accessible from GLSL context.
     */
    bool useOffscreenDepth;

    /**
     * A copy of the Depth Buffer, or the Depth Buffer itself if using
     * an offscreen Depth Buffer.
     */
    ptr<Texture2D> depthBuffer;

    /**
     * Pre-rendering method resulting texture.
     */
    ptr<Texture2D> advectedTex;

    /**
     * The SceneManager on which we want to draw rivers.
     */
    SceneManager *scene;

    /**
     * The ParticleGrid used to store and copy particles to GPU.
     */
    ParticleGrid *particleGrid;

    /**
     * The ParticleLayer that handles particles life cycle data.
     */
    ptr<LifeCycleParticleLayer> lifeCycleLayer;

    /**
     * The ParticleLayer that handles particles screen coordinates.
     */
    ptr<ScreenParticleLayer> screenLayer;

    /**
     * The ParticleLayer that handles particles local coordinates.
     */
    ptr<TerrainParticleLayer> terrainLayer;

    /**
     * Time step at each frame. Changes the speed of the river.
     */
    float timeStep;

    /**
     * Determines whether we draw the particles or not. Particles are
     * drawn as colored dots.
     */
    bool drawParticles;

    /**
     * Determines whether we draw the particles  velocitiesor not.
     */
    bool drawVelocities;

    /**
     * Determines if we display the grid or not.
     */
    bool drawGrid;

    /**
     * Determines whether we draw sun effects (reflectance...).
     */
    bool sunEffects;

    /**
     * Determines the way we display the mesh.
     */
    MeshDisplayType drawMesh;

    /**
     * The GLSL Program used to draw pre-rendering texture.
     */
    ptr<Program> renderTexProg;

    /**
     * The GLSL Program used to draw the particles.
     */
    ptr<Program> particlesProg;

    /**
     * Texture used to render the river. See Q. Yu's paper for its
     * requirement's.
     */
    ptr<WaveTile> riverTex;

    /**
     * Texture used to render the river bed. See Q. Yu's paper for its
     * requirement's.
     */
    ptr<WaveTile> bedTex;

    /**
     * Mesh used to draw the particles.
     */
    ptr<Mesh<vec3f, unsigned int> > mesh;

    /**
     * Mesh used to draw the particles.
     */
    ptr<Mesh<vecParticle, unsigned int> > particleMesh;

    /**
     * Determines whether we need to recover the list of terrains
     * associated to this Task.
     */
    bool initialized;

    /**
     * River's depth.
     */
    float riverDepth;

    /**
     * Factor for waves' size.
     */
    float waveSlopeFactor;

};

}

#endif
