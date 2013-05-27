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

#ifndef _PROLAND_WATERORTHOLAYER_H_
#define _PROLAND_WATERORTHOLAYER_H_

#include "proland/graph/producer/GraphLayer.h"

namespace proland
{

/**
 * An OrthoGPUProducer layer to draw static %rivers and lakes.
 * @ingroup ortho
 * @author Antoine Begault, Guillaume Piolat
 */
PROLAND_API class WaterOrthoLayer : public GraphLayer
{
public:
    /**
     * Predefined type for rivers.
     * Used for drawing and managing rivers.
     */
    enum riverType
    {
        BORDER = -2, //!< River Banks. Curves with this type have as ancestor the curve representing the center of the river they belong to.
        OBSTACLE = -1, //!< Floating obstacle or islands.
        RIVER = 0, //!< Basic river.
        ISLAND = 1, //!< Represents islands when area1 != NULL.
        LAKE = 2 //!< Lake.
    };

    /**
     * Creates a new WaterOrthoLayer.
     *
     * @param graphProducer the GraphProducer that produces the graphs to
     *      be drawn by this layer.
     * @param layerProgram the Program to be used to draw the graphs.
     * @param displayLevel the quadtree level at which the display of
     *      this layer must start.
     * @param quality enable or not the quality mode (better display).
     * @param color the color of water (default 0:0:0).
     * @param deform true if the produced tiles will be mapped on a spherical
     *      %terrain.
     */
    WaterOrthoLayer(ptr<GraphProducer> graphProducer, ptr<Program> layerProgram,
        int displayLevel = 0, bool quality = true, vec4f color = vec4f(0, 0, 0, 0),
        bool deform = false);

    /**
     * Deletes this WaterOrthoLayer.
     */
    virtual ~WaterOrthoLayer();

    virtual void setTileSize(int tileSize, int tileBorder, float rootQuadSize);

    virtual bool doCreateTile(int level, int tx, int ty, TileStorage::Slot *data);

protected:
    /**
     * Creates an uninitialized WaterOrthoLayer.
     */
    WaterOrthoLayer();

    /**
     * Initializes this WaterOrthoLayer. See #WaterOrthoLayer.
     */
    void init(ptr<GraphProducer> graphProducer, ptr<Program> layerProgram,
        int displayLevel = 0, bool quality = true, vec4f color = vec4f(0, 0, 0, 0),
        bool deform = true);

    virtual void swap(ptr<WaterOrthoLayer> p);

private:
    /**
     * Water color.
     */
    vec4f color;

    /**
     * The mesh used for drawing curves.
     */
    ptr< Mesh<vec2f, unsigned int> > mesh;

    /**
     * The tesselator used for drawing areas.
     */
    ptr<Tesselator> tess;

    ptr<Uniform3f> tileOffsetU;

    ptr<Uniform4f> colorU;
};

}

#endif
