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

#ifndef _PROLAND_ORTHO_GPU_PRODUCER_H_
#define _PROLAND_ORTHO_GPU_PRODUCER_H_

#include "ork/render/FrameBuffer.h"
#include "ork/render/Program.h"
#include "ork/render/Texture2D.h"
#include "proland/producer/TileProducer.h"

using namespace ork;

namespace proland
{

/**
 * A TileProducer to create texture tiles on GPU from CPU textures tiles.
 * This %producer simply copies the CPU texture data into GPU textures.
 * This %producer accepts layers, which can modify the raw CPU texture
 * data, using shaders on GPU. If layers are used, this %producer can
 * produce tiles whose level is greater than the maximum level of the CPU
 * tile %producer, using an intermediate tile cache to store the CPU tiles
 * to be enlarged to produce zoomed versions. In fact, if layers are used,
 * the CPU tile %producer is not mandatory: in this case tiles are produced
 * enterily on GPU, using the layers alone.
 * @ingroup ortho
 * @authors Eric Bruneton, Antoine Begault, Guillaume Piolat
 */
PROLAND_API class OrthoGPUProducer : public TileProducer
{
public:
    /**
     * Creates a new OrthoGPUProducer.
     *
     * @param cache the cache to store the produced tiles. The underlying
     *      storage must be a GPUTileStorage. The size of tiles in this
     *      storage size must be equal to the size of the tiles produced
     *      by orthoTiles.
     * @param backgroundCache an intermediate cache to store CPU tiles to
     *      be enlarged to produce zoomed in versions. Only necessary if
     *      layers are used, and if you want to produce tiles of level
     *      greater than the maximum level of orthoTiles.
     * @param orthoTiles the %producer producing the CPU tiles. This
     *      %producer should produce its tiles in a CPUTileStorage of
     *      unsigned byte type. Maybe NULL if layers are used (in this case
     *      tiles are produced enterily on GPU using the layers).
     * @param compressedTexture a texture used to copy the orthoTiles tiles
     *      to GPU, if they are stored in DXT format on CPU. Its size must
     *      be equal to the produced tile size (including borders), and its
     *      format equal to the DXT format used by orthoTiles. Maybe NULL if
     *      orthoTiles are not compressed.
     * @param uncompressedTexture a texture used to uncompress tiles on GPU,
     *      or if orthoTiles are not compressed, to copy them directly to GPU.
     *      Its size must be equal to the produced tile size (including borders).
     */
    OrthoGPUProducer(ptr<TileCache> cache, ptr<TileCache> backgroundCache, ptr<TileProducer> orthoTiles,
        int maxLevel = -1, ptr<Texture2D> compressedTexture = NULL, ptr<Texture2D> uncompressedTexture = NULL);

    /**
     * Deletes this OrthoGPUProducer.
     */
    virtual ~OrthoGPUProducer();

    virtual void getReferencedProducers(std::vector< ptr<TileProducer> > &producers) const;

    virtual void setRootQuadSize(float size);

    virtual int getBorder();

    virtual bool hasTile(int level, int tx, int ty);

    virtual bool prefetchTile(int level, int tx, int ty);

protected:
    /**
     * Creates an uninitialized OrthoGPUProducer.
     */
    OrthoGPUProducer();

    /**
     * Initializes this OrthoGPUProducer. See #OrthoGPUProducer.
     */
    void init(ptr<TileCache> cache, ptr<TileCache> backgroundCache, ptr<TileProducer> orthoTiles,
        int maxLevel = -1, ptr<Texture2D> compressedTexture = NULL, ptr<Texture2D> uncompressedTexture = NULL);

    virtual void *getContext() const;

    virtual ptr<Task> startCreateTile(int level, int tx, int ty, unsigned int deadline, ptr<Task> task, ptr<TaskGraph> owner);

    virtual void beginCreateTile();

    virtual bool doCreateTile(int level, int tx, int ty, TileStorage::Slot *data);

    virtual void endCreateTile();

    virtual void stopCreateTile(int level, int tx, int ty);

    virtual void swap(ptr<OrthoGPUProducer> p);

private:
    ptr<FrameBuffer> frameBuffer;

    /**
     * The %producer producing the CPU tiles. This %producer should produce
     * its tiles in a CPUTileStorage of unsigned byte type. Maybe NULL if
     * layers are used (in this case tiles are produced enterily on GPU
     * using the layers).
     */
    ptr<TileProducer> orthoTiles;

    /**
     * An intermediate OrthoGPUProducer to produce the CPU tiles to
     * be enlarged to produce zoomed in versions. Only necessary if
     * layers are used, and if you want to produce tiles of level
     * greater than the maximum level of orthoTiles.
     */
    ptr<TileProducer> coarseGpuTiles;

    /**
     * The number of components per pixel of the CPU tiles.
     */
    int channels;

    /**
     * The size of the produced tiles, including borders.
     */
    int tileSize;

    int maxLevel;

    /**
     * A texture used to copy the #orthoTiles tiles to GPU, if they are
     * stored in DXT format on CPU.
     */
    ptr<Texture2D> compressedTexture;

    /**
     * A texture used to uncompress tiles on GPU, or if #orthoTiles are
     * not compressed, to copy them directly to GPU.
     */
    ptr<Texture2D> uncompressedTexture;

    ptr<UniformSampler> uncompressSourceU;

    ptr<UniformSampler> upsampleSourceU;

    ptr<Uniform4f> tileU;

    static static_ptr<Program> uncompress;

    static static_ptr<Program> upsample;

    static static_ptr<FrameBuffer> old;
};

}

#endif
