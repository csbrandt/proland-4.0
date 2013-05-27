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

#ifndef _PROLAND_TILE_CACHE_H_
#define _PROLAND_TILE_CACHE_H_

#include <map>
#include <vector>
#include <string>

#include "ork/taskgraph/Scheduler.h"
#include "proland/producer/TileStorage.h"

using namespace ork;

namespace proland
{

class TileProducer;

/**
 * A cache of tiles to avoid recomputing recently produced tiles. A tile cache
 * keeps track of which tiles (identified by their level,tx,ty coordinates) are
 * currently stored in an associated TileStorage. It also keeps track of which
 * tiles are in use, and which are not. Unused tiles are kept in the TileStorage
 * as long as possible, in order to avoid re creating them if they become needed
 * again. But the storage associated with unused tiles can be reused to store
 * other tiles at any moment (in this case we say that a tile is evicted from
 * the cache of unused tiles).
 * Conversely, the storage associated with tiles currently in use cannot be
 * reaffected until these tiles become unused. A tile is in use when it is
 * returned by #getTile, and becomes unused when #putTile is called (more
 * precisely when the number of users of this tile becomes 0, this number being
 * incremented and decremented by #getTile and #putTile, respectively). The
 * tiles that are needed to render the current frame should be declared in use,
 * so that they are not evicted between their creation and their actual
 * rendering.
 * @ingroup producer
 * @authors Eric Bruneton, Antoine Begault, Guillaume Piolat
 */
PROLAND_API class TileCache : public Object
{
public:
    /**
     * A tile described by its level,tx,ty coordinates. A TileCache::Tile
     * describes where the tile is stored in the TileStorage, how its data can
     * be produced, and how many users currently use it.
     */
    class Tile
    {
    public:
        /**
         * A tile identifier for a given producer. Contains the tile coordinates
         * level, tx, ty.
         */
        typedef pair<int, pair<int, int> > Id;

        /**
         * A tile identifier. Contains a %producer id (first pair element) and
         * tile coordinates level,tx,ty (second pair element).
         */
        typedef pair<int, Id> TId;

        /**
         * The id of the %producer that manages this tile.  This local id is
         * assigned to each new %producer that uses this TileCache.
         */
        const int producerId;

        /**
         * The quadtree level of this tile.
         */
        const int level;

        /**
         * The quadtree x coordinate of this tile at level #level.
         * Varies between 0 and 2^level - 1.
         */
        const int tx;

        /**
         * The quadtree y coordinate of this tile at level #level.
         * Varies between 0 and 2^level - 1.
         */
        const int ty;

        /**
         * The task that produces or produced the actual tile data.
         */
        const ptr<Task> task;

        /**
         * Creates a new tile.
         *
         * @param producerId the id of the %producer of this tile.
         * @param level the quadtree level of this tile.
         * @param tx the quadtree x coordinate of this tile.
         * @param ty the quadtree y coordinate of this tile.
         * @param task the task that will produce the tile data.
         * @param data where the produced tile data must be stored.
         */
        Tile(int producerId, int level, int tx, int ty, ptr<Task> task, TileStorage::Slot *data);

        /**
         * Deletes this tile. This does not delete the tile data itself, only
         * the mapping between the tile and its location in the TileStorage.
         */
        ~Tile();

        /**
         * Returns the actual data of this tile.
         *
         * @param check true to check that the task that produced this data is
         *      actually done.
         * @return the actual data of this tile, or NULL if the task that
         *      produces this data is not done.
         */
        TileStorage::Slot *getData(bool check = true);

        /**
         * Returns the identifier of this tile.
         */
        Id getId() const;

        /**
         * Returns the identifier of this tile.
         */
        TId getTId() const;

        /**
         * Returns the identifier of a tile.
         *
         * @param level the tile's quadtree level.
         * @param tx the tile's quadtree x coordinate.
         * @param ty the tile's quadtree y coordinate.
         */
        static Id getId(int level, int tx, int ty);

        /**
         * Returns the identifier of a tile.
         *
         * @param producerId the id of the tile's %producer.
         * @param level the tile's quadtree level.
         * @param tx the tile's quadtree x coordinate.
         * @param ty the tile's quadtree y coordinate.
         */
        static TId getTId(int producerId, int level, int tx, int ty);

    private:
        /**
         * The actual data of this tile. This data is not ready before #task is
         * done.
         */
        TileStorage::Slot *data;

        /**
         * The number of users of this tile. This number is incremented by
         * #getTile and decremented by #putTile. When it becomes 0 the tile
         * becomes unused.
         */
        int users;

        friend class TileCache;

        friend class CreateTile;
    };

    /**
     * Creates a new TileCache.
     *
     * @param storage the tile storage to store the actual tiles data.
     * @param name name of this cache, for logging.
     * @param scheduler an optional scheduler to schedule the creation of
     *      prefetched tiles. If no scheduler is specified, prefetch is
     *      disabled.
     */
    TileCache(ptr<TileStorage> storage, std::string name, ptr<Scheduler> scheduler = NULL);

    /**
     * Deletes this TileCache.
     */
    virtual ~TileCache();

    /**
     * Returns the storage used to store the actual tiles data.
     */
    ptr<TileStorage> getStorage();

    /**
     * Returns the scheduler used to schedule prefetched tiles creation tasks.
     */
    ptr<Scheduler> getScheduler();

    /**
     * Returns the number of tiles currently in use in this cache.
     */
    int getUsedTiles();

    /**
     * Returns the number of tiles currently unused in this cache.
     */
    int getUnusedTiles();

    /**
     * Looks for a tile in this TileCache.
     *
     * @param producerId the id of the tile's %producer.
     * @param level the tile's quadtree level.
     * @param tx the tile's quadtree x coordinate.
     * @param ty the tile's quadtree y coordinate.
     * @param includeCache true to include both used and unused tiles in the
     *      search, false to include only the used tiles.
     * @return the requested tile, or NULL if it is not in this TileCache. This
     *      method does not change the number of users of the returned tile.
     */
    Tile* findTile(int producerId, int level, int tx, int ty, bool includeCache = false);

    /**
     * Returns the requested tile, creating it if necessary. If the tile is
     * currently in use it is returned directly. If it is in cache but unused,
     * it marked as used and returned. Otherwise a new tile is created, marked
     * as used and returned. In all cases the number of users of this tile is
     * incremented by one.
     *
     * @param producerId the id of the tile's %producer.
     * @param level the tile's quadtree level.
     * @param tx the tile's quadtree x coordinate.
     * @param ty the tile's quadtree y coordinate.
     * @param deadline the deadline at which the tile data must be ready. 0 means
     *      the current frame.
     * @param[out] the number of users of this tile, <i>before</i> it is
     *      incremented.
     * @return the requested tile, or NULL if there is no room left in the
     *      TileStorage to store the requested tile.
     */
    Tile* getTile(int producerId, int level, int tx, int ty, unsigned int deadline, int *users = NULL);

    /**
     * Returns a prefetch task to create the given tile. If the requested tile
     * is currently in use or in cache but unused, this method does nothing.
     * Otherwise it gets an unused tile storage (evicting an unused tile if
     * necessary), and then creates a task to produce the data of the requested
     * tile, in this storage. This method must not be called if #getScheduler
     * returns NULL.
     *
     * @param producerId the id of the tile's %producer.
     * @param level the tile's quadtree level.
     * @param tx the tile's quadtree x coordinate.
     * @param ty the tile's quadtree y coordinate.
     */
    ptr<Task> prefetchTile(int producerId, int level, int tx, int ty);

    /**
     * Decrements the number of users of this tile by one. If this number
     * becomes 0 the tile is marked as unused, and so can be evicted from the
     * cache at any moment.
     *
     * @param t a tile currently in use.
     * @return the number of users of this tile, <i>after</i> it has been
     *      decremented.
     */
    int putTile(Tile *t);

    /**
     * Invalidates the tiles from this cache produced by the given producer.
     * This means that the tasks to produce the actual data of these tiles will
     * be automatically reexecuted before the data can be used.
     *
     * @param producerId the id of a producer using this cache.
     *      See TileProducer#getId.
     */
    void invalidateTiles(int producerId);

    /**
     * Invalidates the selected tile from this cache produced by the given producer.
     * This means that the tasks to produce the actual data of this tile will
     * be automatically reexecuted before the data can be used.
     *
     * @param producerId the id of a producer using this cache.
     *      See TileProducer#getId.
     * @param level the level of a tile in the producer
     * @param tx the x coord of that tile
     * @param ty the y coord of that tile
     */
    void invalidateTile(int producerId, int level, int tx, int ty);

protected:
    /**
     * Creates a new uninitalized TileCache.
     */
    TileCache();

    /**
     * The name of this cache for debugging purpose.
     */
    std::string name;

    /**
     * Initializes this TileCache.
     *
     * @param storage the tile storage to store the actual tiles data.
     * @param name name of this cache, for logging.
     * @param scheduler an optional scheduler to schedule the creation of
     *      prefetched tiles. If no scheduler is specified, prefetch is
     *      disabled.
     */
    void init(ptr<TileStorage> storage, std::string name, ptr<Scheduler> scheduler = NULL);

    void swap(ptr<TileCache> c);

private:
    typedef map<Tile::TId, list<Tile*>::iterator> Cache;

    /**
     * Next local identifier to be used for a TileProducer using this cache.
     */
    int nextProducerId;

    /**
    * The producers that use this TileCache. Maps local %producer identifiers to
    * actual producers.
    */
    map<int, TileProducer*> producers;

    /**
     * The storage to store the tiles data.
     */
    ptr<TileStorage> storage;

    /**
     * The scheduler to schedule prefetched tiles creation tasks, and to
     * reschedule invalidated tiles creation tasks.
     */
    ptr<Scheduler> scheduler;

    /**
     * The tiles currently in use. These tiles cannot be evicted from the cache
     * and from the TileStorage, until they become unused. Maps tile identifiers
     * to actual tiles.
     */
    map<Tile::TId, Tile*> usedTiles;

    /**
     * The unused tiles. These tiles can be evicted from the cache at any
     * moment. Maps tile identifiers to positions in the ordered list of tiles
     * #unusedTilesOrder (used to implement a LRU cache).
     */
    Cache unusedTiles;

    /**
     * The unused tiles, ordered by date of last use (to implement a LRU cache).
     */
    list<Tile*> unusedTilesOrder;

    /**
     * The tasks to produce the data of deleted tiles. When an unused tile is
     * evicted from the cache it is destroyed, but its %producer task may not be
     * destroyed (if there remain some reference to it, for example via a task
     * graph). If the tile is needed again, #getTile will create a new Tile,
     * which could produce a new %producer task. Hence we could get two %producer
     * tasks for the same tile, which could lead to inconsistencies (the two
     * tasks may not have the same execution state, may not use the same storage
     * to store their result, etc). To avoid this problem we store the tasks of
     * deleted tiles in this map, in order to reuse them if a deleted tile is
     * needed again. When a %producer task gets deleted, it removes itself from
     * this map by calling #createTileTaskDeleted (because then it is not a
     * problem to recreate a new Task, there will be no duplication). So the size
     * of this map cannot grow unbounded.
     */
    map<Tile::TId, Task*> deletedTiles;

    /**
     * The number of queries to this tile cache. Only used for statistics.
     */
    int queries;

    /**
     * The number of missed queries to this tile cache. This is the number of
     * times a tile was requested but not found in the cache, requiring to
     * (re)create it. Only used for statistics.
     */
    int misses;

    /**
     * A mutex to serialize parallel accesses to this cache.
     */
    void* mutex;

    /**
     * Notifies this TileCache that a tile creation task has been deleted.
     */
    void createTileTaskDeleted(int producerId, int level, int tx, int ty);

    friend class TileProducer;

    friend class CreateTile;
};

}

#endif
