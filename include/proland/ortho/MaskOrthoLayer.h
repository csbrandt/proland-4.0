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

#ifndef _PROLAND_MASKORTHOLAYER_H_
#define _PROLAND_MASKORTHOLAYER_H_

#include "proland/graph/producer/GraphLayer.h"

namespace proland
{

/**
 * An OrthoGPUProducer layer to draw a Graph as a mask. This layer
 * can draw the %graph in a specific channel, and can combine it
 * with the previous content of this channel via blending equations.
 * @ingroup ortho
 * @author Antoine Begault, Guillaume Piolat
 */
PROLAND_API class MaskOrthoLayer : public GraphLayer
{
public:
    /**
     * Structure used to pass blend arguments to the framebuffer.
     */
    struct BlendParams
    {
        BlendParams()
        {
            enable = false;
            buffer = (BufferId) -1;
            rgb = ADD;
            srgb = ONE;
            drgb = ZERO;
            alpha = ADD;
            salpha = ONE;
            dalpha = ZERO;
        }

        BufferId buffer;

        bool enable;

        BlendEquation rgb;

        BlendArgument srgb;

        BlendArgument drgb;

        BlendEquation alpha;

        BlendArgument salpha;

        BlendArgument dalpha;
    };

    /**
     * Creates a new MaskOrthoLayer.
     *
     * @param graphs the GraphProducer that produces the graphs to
     *      be drawn by this layer.
     * @param ignored the curve type that the mask should ignore (not draw).
     * @param layerProgram the Program to be used to draw the graphs.
     * @param writeMask the channels into which the graphs must be drawn.
     *      Must be used as a bit value.
     *      0 = none, 1 = Red, 2 = Green, 4 = Blue, 8 = Alpha
     *      16 = DepthBuffer, 32 = FSTENCIL, 64 = BSTENCIL.
     * @param color drawn color.
     * @param depth drawn depth.
     * @param blend the blending equations to be used to combine this mask with
     *      the previous content of the channel #channel.
     * @param blendColor the color used in blend equations.
     * @param displayLevel the quadtree level at which the display of
     *      this layer must start.
     */
    MaskOrthoLayer(ptr<GraphProducer> graphs, set<int> ignored, ptr<Program> layerProgram, int writeMask, vec4f color, float depth, float widthFactor, BlendParams blendParams, vec4f blendColor, int displayLevel, bool deform = false);

    /**
     * Deletes this MaskOrthoLayer.
     */
    virtual ~MaskOrthoLayer();

    virtual void setTileSize(int tileSize, int tileBorder, float rootQuadSize);

    virtual bool doCreateTile(int level, int tx, int ty, TileStorage::Slot *data);

protected:
    /**
     * Creates an uninitialized MaskOrthoLayer.
     */
    MaskOrthoLayer();

    /**
     * Initializes thie MaskOrthoLayer. See #MaskOrthoLayer.
     */
    void init(ptr<GraphProducer> graphs, set<int> ignored, ptr<Program> layerProgram, int writeMask, vec4f color, float depth, float widthFactor, BlendParams blendParams, vec4f blendColor, int displayLevel, bool deform = false);

    void swap(ptr<MaskOrthoLayer> p);

private:
    /**
     * The curve type that the mask should ignore (not draw).
     */
    set<int> ignored;

    /**
     * The channels into which the graphs must be drawn.
     * Must be used as a bit value.
     * 0 = none, 1 = Red, 2 = Green, 4 = Blue, 8 = Alpha
     * 16 = DepthBuffer, 32 = FSTENCIL, 64 = BSTENCIL.
     */
    int writeMask;

    /**
     * Drawn color.
     */
    vec4f color;

    /**
     * Drawn depth.
     */
    float depth;

    float widthFactor;

    /**
     * The blending equations to be used to combine this mask with
     * the previous content of the channel #channel.
     */
    BlendParams blendParams;

    /**
     * The Color used in blend equations.
     */
    vec4f blendColor;

    /**
     * The mesh used to draw curves and areas.
     */
    ptr<Mesh< vec2f, unsigned int> > mesh;

    /**
     * The tesselator used to draw areas.
     */
    ptr<Tesselator> tess;

    ptr<Uniform3f> tileOffsetU;

    ptr<Uniform4f> colorU;

    ptr<Uniform1f> depthU;

};

}

#endif
