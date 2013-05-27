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

#ifndef _PROLAND_TILE_PRODUCER_H_
#define _PROLAND_TILE_PRODUCER_H_

#include "ork/math/vec2.h"
#include "ork/math/vec4.h"
#include "ork/taskgraph/Scheduler.h"
#include "ork/taskgraph/TaskGraph.h"
#include "proland/producer/TileCache.h"
#include "proland/producer/TileLayer.h"

using namespace ork;

namespace ork
{

class SceneManager;

}

namespace proland
{

/**
 * An abstract %producer of tiles.
 * Note that several TileProducer can share the same TileCache, and hence the
 * same TileStorage.
 * @ingroup producer
 * @authors Eric Bruneton, Antoine Begault, Guillaume Piolat
 */
PROLAND_API class TileProducer : public Object
{
public:
    /**
     * Creates a new TileProducer.
     *
     * @param type the type of this %producer.
     * @param taskType the type of the Task that produce the actual tile data.
     * @param cache the tile cache that stores the tiles produced by this %producer.
     * @param gpuProducer true if this %producer produces textures on GPU.
     */
    TileProducer(const char* type, const char *taskType, ptr<TileCache> cache, bool gpuProducer);

    /**
     * Deletes this TileProducer.
     */
    virtual ~TileProducer();

    /**
     * Returns the size in meters of the root quad produced by this %producer.
     */
    float getRootQuadSize();

    /**
     * Sets the size in meters of the root quad produced by this %producer.
     *
     * @param size the size of the root quad of this %producer.
     */
    virtual void setRootQuadSize(float size);

    /**
     * Returns the id of this %producer. This id is local to the TileCache used by
     * this %producer, and is used to distinguish all the producers that use this
     * cache.
     */
    int getId();

    /**
     * Returns the TileCache that stores the tiles produced by this %producer.
     */
    virtual ptr<TileCache> getCache();

    /**
     * Returns true if this %producer produces textures on GPU.
     */
    bool isGpuProducer();

    /**
     * Returns the size in pixels of the border of each tile. Tiles made of
     * raster data may have a border that contains the value of the neighboring
     * pixels of the tile. For instance if the tile size (returned by
     * TileStorage#getTileSize) is 196, and if the tile border is 2, this means
     * that the actual tile data is 192x192 pixels, with a 2 pixel border that
     * contains the value of the neighboring pixels. Using a border introduces
     * data redundancy but is usefull to get the value of the neighboring pixels
     * of a tile without needing to load the neighboring tiles.
     */
    virtual int getBorder();

    /**
     * Returns true if this %producer can produce the given tile.
     *
     * @param level the tile's quadtree level.
     * @param tx the tile's quadtree x coordinate.
     * @param ty the tile's quadtree y coordinate.
     */
    virtual bool hasTile(int level, int tx, int ty);

    /**
     * Returns true if this %producer can produce the children of the given tile.
     *
     * @param level the tile's quadtree level.
     * @param tx the tile's quadtree x coordinate.
     * @param ty the tile's quadtree y coordinate.
     */
    bool hasChildren(int level, int tx, int ty);

    /**
     * Looks for a tile in the TileCache of this TileProducer.
     *
     * @param level the tile's quadtree level.
     * @param tx the tile's quadtree x coordinate.
     * @param ty the tile's quadtree y coordinate.
     * @param includeCache true to include both used and unused tiles in the
     *      search, false to include only the used tiles.
     * @param done true to check that the tile's creation task is done.
     * @return the requested tile, or NULL if it is not in the TileCache or
     *      if 'done' is true, if it is not ready. This method does not change the
     *      number of users of the returned tile.
     */
    virtual TileCache::Tile* findTile(int level, int tx, int ty, bool includeCache = false, bool done = false);

    /**
     * Returns the requested tile, creating it if necessary. If the tile is
     * currently in use it is returned directly. If it is in cache but unused,
     * it marked as used and returned. Otherwise a new tile is created, marked
     * as used and returned. In all cases the number of users of this tile is
     * incremented by one.
     *
     * @param level the tile's quadtree level.
     * @param tx the tile's quadtree x coordinate.
     * @param ty the tile's quadtree y coordinate.
     * @param deadline the deadline at which the tile data must be ready. 0 means
     *      the current frame.
     * @return the requested tile, or NULL if there is no room left in the
     *      TileStorage to store the requested tile.
     */
    virtual TileCache::Tile* getTile(int level, int tx, int ty, unsigned int deadline);

    /**
     * Returns the coordinates in the GPU storage of the given tile. If the
     * given tile is not in the GPU storage, this method uses the first ancestor
     * of this tile that is in the storage. It then returns the coordinates of
     * the area of this ancestor tile that correspond to the requested tile.
     *
     * @param level the tile's quadtree level.
     * @param tx the tile's quadtree x coordinate.
     * @param ty the tile's quadtree y coordinate.
     * @param[in,out] tile the tile (level,tx,ty) or its first ancestor that is
     *     in the storage, if known. If no tile is specified (passing NULL) this
     *     method finds it and returns it in this parameter.
     * @return the coordinates in the GPU storage texture of the requested tile.
     *     The x,y components correspond to the lower left corner of the tile in
     *     the GPU storage texture. The z,w components correspond to the width
     *     and height of the tile. All components are in texture coordinates,
     *     between 0 and 1. If the tile has borders, the returned coordinates
     *     correspond to the inside of the tile, excluding its border.
     */
    vec4f getGpuTileCoords(int level, int tx, int ty, TileCache::Tile **tile);

    /**
     * Schedules a prefetch task to create the given tile. If the requested tile
     * is currently in use or in cache but unused, this method does nothing.
     * Otherwise it gets an unused tile storage (evicting an unused tile if
     * necessary), and then creates and schedules a task to produce the data of
     * the requested tile.
     *
     * @param level the tile's quadtree level.
     * @param tx the tile's quadtree x coordinate.
     * @param ty the tile's quadtree y coordinate.
     * @return true if this method has been able to schedule a prefetch task
     *      for the given tile.
     */
    virtual bool prefetchTile(int level, int tx, int ty);

    /**
     * Decrements the number of users of this tile by one. If this number
     * becomes 0 the tile is marked as unused, and so can be evicted from the
     * cache at any moment.
     *
     * @param t a tile currently in use.
     */
    virtual void putTile(TileCache::Tile *t);

    /**
     * Invalidates the tiles produced by this producer.
     * This means that the tasks to produce the actual data of these tiles will
     * be automatically reexecuted before the data can be used.
     */
    virtual void invalidateTiles();

    /**
     * Invalidates the selected tile produced by this producer.
     * This means that the tasks to produce the actual data of this tile will
     * be automatically reexecuted before the data can be used.
     */
    virtual void invalidateTile(int level, int tx, int ty);

    /**
     * Updates the tiles produced by this producer, if necessary.
     * The default implementation of this method does nothing.
     */
    virtual void update(ptr<SceneManager> scene);

    /**
     * Updates the GPU tile map for this %producer. A GPU tile map allows a GPU
     * shader to retrieve the data of any tile from its level,tx,ty coordinates,
     * thanks to a mapping between these coordinates and texture tile storage
     * u,v coordinates, stored in the tile map. This is only possible if the
     * quadtree of tiles is subdivided only based on the distance to the camera.
     * The camera position and the subdivision parameters are needed to create
     * the tile map and to decode it on GPU.
     *
     * @param splitDistance the distance at which a quad is subdivided. In fact
     *      a quad is supposed to be subdivided if the camera distance is less
     *      than splitDistance times the quad size (in meters).
     * @param camera the camera position. This position, together with
     *      splitDistance and rootQuadSize completely define the current
     *      quadtree (i.e. which quads are subdivided, and which are not).
     * @param maxLevel the maximum subdivision level of the quadtree (included).
     * @return true if the tile storage for this %producer has a tile map (see
     *      GPUTileStorage#getTileMap). Otherwise this method does nothing.
     */
    bool updateTileMap(float splitDistance, vec2f camera, int maxLevel);

    /**
     * Returns the tile producers used by this TileProducer.
     *
     * @param[out] producers the tile producers used by this TileProducer.
     */
    virtual void getReferencedProducers(vector< ptr<TileProducer> > &producers) const;

    /**
     * Returns the number of layers of this producer.
     */
    int getLayerCount() const;

    /**
     * Returns the layer of this producer whose index is given.
     *
     * @param index a layer index between 0 and #getLayerCount (exclusive).
     */
    ptr<TileLayer> getLayer(int index) const;

    /**
     * Returns true if the list of layers is not empty. False otherwise.
     */
    bool hasLayers() const;

    /**
     * Adds a Layer to this %producer.
     *
     * @param l the layer to be added.
     */
    void addLayer(ptr<TileLayer> l);

protected:
    /**
     * Creates an uninitialized TileProducer.
     *
     * @param type the type of this %producer.
     * @param taskType the type of the Task that produce the actual tile data.
     */
    TileProducer(const char* type, const char *taskType);

    /**
     * Initializes this TileProducer.
     *
     * @param cache the tile cache that stores the tiles produced by this %producer.
     * @param gpuProducer true if this %producer produces textures on GPU.
     */
    void init(ptr<TileCache> cache, bool gpuProducer);

    virtual void swap(ptr<TileProducer> p);

    /**
     * Returns the context for the Task that produce the tile data.
     * This is only needed for GPU tasks (see Task#getContext).
     * The default implementation of this method does nothing and returns NULL.
     */
    virtual void* getContext() const;

    /**
     * Starts the creation of a tile of this %producer. This method is used for
     * producers that need tiles produced by other producers to create a tile.
     * In these cases this method must acquire these other tiles with #getTile
     * so that these tiles are available with #findTile during the actual tile
     * creation in #doCreateTile.
     *
     * @param level the tile's quadtree level.
     * @param tx the tile's quadtree x coordinate.
     * @param ty the tile's quadtree y coordinate.
     * @param deadline the deadline at which the tile data must be ready. 0
     *      means the current frame.
     * @param task the task to produce the tile itself.
     * @param owner the task %graph that contains 'task', or NULL. This task
     *      %graph can be used if 'task' depends on other tasks. These other
     *      tasks must then be added to 'owner'.
     * @return the task or task %graph to produce the tile itself, and all the
     *      tiles needed to produce it. The default implementation of this
     *      method calls Layer#startCreateTile on each layer, and returns
     *      'owner' if it is not NULL (otherwise it returns 'task'). NOTE: if
     *      a task graph is returned, it must be created with #createTaskGraph.
     */
    virtual ptr<Task> startCreateTile(int level, int tx, int ty, unsigned int deadline, ptr<Task> task, ptr<TaskGraph> owner);

    /**
     * Sets the execution context for the Task that produces the tile data.
     * This is only needed for GPU tasks (see Task#begin).
     * The default implementation of this method calls Layer#beginCreateTile on
     * each Layer of this %producer.
     */
    virtual void beginCreateTile();

    /**
     * Creates the given tile. If this task requires tiles produced by other
     * producers (or other tiles produced by this %producer), these tiles must
     * be acquired and released in #startCreateTile and #stopCreateTile with
     * #getTile and #putTile, and retrieved in this method with #findTile.
     * The default implementation of this method calls Layer#doCreateTile on
     * each Layer of this %producer.
     *
     * @param level the tile's quadtree level.
     * @param tx the tile's quadtree x coordinate.
     * @param ty the tile's quadtree y coordinate.
     * @param data where the created tile data must be stored.
     * @return true the result of this creation is different from the result
     *      of the last creation of this tile. See Task#run.
     */
    virtual bool doCreateTile(int level, int tx, int ty, TileStorage::Slot *data);

    /**
     * Restores the execution context for the Task that produces the tile data.
     * This is only needed for GPU tasks (see Task#end).
     * The default implementation of this method calls Layer#endCreateTile on
     * each Layer of this %producer.
     */
    virtual void endCreateTile();

    /**
     * Stops the creation of a tile of this %producer. This method is used for
     * producers that need tiles produced by other producers to create a tile.
     * In these cases this method must release these other tiles with #putTile
     * so that these tiles can be evicted from the cache after use. The default
     * implementation of this method calls Layer#stopCreateTile on each Layer of
     * this %producer.
     *
     * @param level the tile's quadtree level.
     * @param tx the tile's quadtree x coordinate.
     * @param ty the tile's quadtree y coordinate.
     */
    virtual void stopCreateTile(int level, int tx, int ty);

    /**
     * Removes a task from the vector tasks. This is used to avoid delete-calls
     * for objects that were already deleted.
     * This is called when a task created by this producer gets deleted.
     */
    void removeCreateTile(Task *t);

    /**
     * Creates a task %graph for use in #startCreateTile.
     *
     * @param task a task that will be added to the returned task %graph.
     * @return a task %graph containing 'task', for use in #startCreateTile.
     */
    ptr<TaskGraph> createTaskGraph(ptr<Task> task);

    friend class CreateTile;

    friend class CreateTileTaskGraph;

private:
    /**
     * The list of all the Layers used by this %producer.
     */
    vector< ptr<TileLayer> > layers;

    /**
     * The list of all the Tasks created by this %producer.
     */
    vector<Task*> tasks;

    /**
     * The type of the Task that produce the actual tile data.
     */
    char* taskType;

    /**
     * The tile cache that stores the tiles produced by this %producer.
     */
    ptr<TileCache> cache;

    /**
     * True if this %producer produces textures on GPU.
     */
    bool gpuProducer;

    /**
     * The id of this %producer. This id is local to the TileCache used by this
     * %producer, and is used to distinguish all the producers that use this
     * cache.
     */
    int id;

    /**
     * The size in meters of the root tile produced by this %producer.
     */
    float rootQuadSize;

    /**
     * The data of the tileMap texture line on GPU for this %producer. If a
     * quadtree is subdivided based only on the distance to the camera, it is
     * possible to compute on GPU which level,tx,ty tile corresponds to an x,y
     * position in meters. But in order to compute where the data of this tile
     * is stored in a texture tile storage, the mapping between level,tx,ty
     * coordinates and texture tile storage u,v coordinates is needed. A tileMap
     * provides this information (it must be updated each time the camera moves
     * or the tile storage layout changes). Each line of this tileMap
     * corresponds to a single %producer. Only GPU producers can have a tileMap.
     */
    unsigned char* tileMap;

    /**
     * A mutex to serialize parallel accesses to #tasks.
     */
    void* mutex;

    /**
     * Creates a Task to produce the data of the given tile.
     *
     * @param level the tile's quadtree level.
     * @param tx the tile's quadtree x coordinate.
     * @param ty the tile's quadtree y coordinate.
     * @param data where the produced tile data must be stored.
     * @param deadline the deadline at which the tile data must be ready. 0 means
     *      the current frame.
     * @param t the existing task to create this tile if it still exists, or NULL.
     */
    ptr<Task> createTile(int level, int tx, int ty, TileStorage::Slot *data, unsigned int deadline, ptr<Task> old);

    friend class TileCache;
};

}

#endif
