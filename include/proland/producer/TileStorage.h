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

#ifndef _PROLAND_TILE_STORAGE_H_
#define _PROLAND_TILE_STORAGE_H_

#include <list>

#include "ork/core/Object.h"

using namespace ork;

namespace proland
{

/**
 * @defgroup proland proland
 * Provides a framework for Procedural Landscapes rendering.
 */

/**
 * @defgroup producer producer
 * Provides a framework to produce, store and cache tile-based data. Tile-based
 * data means data associated with a quadtree, i.e., data represented at several
 * levels of details. The root of the quadtree has level 0, its four children
 * have level 1, and so on recursively. At each level i there are 2^i * 2^i
 * tiles. Hence tiles can be identified with 3 coordinates (level, tx, ty),
 * where tx and ty vary between 0 and 2^i - 1.
 * @ingroup proland
 */

/**
 * A shared storage to store tiles of the same kind. This abstract class defines
 * the behavior of tile storages but does not provide any storage itself. The
 * slots managed by a tile storage can be used to store any tile identified by
 * its (level,tx,ty) coordinates. This means that a TileStorage::Slot can store
 * the data of some tile at some moment, and then be reused to store the data of
 * tile some time later. The mapping between tiles and TileStorage::Slot is not
 * managed by the TileStorage itself, but by a TileCache. A TileStorage just
 * keeps track of which slots in the pool are currently associated with a
 * tile (i.e., store the data of a tile), and which are not. The first ones are
 * called allocated slots, the others free slots.
 * @ingroup producer
 * @authors Eric Bruneton, Antoine Begault
 */
PROLAND_API class TileStorage : public Object
{
public:
    /**
     * A slot managed by a TileStorage. Concrete sub classes of this class must
     * provide a reference to the actual tile data.
     */
    class Slot
    {
    public:
        /**
         * The id of the tile currently stored in this slot.
         */
        pair<int, pair<int, pair<int, int> > > id;

        /**
         * The task that is responsible for producing the data for the tile
         * stored in this slot.
         */
        void* producerTask;

        /**
         * Creates a new TileStorage::Slot.
         *
         * @param owner the TileStorage that will manage this slot.
         */
        Slot(TileStorage *owner);

        /**
         * Deletes this TileStorage::Slot. This destroys the data of the tile
         * stored in this slot, if any.
         */
        virtual ~Slot();

        /**
         * Returns the TileStorage that manages this slot.
         */
        TileStorage *getOwner();

        /**
         * Locks or unlocks this slots. Slots can be accessed by several threads
         * simultaneously. This lock can be used to serialize these accesses.
         * In particular it is used to change the #producerTask, when a slot is
         * reused to store new data.
         *
         * @param lock true to lock the slot, false to unlock it.
         */
        void lock(bool lock);

    private:
        /**
         * The TileStorage that manages this slot.
         */
        TileStorage *owner;

        /**
         * A mutex used to serialize parallel accesses to this slot.
         */
        void* mutex;
    };

    /**
     * Creates a new TileStorage.
     *
     * @param tileSize the size of each tile. For tiles made of raster data,
     *      this size is the tile width in pixels (the tile height is supposed
     *      equal to the tile width).
     * @param capacity the number of slots allocated and managed by this tile
     *      storage. This capacity is fixed and cannot change with time.
     */
    TileStorage(int tileSize, int capacity);

    /**
     * Deletes this TileStorage. This deletes the data associated with all the
     * slots managed by this tile storage.
     */
    virtual ~TileStorage();

    /**
     * Returns a free slot in the pool of slots managed by this TileStorage.
     *
     * @return a free slot, or NULL if all tiles are currently allocated. The
     *      returned slot is then considered to be allocated, until it is
     *      released with deleteSlot.
     */
    Slot *newSlot();

    /**
     * Notifies this storage that the given slot is free. The given slot can
     * then be allocated to store a new tile, i.e., it can be returned by a
     * subsequent call to newSlot.
     *
     * @param t a slot that is no longer in use.
     */
    void deleteSlot(Slot *t);

    /**
     * Returns the size of each tile. For tiles made of raster data, this size
     * is the tile width in pixels (the tile height is supposed equal to the
     * tile width).
     */
    int getTileSize();

    /**
     * Returns the total number of slots managed by this TileStorage. This
     * includes both unused and used tiles.
     */
    int getCapacity();

    /**
     * Returns the number of slots in this TileStorage that are currently unused.
     */
    int getFreeSlots();

protected:
    /**
     * The size of each tile. For tiles made of raster data, this size is the
     * tile width in pixels (the tile height is supposed equal to the tile
     * width).
     */
    int tileSize;

    /**
     * The total number of slots managed by this TileStorage. This includes both
     * unused and used tiles.
     */
    int capacity;

    /**
     * The currently free slots.
     */
    list<Slot*> freeSlots;

    /**
     * Creates a new uninitialized TileStorage.
     */
    TileStorage();

    /**
     * Initializes this TileStorage.
     *
     * @param tileSize the size of each tile. For tiles made of raster data,
     *      this size is the tile width in pixels (the tile height is supposed
     *      equal to the tile width).
     * @param capacity the number of slots allocated and managed by this tile
     *      storage. This capacity is fixed and cannot change with time.
     */
    void init(int tileSize, int capacity);
};

}

#endif
