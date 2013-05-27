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

#ifndef _PROLAND_UNIFORM_SAMPLER_TILE_H_
#define _PROLAND_UNIFORM_SAMPLER_TILE_H_

#include "ork/taskgraph/TaskGraph.h"
#include "ork/scenegraph/SceneManager.h"
#include "proland/producer/TileProducer.h"
#include "proland/terrain/TerrainNode.h"

using namespace ork;

namespace proland
{

/**
 * A ork::Uniform to access texture tiles stored on GPU. This class can
 * set the GLSL uniforms necessary to access a given texture tile on GPU, stored
 * in a proland::GPUTileStorage. This class also manages the creation
 * of new texture tiles when a %terrain quadtree is updated, via a
 * proland::TileProducer.
 * @ingroup terrain
 * @authors Eric Bruneton, Antoine Begault
 */
PROLAND_API class TileSampler : public Object
{
public:
    /**
     * A filter to decide whether a texture tile must be produced or not
     * for a given quad.
     */
    class TileFilter {
    public:
        /**
         * Returns true if a texture tile must be produced for the given quad.
         *
         * @param q a %terrain quad.
         */
        virtual bool storeTile(ptr<TerrainQuad> q) = 0;
    };

    /**
     * Creates a new TileSampler.
     *
     * @param name the GLSL name of this uniform.
     * @param producer the producer to be used to create new tiles in #update.
     *      Maybe NULL if this producer is used to update a tileMap (see
     *      #setTileMap). If not NULL, must have a
     *      proland::GPUTileStorage.
     */
    TileSampler(const std::string &name, ptr<TileProducer> producer = NULL);

    /**
     * Deletes this TileSampler.
     */
    virtual ~TileSampler();

    /**
     * Returns the producer to create new tiles in #update.
     */
    ptr<TileProducer> get();

    /**
     * Returns a %terrain associated with this uniform. Only used with #setTileMap.
     *
     * @param a %terrain index.
     */
    ptr<TerrainNode> getTerrain(int i);

    /**
     * Returns true if texture tiles must be created for leaf quads.
     */
    bool getStoreLeaf();

    /**
     * Returns true if new tiles must be produced asychronously.
     */
    bool getAsync();

    /**
     * Returns true if a (part of) parent tile can be used instead of the
     * tile itself for rendering (this may happen if tiles are produced
     * asynchronously, and if a tile is not ready but a parent tile is).
     */
    bool getMipMap();

    /**
     * Sets the producer to create new tiles in #update. This producer must have
     * a proland::GPUTileStorage.
     *
     * @param producer the producer to create new tiles in #update.
     */
    void set(ptr<TileProducer> producer);

    /**
     * Adds a %terrain associated with this uniform. Only used with #setTileMap.
     *
     * @param terrain a %terrain to be associated with this uniform.
     */
    void addTerrain(ptr<TerrainNode> terrain);

    /**
     * Sets the option to create new tiles for leaf quads or not.
     *
     * @param storeLeaf true to create new tiles for leaf quads.
     */
    void setStoreLeaf(bool storeLeaf);

    /**
     * Sets the option to create new tiles for non leaf quads or not.
     *
     * @param storeParent true to create new tiles for non leaf quads.
     */
    void setStoreParent(bool storeParent);

    /**
     * Sets the option to create new tiles for invisible quads or not.
     *
     * @param storeInvisible true to create new tiles for invisible quads.
     */
    void setStoreInvisible(bool storeInvisible);

    /**
     * Sets the options to create new tiles for arbitrary quads or not.
     *
     * @param filter a filter to decide whether a new texture tile must
     *      be created for a given quad. This filter is added to previously
     *      added filters with this method. A texture tile is created for
     *      a quad if at least one filter returns true for this quad.
     */
    void setStoreFilter(TileFilter* filter);

    /**
     * Sets the option to create new tiles for new quads in synchronous or
     * asychronous way. In synchronous mode, a frame is not displayed until
     * all the tasks to create the tile data have been executed, which can
     * lead to perceptible pauses. In asynchronous mode, the frame is
     * displayed even if all tile data are not yet ready. In this case the
     * first tile ancestor whose data is ready is used instead (the
     * asynchronous mode is only possible is #setStoreParent is true). This
     * mode can lead to visible popping when more precise data suddenly
     * replaces coarse data. NOTE: the asynchronous mode requires a scheduler
     * that supports prefetching of any kind of task (both cpu and gpu).
     * NOTE: you can mix TileSampler in synchronous mode with others
     * using asynchronous mode. Hence some tile data can be produced
     * synchronously while other data is produced asynchronously.
     *
     * @param async true to create tiles asynchrously.
     */
    void setAsynchronous(bool async);

    /**
     * Sets the option to allow using a (part of) parent tile instead of the
     * tile itself for rendering. This option is only useful when the
     * asynchronous mode is set.
     */
    void setMipMap(bool mipmap);

    /**
     * Sets the GLSL uniforms necessary to access the texture tile for
     * the given quad. This methods does nothing if terrains are associated
     * with this uniform (uniform intended to be used with #setTileMap).
     *
     * @param level a quad level.
     * @param tx a quad logical x coordinate.
     * @param ty a quad logical y coordinate.
     */
    void setTile(int level, int tx, int ty);

    /**
     * Sets the GLSL uniforms necessary to access the texture tiles for
     * arbitrary quads on GPU. This method does nothing if terrains have
     * not been associated with this uniform. These terrains must be all
     * the terrains for which the tile map may be used on GPU.
     */
    void setTileMap();

    /**
     * Returns the task %graph necessary to create new texture tiles for
     * newly created quads in the given %terrain quadtree (and to release
     * tiles for destroyed quads). This method returns an empty task %graph
     * if terrains have been associated with this uniform (uniform intended
     * to be used with #setTileMap).
     *
     * @param scene the scene manager.
     * @param root the root of a %terrain quadtree.
     */
    virtual ptr<Task> update(ptr<SceneManager> scene, ptr<TerrainQuad> root);

protected:
    /**
     * An internal quadtree to store the texture tile associated with each
     * %terrain quad.
     */
    class Tree {
    public:
        bool newTree;

        bool needTile;

        /**
         * The parent quad of this quad.
         */
        Tree *parent;

        /**
         * The texture tile associated with this quad.
         */
        TileCache::Tile *t;

        /**
         * The subquads of this quad.
         */
        Tree *children[4];

        /**
         * Creates a new Tree.
         */
        Tree(Tree *parent);

        /**
         * Deletes this Tree. Does not delete its sub elements.
         */
        virtual ~Tree();

        /**
         * Deletes this Tree and all its subelements. Releases
         * all the corresponding texture tiles #t.
         */
        virtual void recursiveDelete(TileSampler *owner);
    };

    /**
     * An internal quadtree to store the texture tiles associated with each quad.
     */
    Tree *root;

    /**
     * Creates an uninitialized TileSampler.
     */
    TileSampler();

    /**
     * Initializes this TileSampler.
     *
     * @param name the GLSL name of this uniform.
     * @param producer the %producer to be used to create new tiles in #update.
     *      Maybe NULL if this %producer is used to update a tileMap (see
     *      #setTileMap). If not NULL, must have a
     *      proland::GPUTileStorage.
     */
    virtual void init(const std::string &name, ptr<TileProducer> producer = NULL);

    /**
     * Returns true if a tile is needed for the given terrain quad.
     *
     * @param q a quadtree node.
     */
    virtual bool needTile(ptr<TerrainQuad> q);

    /**
     * Updates the internal quadtree to make it identical to the given %terrain
     * quadtree. This method releases the texture tiles corresponding to
     * deleted quads.
     *
     * @param t the internal quadtree node corresponding to q.
     * @param q a quadtree node.
     */
    virtual void putTiles(Tree **t, ptr<TerrainQuad> q);

    /**
     * Updates the internal quadtree to make it identical to the given %terrain
     * quadtree. Collects the tasks necessary to create the missing texture
     * tiles, corresponding to newly created quads, in the given task %graph.
     *
     * @param t the internal quadtree node corresponding to q.
     * @param q a quadtree node.
     * @param result the task %graph to collect the tile %producer tasks.
     */
    virtual void getTiles(Tree *parent, Tree **t, ptr<TerrainQuad> q, ptr<TaskGraph> result);

    /**
     * Creates prefetch tasks for the sub quads of quads marked as new in
     * Tree#newTree, in the limit of the prefetch count.
     *
     * @param t the internal quadtree node corresponding to q.
     * @param q a quadtree node.
     * @param[in,out] prefetchCount the maximum number of prefetch tasks
     *      that can be created by this method.
     */
    void prefetch(Tree *t, ptr<TerrainQuad> q, int &prefetchCount);

    /**
     * Checks if the last checked Program is the same as the current one,
     * and updates the Uniforms if necessary.
     */
    void checkUniforms();

    virtual void swap(ptr<TileSampler> p);

private:
    std::string name;

    /**
     * The %producer to be used to create texture tiles for newly created quads.
     */
    ptr<TileProducer> producer;

    /**
     * The terrains associated with this uniform. Only used with #setTileMap.
     */
    std::vector< ptr<TerrainNode> > terrains;

    /**
     * Last used GLSL program. Updated each time #setTile or #setTileMap is called with a
     * different Program. Helps to retrieve the correct Uniforms and store them.
     */
    ptr<Program> lastProgram;

    /**
     * The texture sampler to access the proland::GPUTileStorage.
     */
    ptr<UniformSampler> samplerU;

    /**
     * The coordinates of a tile in the proland::GPUTileStorage.
     */
    ptr<Uniform3f> coordsU;

    /**
     * The relative size of a tile in the proland::GPUTileStorage.
     */
    ptr<Uniform3f> sizeU;

    /**
     * The texture sampler to access the proland::GPUTileStorage
     * tile map (an indirection structure to get the storage coordinate of
     * a tile from its logical coordinates).
     */
    ptr<UniformSampler> tileMapU;

    /**
     * rootTileSize, splitDistance, k=ceil(splitDistance), 4*k+2.
     */
    ptr<Uniform4f> quadInfoU;

    /**
     * Tile size in pixels including borders, border in pixels, tilePool 1/w, 1/h.
     */
    ptr<Uniform4f> poolInfoU;

    /**
     * The current camera position in local space for each %terrain associated
     * with this uniform (only used with #setTileMap).
     */
    std::vector< ptr<Uniform4f> > cameraU;

    /**
     * True to store texture tiles for leaf quads.
     */
    bool storeLeaf;

    /**
     * True to store texture tiles for non leaf quads.
     */
    bool storeParent;

    /**
     * True to store texture tiles for invisible quads.
     */
    bool storeInvisible;

    /**
     * A set of filters to decide whether a texture tile must be stored
     * for a given quad. A texture is stored if at least one filter returns
     * true.
     */
    std::vector<TileFilter*> storeFilters;

    /**
     * True if tiles must be loaded in an asynchronous way, using prefetching.
     */
    bool async;

    /**
     * True if a parent tile can be used instead of the tile itself for rendering.
     */
    bool mipmap;
};

}

#endif
