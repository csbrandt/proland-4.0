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

#ifndef _PROLAND_UNIFORM_SAMPLER_TILEZ_H_
#define _PROLAND_UNIFORM_SAMPLER_TILEZ_H_

#include <set>

#include "ork/core/Factory.h"

#include "proland/producer/GPUTileStorage.h"
#include "proland/terrain/ReadbackManager.h"
#include "proland/terrain/TileSampler.h"

using namespace ork;

namespace proland
{

/**
 * A TileSampler to be used with a proland::ElevationProducer.
 * This class reads back the elevation data of newly created elevation tiles
 * in order to update the TerrainQuad#zmin and TerrainQuad#zmax fields. It
 * also reads back the elevation value below the current viewer position to
 * update the TerrainNode#groundHeightAtCamera static field.
 * @ingroup terrain
 * @authors Eric Bruneton, Antoine Begault, Guillaume Piolat
 */
PROLAND_API class TileSamplerZ : public TileSampler
{
public:
    /**
     * Creates a new TileSamplerZ.
     *
     * @param name the GLSL name of this uniform.
     * @param producer the producer to be used to create new tiles in #update.
     *      Maybe NULL if this producer is used to update a tileMap (see
     *      #setTileMap). If not NULL, must have a
     *      proland::GPUTileStorage.
     */
    TileSamplerZ(const string &name, ptr<TileProducer> producer = NULL);

    /**
     * Deletes this TileSamplerZ.
     */
    virtual ~TileSamplerZ();

    virtual ptr<Task> update(ptr<SceneManager> scene, ptr<TerrainQuad> root);

protected:
    /**
     * Creates an uninitialized TileSamplerZ.
     */
    TileSamplerZ();

    /**
     * Initializes this TileSamplerZ.
     *
     * @param name the GLSL name of this uniform.
     * @param producer the %producer to be used to create new tiles in #update.
     *      Maybe NULL if this %producer is used to update a tileMap (see
     *      #setTileMap). If not NULL, must have a
     *      proland::GPUTileStorage.
     */
    virtual void init(const string &name, ptr<TileProducer> producer = NULL);

    virtual bool needTile(ptr<TerrainQuad> q);

    virtual void getTiles(Tree *parent, Tree **t, ptr<TerrainQuad> q, ptr<TaskGraph> result);

private:
    /**
     * An internal quadtree to store the texture tile associated with each
     * %terrain quad, and to keep track of tiles that need to be read back.
     */
    class TreeZ : public Tree
    {
    public:
        /**
         * The TerrainQuad whose zmin and zmax values must be updated.
         */
        ptr<TerrainQuad> q;

        /**
         * True if the elevation values of this tile have been read back.
         */
        bool readback;

        /**
         * Completion date of the elevation tile data at the time of the
         * last read back. This is used to trigger a new read back if the
         * elevation data changes.
         */
        unsigned int readbackDate;

        /**
         * Creates a new TreeZ.
         *
         * @param q a %terrain quad.
         */
        TreeZ(Tree *parent, ptr<TerrainQuad> q);

        virtual void recursiveDelete(TileSampler *owner);
    };

    /**
     * A sort operator to sort TreeZ elements. It is used to read back
     * coarsest tiles first (i.e. those whose level is minimal).
     */
    struct TreeZSort : public less<TreeZ*>
    {
        /**
         * Returns true if x's level is less than y's level.
         */
        bool operator()(const TreeZ *x, const TreeZ *y) const;
    };

    /**
     * A ork::ReadbackManager::Callback to readback an
     * elevation tile and to update the zmin and zmax fields
     * of a TerrainQuad.
     */
    class TileCallback : public ReadbackManager::Callback
    {
    public:
        /**
         * The TerrainQuad(s) whose zmin and zmax values must be read back.
         */
        vector< ptr<TerrainQuad> > targets;

        /**
         * True if the first element in target is the quad under the camera.
         * If so its zmin and zmax are used to update TerrainNode::groundHeightAtCamera
         */
        bool camera;

        /**
         * Creates a new ZTileCallback.
         *
         * @param targets the TerrainQuad(s) whose zmin and zmax values
         *      must be read back.
         * @param camera true if the first element in target is the quad
         *      under the camera.
         */
        TileCallback(vector< ptr<TerrainQuad> > &targets, bool camera);

        virtual void dataRead(volatile void *data);
    };

    /**
     * A state object to share the readback managers between all
     * TileSamplerZ instances with the same tile storage.
     */
    class State : public Object
    {
    public:
        /**
         * The tile storage for which this State is built.
         */
        ptr<GPUTileStorage> storage;

        /**
         * Framebuffer object used to compute zmin and zmax of tiles.
         */
        ptr<FrameBuffer> fbo;

        /**
         * Buffer of FBO used to read back the computed zmin and zmax values.
         */
        BufferId readBuffer;

        /**
         * The custom "mipmapping" program used to compute min and max
         * elevation values over a tile.
         */
        ptr<Program> minmaxProg;


        ptr<Uniform4f> viewportU;

        ptr<Uniform3f> sizesU;

        vector< ptr<Uniform4i> > tileU;

        ptr<UniformSampler> inputU;

        /**
         * The readback manager used to perform asynchronous readbacks.
         */
        ptr<ReadbackManager> tileReadback;

        /**
         * The set of texture tile that need to be read back.
         */
        std::set<TreeZ*, TreeZSort> needReadback;

        /**
         * The slot of #storage corresponding to the quad below the camera.
         */
        GPUTileStorage::GPUSlot *cameraSlot;

        /**
         * Relative offset in #cameraSlot of the pixel under the camera.
         * The value of this pixel is readback into TerrainNode::groundHeightAtCamera.
         */
        vec2i cameraOffset;

        /**
         * The last frame for which a readback was performed. This is used
         * to avoid doing more readbacks per frame than specified in the
         * readback managers, when several TileSamplerZ sharing the
         * same State are used.
         */
        unsigned int lastFrame;

        /**
         * Creates a new State for the given tile storage.
         */
        State(ptr<GPUTileStorage> storage);
    };

    /**
     * The ork::Factory to create shared State objects for a given
     * tile storage.
     */
    ptr< Factory< ptr<GPUTileStorage>, ptr<State> > > factory;

    /**
     * The State object for this TileSamplerZ (may be shared
     * with other TileSamplerZ with the same tile storage).
     */
    ptr<State> state;

    /**
     * The %terrain quad directly below the current viewer position.
     */
    TreeZ *cameraQuad;

    /**
     * The relative viewer position in the #cameraQuad quad.
     */
    vec2f cameraQuadCoords;

    /**
     * Last camera position used to perform a readback of the camera elevation
     * above the ground. This is used to avoid reading back this value at each
     * frame when the camera does not move.
     */
    vec3d oldLocalCamera;

    /**
     * Creates a new State for elevation tiles of the given size.
     *
     * @param tileSize an elevation tile size.
     */
    static ptr<State> newState(ptr<GPUTileStorage> storage);

    /**
     * The ork::Factory to create shared State objects for a given
     * elevation tile size.
     */
    static static_ptr< Factory< ptr<GPUTileStorage>, ptr<State> > > stateFactory;

    friend class State;
};

}

#endif
