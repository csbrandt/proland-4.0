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

#ifndef _PROLAND_NORMAL_PRODUCER_H_
#define _PROLAND_NORMAL_PRODUCER_H_

#include "ork/render/FrameBuffer.h"
#include "ork/render/Program.h"
#include "ork/render/Texture2D.h"
#include "proland/producer/TileProducer.h"

using namespace ork;

namespace proland
{

/**
 * A TileProducer to generate %terrain normals from %terrain elevations on GPU.
 * @ingroup dem
 * @authors Eric Bruneton, Antoine Begault
 */
PROLAND_API class NormalProducer : public TileProducer
{
public:
    /**
     * Creates a new NormalProducer.
     *
     * @param cache the cache to store the produced tiles. The underlying
     *      storage must be a GPUTileStorage with textures of two or four
     *      components per pixel. If two components are used only one normal
     *      is stored per pixel (n.x and n.y are stored, n.z is implicit). If
     *      four components are used a coarse normal is also stored, to provide
     *      smooth interpolation of normals between quadtree levels. If floating
     *      point textures are used normals components vary between -1 and 1. If
     *      non floating point textures are used, this range is scaled to 0-1 to
     *      fit in the texture format.
     * @param elevationTiles the %producer producing the elevation tiles on GPU.
     *      The underlying storage must be a GPUTileStorage with floating point
     *      textures with at least 3 components per pixel (4 if 'deform' is true).
     *      The elevation tile size, without borders, must be equal to the normal
     *      tile size, minus 1.
     * @param normalTexture a texture used to produce the tiles. Its size must be
     *      equal to the normal tile size. Its format must be the same as the
     *      format used for the tile storage of this %producer.
     * @param normals the Program to compute normals from elevations on GPU.
     * @param gridMeshSize the size of the grid that will be used to render each tile.
     *      Must be the tile size (minus 1) divided by a power of two.
     * @param deform true if the produced normals will be mapped on a spherical
     *      %terrain.
     */
    NormalProducer(ptr<TileCache> cache, ptr<TileProducer> elevationTiles,
        ptr<Texture2D> normalTexture, ptr<Program> normals, int gridMeshSize, bool deform = false);

    /**
     * Deletes this NormalProducer.
     */
    virtual ~NormalProducer();

    virtual void getReferencedProducers(std::vector< ptr<TileProducer> > &producers) const;

    virtual void setRootQuadSize(float size);

    virtual int getBorder();

    virtual bool hasTile(int level, int tx, int ty);

protected:
    /**
     * The Program to compute normals from elevations on GPU.
     */
    ptr<Program> normals;

    /**
     * Creates an uninitialized NormalProducer.
     */
    NormalProducer();

    /**
     * Initializes this NormalProducer. See #NormalProducer.
     */
    void init(ptr<TileCache> cache, ptr<TileProducer> elevationTiles,
        ptr<Texture2D> normalTexture, ptr<Program> normals, int gridMeshSize, bool deform = false);

    virtual void *getContext() const;

    virtual ptr<Task> startCreateTile(int level, int tx, int ty, unsigned int deadline, ptr<Task> task, ptr<TaskGraph> owner);

    virtual void beginCreateTile();

    virtual bool doCreateTile(int level, int tx, int ty, TileStorage::Slot *data);

    virtual void endCreateTile();

    virtual void stopCreateTile(int level, int tx, int ty);

    virtual void swap(ptr<NormalProducer> p);

private:
    ptr<FrameBuffer> frameBuffer;

    /**
     * The %producer producing the elevation tiles on GPU. The underlying storage
     * must be a GPUTileStorage with floating point textures with at least 3
     * components per pixel (4 if 'deform' is true). The elevation tile size,
     * without borders, must be equal to the normal tile size, minus 1.
     */
    ptr<TileProducer> elevationTiles;

    /**
     * A texture used to produce the tiles. Its size must be  equal to the normal
     * tile size. Its format must be the same as the format used for the tile
     * storage of this %producer.
     */
    ptr<Texture2D> normalTexture;

    /**
     * true if the produced elevations will be mapped on a spherical %terrain.
     */
    bool deform;

    /**
     * The size of the grid that will be used to render each tile.
     * Must be the tile size (without borders) divided by a power of two.
     */
    int gridMeshSize;

    ptr<Uniform3f> tileSDFU;

    ptr<UniformSampler> elevationSamplerU;

    ptr<Uniform4f> elevationOSLU;

    ptr<UniformSampler> normalSamplerU;

    ptr<Uniform4f> normalOSLU;

    ptr<UniformMatrix4f> patchCornersU;

    ptr<UniformMatrix4f> patchVerticalsU;

    ptr<Uniform4f> patchCornerNormsU;

    ptr<UniformMatrix3f> worldToTangentFrameU;

    ptr<UniformMatrix3f> parentToTangentFrameU;

    ptr<Uniform4f> deformU;

    static static_ptr<FrameBuffer> old;
};

}

#endif
