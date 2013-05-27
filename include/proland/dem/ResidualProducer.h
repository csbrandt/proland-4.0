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

#ifndef _PROLAND_RESIDUAL_PRODUCER_H_
#define _PROLAND_RESIDUAL_PRODUCER_H_

#include <string>

#include "ork/resource/Resource.h"
#include "proland/producer/TileProducer.h"

using namespace ork;

namespace proland
{

/**
 * A TileProducer to load elevation residuals from disk to CPU memory.
 * See \ref sec-residual.
 * @ingroup dem
 * @authors Eric Bruneton, Antoine Begault
 */
PROLAND_API class ResidualProducer : public TileProducer
{
public:
    /**
     * Creates a new ResidualProducer.
     *
     * @param cache the cache to store the produced tiles. The underlying
     *      storage must be a CPUTileStorage of float type. The size
     *      of tiles in this storage size must be equal to the size of the
     *      tiles stored on disk, borders included.
     * @param name the name of the file containing the tiles to load.
     * @param deltaLevel the stored tiles level that must be considered as
     *      the root level in this %producer. Must be less than or equal to
     *      #getMinLevel().
     * @param zscale a vertical scaling factor to be applied to all elevations.
     */
    ResidualProducer(ptr<TileCache> cache, const char *name, int deltaLevel = 0, float zscale = 1.0);

    /**
     * Deletes this ResidualProducer.
     */
    virtual ~ResidualProducer();

    /**
     * Returns the size in pixels of the borders around each tile.
     */
    virtual int getBorder();

    /**
     * Adds a ResidualProducer to provide more details in some region.
     * Each producer manages a tile pyramid. A "subproducer" can provide
     * a "subpyramid" of tiles in some region, and so on recursively (see
     * \ref sec-residual).
     *
     * @param p a ResidualProducer providing more details in some region.
     */
    void addProducer(ptr<ResidualProducer> p);

    virtual bool hasTile(int level, int tx, int ty);

protected:
    /**
     * Creates an uninitialized ResidualProducer.
     */
    ResidualProducer();

    /**
     * Initializes this ResidualProducer.
     *
     * See #ResidualProducer.
     */
    void init(ptr<TileCache> cache, const char *name, int deltaLevel = 0, float zscale = 1.0);

    /**
     * Initializes this ResidualProducer from a Resource.
     *
     * @param manager the manager that will manage the created %resource.
     * @param r the %resource.
     * @param name the %resource name.
     * @param desc the %resource descriptor.
     * @param e an optional XML element providing contextual information (such
     *      as the XML element in which the %resource descriptor was found).
     */
    void init(ptr<ResourceManager> manager, Resource *r, const string &name, ptr<ResourceDescriptor> desc, const TiXmlElement *e = NULL);

    /**
     * Returns the stored tile level of the first tile of size #tileSize.
     * See \ref sec-residual.
     */
    int getMinLevel();

    /**
     * Returns the stored tiles level that must be considered as the root level
     * in this %producer. Must be less than or equal to #getMinLevel().
     */
    int getDeltaLevel();

    virtual bool doCreateTile(int level, int tx, int ty, TileStorage::Slot *data);

    virtual void swap(ptr<ResidualProducer> p);

    /**
     * Upsamples the elevations of a parent tile into a sub tile. This method is
     * used to compute the root tile from the first residual tiles when
     * #getDeltaLevel is not 0.
     *
     * @param level the level of the elevation tile to produce.
     * @param tx the logical x coordinate of the tile to produce.
     * @param ty the logical y coordinate of the tile to produce.
     * @param parentTile the elevations of the parent tile (level-1,tx/2,ty/2).
     * @param result the upsampled elevations for the sub tile (level,tx,ty).
     */
    void upsample(int level, int tx, int ty, float *parentTile, float *result);

private:
    /**
     * The name of the file containing the residual tiles to load.
     */
    string name;

    /**
     * The size of the tiles whose level (on disk) is at least #minLevel.
     * This size does not include the borders. A tile contains
     * (tileSize+5)*(tileSize+5) samples.
     */
    int tileSize;

    /**
     * The level of the root of the tile pyramid managed by this %producer
     * in the global set of tile pyramids describing a %terrain. Can be
     * different from 0 when multiple pyramids are used, to provide more
     * details in some regions.
     */
    int rootLevel;

    /**
     * The stored tiles level that must be considered as the root level in
     * this %producer. Must be less than or equal to #getMinLevel().
     */
    int deltaLevel;

    /**
     * The logical x coordinate of the root of the tile pyramid managed
     * by this %producer in the global set of tile pyramids describing a
     * %terrain. Can be different from 0 when multiple pyramids are used,
     * to provide more details in some regions.
     */
    int rootTx;

    /**
     * The logical y coordinate of the root of the tile pyramid managed
     * by this %producer in the global set of tile pyramids describing a
     * %terrain. Can be different from 0 when multiple pyramids are used,
     * to provide more details in some regions.
     */
    int rootTy;

    /**
     * The stored tile level of the first tile of size #tileSize.
     * See \ref sec-residual.
     */
    int minLevel;

    /**
     * The maximum level of the stored tiles on disk (inclusive, and
     * relatively to #rootLevel).
     */
    int maxLevel;

    /**
     * A scaling factor to be applied to all residuals read from disk.
     */
    float scale;

    /**
     * Offset of the first stored tile on disk. The offsets indicated in
     * the tile offsets array #offsets are relative to this offset.
     */
    unsigned int header;

    /**
     * The offsets of each tile on disk, relatively to #offset, for each
     * tile id (see #getTileId).
     */
    unsigned int* offsets;

    /**
     * A mutex used to serializes accesses to the file storing the tiles.
     */
    void *mutex;

    /**
     * The file storing the residual tiles on disk.
     */
    FILE *tileFile;

    /**
     * The "subproducers" providing more details in some regions.
     * Each subproducer can have its own subproducers, recursively.
     */
    vector< ptr<ResidualProducer> > producers;

    /**
     * A key to store thread specific buffers used to produce the tiles.
     */
    static void *key;

    /**
     * Returns the size of tiles of the given level (without borders).
     * This size can vary with the level if #getMinLevel is not 0 (see
     * \ref sec-residual).
     *
     * @param level a quadtree level.
     */
    int getTileSize(int level);

    /**
     * Returns the id of the given tile. This id is used to find the offset
     * the tile data on disk, using #offsets.
     *
     * @param level the level of the tile.
     * @param tx the logical x coordinate of the tile.
     * @param ty the logical y coordinate of the tile.
     * @return the id of the given tile.
     */
    int getTileId(int level, int tx, int ty);

    /**
     * Reads compressed tile data on disk, uncompress it and scale it with
     * #scale.
     *
     * @param level the level of the tile.
     * @param tx the logical x coordinate of the tile.
     * @param ty the logical y coordinate of the tile.
     * @param compressedData where the compressed tile data must be stored.
     * @param uncompressedData where the uncompressed data must be stored.
     * @param tile an optional tile to be added to the result. Maybe NULL.
     * @param result where the uncompressed data, scaled by #scale and
     *      optionally offseted by 'tile', must be stored.
     */
    void readTile(int level, int tx, int ty,
            unsigned char* compressedData, unsigned char *uncompressedData,
            float *tile, float *result);
};

}

#endif
