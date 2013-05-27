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

#ifndef _PROLAND_TextureLayer_H_
#define _PROLAND_TextureLayer_H_

#include "ork/math/vec4.h"
#include "ork/render/Program.h"
#include "proland/producer/TileLayer.h"
#include "proland/producer/TileProducer.h"
#include "proland/ortho/OrthoGPUProducer.h"

using namespace ork;

namespace proland
{

/**
 * An OrthoGPUProducer layer to blend the tiles of two OrthoGPUProducer.
 * This layer modifies the tiles of its OrthoGPUProducer %producer by
 * blending into them the tiles produced by another OrthoGPUProducer,
 * after they have been transformed via a GPU Program.
 * @ingroup ortho
 * @authors Eric Bruneton, Antoine Begault, Guillaume Piolat
 */
PROLAND_API class TextureLayer : public TileLayer
{
public:
    /**
     * Structure used to pass blend arguments to the framebuffer.
     */
    struct BlendParams
    {
        BlendParams()
        {
            buffer = (BufferId) -1;
            rgb = ADD;
            srgb = ONE;
            drgb = ZERO;
            alpha = ADD;
            salpha = ONE;
            dalpha = ZERO;
        }

        BufferId buffer;

        BlendEquation rgb;

        BlendArgument srgb;

        BlendArgument drgb;

        BlendEquation alpha;

        BlendArgument salpha;

        BlendArgument dalpha;
    };

    /**
     * Creates a new TextureLayer.
     *
     * @param tiles the %producer producing the tiles that must be blended
     *      into the tiles produced by the %producer to which this layer
     *      belongs. The 'tiles' %producer must be a GPU producer, but the
     *      size and format of its tiles are arbitrary. Its maximum level can
     *      also be less than the maximum level of the %producer to which this
     *      layer belongs (in this cas an automatic zoom is used to produce
     *      finer tiles).
     * @param program the Program to be used to transform the tiles produced
     *      by 'tiles' before bending them into the tiles produced by the
     *      %producer to which this layer belongs.
     * @param tilesSamplerName name of the samplerTile uniform in 'program'
     *      that is used to access the tiles produced by 'tiles'.
     * @param blend the blend equations and functions to be used to blend
            the tiles together.
     * @param minDisplayLevel the quadtree level at which the display of
     *      this layer must start.
     */
    TextureLayer(ptr<TileProducer> tiles, ptr<Program> program, std::string tilesSamplerName,
        BlendParams blend, int minDisplaylevel, bool storeTiles = false);

    /**
     * Deletes this TextureLayer.
     */
    virtual ~TextureLayer();

    virtual void getReferencedProducers(vector< ptr<TileProducer> > &producers) const;

    void useTile(int level, int tx, int ty, unsigned int deadline);

    void unuseTile(int level, int tx, int ty);

    virtual bool doCreateTile(int level, int tx, int ty, TileStorage::Slot *data);

    virtual void startCreateTile(int level, int tx, int ty, unsigned int deadline, ptr<Task> task, ptr<TaskGraph> result);

    virtual void stopCreateTile(int level, int tx, int ty);

protected:
    /**
     * The Program to be used to transform the tiles produced by #tiles before
     * bending them into the tiles produced by the %producer to which this
     * layer belongs.
     */
    ptr<Program> program;

    /**
     * Creates an uninitialized TextureLayer.
     */
    TextureLayer();

    /**
     * Initializes this TextureLayer. See #TextureLayer.
     */
    void init(ptr<TileProducer> tiles, ptr<Program> program, std::string tilesSamplerName,
        BlendParams blend, int minDisplaylevel, bool storeTiles = false);

    virtual void swap(ptr<TextureLayer> p);

private:
    /**
     * The %producer producing the tiles that must be blended into the tiles
     * produced by the %producer to which this layer belongs. Must be a GPU
     * producer, but the size and format of its tiles are arbitrary.
     */
    ptr<TileProducer> tiles;

    /**
     * Name of the samplerTile uniform in #program that is used to access the
     * tiles produced by #tiles.
     */
    std::string tilesSamplerName;

    /**
     * The blend equations and functions to be used to blend the tiles together.
     */
    BlendParams blend;

    bool storeTiles;

    /**
     * The quadtree level at which the display of this layer must start.
     */
    int minDisplayLevel;

    ptr<UniformSampler> samplerU;

    ptr<Uniform3f> coordsU;

    ptr<Uniform3f> sizeU;
};

}

#endif
