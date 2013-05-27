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

#ifndef _PROLAND_ELEVATION_PRODUCER_H_
#define _PROLAND_ELEVATION_PRODUCER_H_

#include "proland/producer/TileProducer.h"

namespace proland
{

/**
 * A TileProducer to create elevation tiles on CPU from CPU residual tiles.
 * @ingroup dem
 * @authors Antoine Begault, Eric Bruneton
 */
PROLAND_API class CPUElevationProducer : public TileProducer
{
public:
    /**
     * Creates a new CPUElevationProducer.
     *
     * @param cache the cache to store the produced tiles. The underlying
     *      storage must be a CPUTileStorage of float type.
     * @param residualTiles the %producer producing the residual tiles. This
     *      %producer should produce its tiles in a CPUTileStorage of float type.
     *      The size of the residual tiles (without borders) must be a multiple
     *      of the size of the elevation tiles (without borders).
     */
    CPUElevationProducer(ptr<TileCache> cache, ptr<TileProducer> residualTiles);

    /**
     * Deletes this CPUElevationProducer.
     */
    virtual ~CPUElevationProducer();

    virtual void getReferencedProducers(vector< ptr<TileProducer> > &producers) const;

    virtual void setRootQuadSize(float size);

    virtual int getBorder();

    virtual bool prefetchTile(int level, int tx, int ty);

    /**
     * Returns the %terrain altitude at a given point, at a given level.
     * The corresponding tile must be in cache before calling this method.
     *
     * @param producer a CPUElevationProducer or an equivalent (i.e. a %
     *      producer using an underlying CPUTileStorage of float type).
     * @param level level at which we want to get the altitude.
     * @param x physical x coordinate of the point to get (in meters from the %terrain center).
     * @param y physical y coordinate of the point to get (in meters from the %terrain center).
     */
    static float getHeight(ptr<TileProducer> producer, int level, float x, float y);

protected:
    /**
     * Creates an uninitialized CPUElevationProducer.
     */
    CPUElevationProducer();

    /**
     * Initializes this CPUElevationProducer.
     *
     * @param cache the cache to store the produced tiles. The underlying
     *      storage must be a CPUTileStorage of float type.
     * @param residualTiles the %producer producing the residual tiles. This
     *      %producer should produce its tiles in a CPUTileStorage of float type.
     *      The size of the residual tiles (without borders) must be a multiple
     *      of the size of the elevation tiles (without borders).
     */
    void init(ptr<TileCache> cache, ptr<TileProducer> residualTiles);

    virtual ptr<Task> startCreateTile(int level, int tx, int ty, unsigned int deadline, ptr<Task> task, ptr<TaskGraph> owner);

    virtual void beginCreateTile();

    virtual bool doCreateTile(int level, int tx, int ty, TileStorage::Slot *data);

    virtual void endCreateTile();

    virtual void stopCreateTile(int level, int tx, int ty);

    virtual void swap(ptr<CPUElevationProducer> p);

private:
    /**
     * The %producer producing the residual tiles. This %producer should produce
     * its tiles in a CPUTileStorage of float type. The size of the residual tiles
     * (without borders) must be a multiple of the size of the elevation tiles
     * (without borders).
     */
    ptr<TileProducer> residualTiles;
};

}

#endif
