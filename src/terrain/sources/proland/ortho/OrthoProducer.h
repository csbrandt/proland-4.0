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

#ifndef _PROLAND_ORTHO_PRODUCER_H_
#define _PROLAND_ORTHO_PRODUCER_H_

#include "ork/render/Program.h"
#include "ork/render/Texture2DArray.h"
#include "ork/render/RenderBuffer.h"
#include "ork/resource/ResourceManager.h"
#include "proland/producer/TileProducer.h"

using namespace ork;

namespace proland
{

/**
 * A TileProducer to create texture tiles on GPU from CPU residual tiles.
 * See \ref sec-ortho.
 * @ingroup ortho
 * @authors Eric Bruneton, Antoine Begault
 */
PROLAND_API class OrthoProducer : public TileProducer
{
public:
    /**
     * Creates a new OrthoProducer.
     *
     * @param cache the cache to store the produced tiles. The underlying
     *      storage must be a GPUTileStorage.
     * @param residualTiles the %producer producing the residual tiles. This
     *      %producer should produce its tiles in a CPUTileStorage of
     *      unsigned ybte type. The size of the residual tiles (without
     *      borders) must be equal to the size of the produced tiles (without
     *      borders). The border sizes must be the same.
     * @param orthoTexture a texture used to produce the tiles. Its size must
     *      be equal to the produced tile size (including borders).
     * @param residualTexture a texture used to produce the tiles. Its size
     *      must be equal to the produced tile size (including borders).
     * @param upsample the Program to perform the upsampling and add
     *      procedure on GPU. See \ref sec-ortho.
     * @param scale scaling factor used for residual values.
     * @param maxLevel maximum quadtree level, or -1 to allow any level.
     */
    OrthoProducer(ptr<TileCache> cache, ptr<TileProducer> residualTiles,
        ptr<Texture2D> orthoTexture, ptr<Texture2D> residualTexture,
        ptr<Program> upsample, vec4f rootNoiseColor, vec4f noiseColor,
        std::vector<float> &noiseAmp, bool noiseHsv,
        float scale, int maxLevel);

    /**
     * Deletes this OrthoProducer.
     */
    virtual ~OrthoProducer();

    virtual void getReferencedProducers(std::vector< ptr<TileProducer> > &producers) const;

    virtual void setRootQuadSize(float size);

    virtual int getBorder();

    virtual bool hasTile(int level, int tx, int ty);

    virtual bool prefetchTile(int level, int tx, int ty);

protected:
    ptr<FrameBuffer> frameBuffer;

    /**
     * The %producer producing the residual tiles. This %producer should produce its
     * tiles in a CPUTileStorage of unsigned byte type. The size of the residual
     * tiles (without borders) must be equal to the size of the produced tiles
     * (without borders).
     */
    ptr<TileProducer> residualTiles;

    /**
     * A texture used to produce the tiles. Its size must be equal to the produced
     * tile size (including borders).
     */
    ptr<Texture2D> orthoTexture;

    /**
     * A texture used to produce the tiles. Its size must be equal to the produced
     * tile size (including borders).
     */
    ptr<Texture2D> residualTexture;

    /**
     * Cube face ID for producers targeting spherical terrains.
     */
    int face;

    /**
     * The Program to perform the upsampling and add procedure on GPU.
     * See \ref sec-ortho.
     */
    ptr<Program> upsample;

    /**
     * Creates an uninitialized OrthoProducer.
     */
    OrthoProducer();

    /**
     * Initializes this OrthoProducer. See #OrthoProducer.
     */
    void init(ptr<TileCache> cache, ptr<TileProducer> residualTiles,
        ptr<Texture2D> orthoTexture, ptr<Texture2D> residualTexture,
        ptr<Program> upsample, vec4f rootNoiseColor, vec4f noiseColor,
        std::vector<float> &noiseAmp, bool noiseHsv,
        float scale, int maxLevel);

    /**
     * Initializes this OrthoProducer from a Resource.
     *
     * @param manager the manager that will manage the created %resource.
     * @param r the %resource.
     * @param name the %resource name.
     * @param desc the %resource descriptor.
     * @param e an optional XML element providing contextual information (such
     *      as the XML element in which the %resource descriptor was found).
     */
    void init(ptr<ResourceManager> manager, Resource *r, const std::string &name, ptr<ResourceDescriptor> desc, const TiXmlElement *e = NULL);

    virtual void *getContext() const;

    virtual ptr<Task> startCreateTile(int level, int tx, int ty, unsigned int deadline, ptr<Task> task, ptr<TaskGraph> owner);

    virtual void beginCreateTile();

    virtual bool doCreateTile(int level, int tx, int ty, TileStorage::Slot *data);

    virtual void endCreateTile();

    virtual void stopCreateTile(int level, int tx, int ty);

    virtual void swap(ptr<OrthoProducer> p);

private:
    /**
     * The number of components per pixel in the CPU residual tiles.
     */
    int channels;

    /**
     * Maximum quadtree level, or -1 to allow any level.
     */
    int maxLevel;

    ptr<Texture2DArray> noiseTexture;

    vec4f rootNoiseColor;

    vec4f noiseColor;

    std::vector<float> noiseAmp;

    bool noiseHsv;

    /**
     * Scaling factor used for residual values.
     */
    float scale;

    ptr<Uniform1f> tileWidthU;

    ptr<UniformSampler> coarseLevelSamplerU;

    ptr<Uniform4f> coarseLevelOSLU;

    ptr<UniformSampler> residualSamplerU;

    ptr<Uniform4f> residualOSHU;

    ptr<UniformSampler> noiseSamplerU;

    ptr<Uniform4i> noiseUVLHU;

    ptr<Uniform4f> noiseColorU;

    ptr<Uniform4f> rootNoiseColorU;

    static static_ptr<FrameBuffer> old;
};

}

#endif
