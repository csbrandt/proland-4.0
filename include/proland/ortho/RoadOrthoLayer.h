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

#ifndef _PROLAND_ROADORTHOLAYER_H_
#define _PROLAND_ROADORTHOLAYER_H_

#include "proland/graph/producer/GraphLayer.h"
#include "proland/graph/producer/CurveDataFactory.h"

namespace proland
{

/**
 * An OrthoGPUProducer layer to draw roads.
 * @ingroup ortho
 * @author Antoine Begault, Guillaume Piolat
 */
PROLAND_API class RoadOrthoLayer : public GraphLayer, public CurveDataFactory
{
public:
    /**
     * Predefined type for roads.
     * Used for drawing and managing roads.
     */
    enum roadType
    {
        BASIC = 0,
        ROAD = 0, //!< Basic road.
        UNKNOWN = 1, //!< Undefined.
        BRIDGE = 2 //!< Bridge linking 2 roads, passing on top of another.
    };

    /**
     * Creates a new RoadOrthoLayer.
     *
     * @param graphProducer the GraphProducer that produces the graphs to
     *      be drawn by this layer.
     * @param layerProgram the Program to be used to draw the graphs.
     * @param displayLevel the quadtree level at which the display of
     *      this layer must start.
     * @param quality enable or not the quality mode (better display).
     * @param color roads color (default 0:0:0).
     * @param dirt small dirt paths color.
     * @param border roads border color.
     * @param border_width roads border width.
     * @param inner_border_width roads inner border width.
     * @param deform true if the produced tiles will be mapped on a spherical
     *      %terrain.
     */
    RoadOrthoLayer(ptr<GraphProducer> graphProducer, ptr<Program> layerProgram,
        int displayLevel = 0, bool quality = true, vec4f color = vec4f(0, 0, 0, 0),
        vec4f dirt = vec4f(0, 0, 0, 0), vec4f border = vec4f(1, 1, 1, 0),
        float border_width = 1.2f, float inner_border_width = 2.0f,
        bool deform = false);

    /**
     * Deletes this RoadOrthoLayer.
     */
    virtual ~RoadOrthoLayer();

    virtual void setTileSize(int tileSize, int tileBorder, float rootQuadSize);

    virtual void startCreateTile(int level, int tx, int ty, unsigned int deadline, ptr<Task> task, ptr<TaskGraph> result);

    virtual bool doCreateTile(int level, int tx, int ty, TileStorage::Slot *data);

    virtual void stopCreateTile(int level, int tx, int ty);

protected:
    /**
     * Creates an uninitialized RoadOrthoLayer.
     */
    RoadOrthoLayer();

    /**
     * Initializes this RoadOrthoLayer. See #RoadOrthoLayer.
     */
    void init(ptr<GraphProducer> graphProducer, ptr<Program> layerProgram,
        int displayLevel = 0, bool quality = true, vec4f color = vec4f(0, 0, 0, 0),
        vec4f dist = vec4f(0, 0, 0, 0), vec4f border = vec4f(1, 1, 1, 0),
        float border_width = 1.2f, float inner_border_width = 2.0f,
        bool deform = false);

    virtual void swap(ptr<RoadOrthoLayer> p);

private:
    /**
     * Roads border width.
     */
    float border_width;

    /**
     * Roads inner border width.
     */
    float inner_border_width;

    /**
     * Roads color (default 0:0:0).
     */
    vec4f color;

    /**
     * Small dirt paths color.
     */
    vec4f dirt;

    /**
     * Roads border color.
     */
    vec4f border;

    /**
     * The mesh used for drawing curves.
     * Contains XY coordinates of each vertex.
     */
    ptr< Mesh<vec2f, unsigned int> > mesh;

    /**
     * The mesh used for drawing curves.
     * Contains XY coordinates + UV parameters for each vertex.
     */
    ptr< Mesh<vec4f, unsigned int> > meshuv;

    /**
     * Draws a road end white stripe between 2 points.
     *
     * @param fb a FrameBuffer.
     * @param p Coordinates of the first point.
     * @param n Coordinates of the second point.
     * @param w The line width.
     * @param scale The scale, based on the current display level.
     */
    void drawRoadEnd(const vec3d &tileCoords, ptr<FrameBuffer> fb, const vec2d &p, const vec2d &n, double length, float w, float scale, vec2d *nx = NULL, vec2d *ny = NULL, vec2d *lx = NULL, vec2d *ly = NULL);

    /**
     * Returns the length of a curve to be drawn without stripes.
     *
     * @param p a curve.
     * @param data the CurveData containing data about Curve p.
     * @param start if true, will compute from the starting node. Else, from the end node.
     */
    float getLengthWithoutStripes(CurvePtr p, CurveData * data, bool start);

    /**
     * Returns the longest flat length of curves around a node.
     *
     * @param p a Node.
     * @param q a position.
     * @param path a Curve.
     * @return the longest flat length around node p.
     */
    float getFlatLength(NodePtr p, vec2d q, CurvePtr path);

    ptr<Uniform3f> tileOffsetU;

    ptr<Uniform4f> colorU;

    ptr<Uniform2f> blendSizeU;

    ptr<Uniform3f> stripeSizeU;
};

}

#endif
