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

#ifndef _PROLAND_ELEVATIONGRAPHLAYER_H_
#define _PROLAND_ELEVATIONGRAPHLAYER_H_

#include "proland/graph/producer/GraphLayer.h"
#include "proland/graph/producer/CurveDataFactory.h"
#include "proland/dem/ElevationCurveData.h"

namespace proland
{

/**
 * An abstract GraphLayer to modify the elevations of an ElevationProducer,
 * by drawing a Graph. The elevations of the Graph vertices are given
 * by ElevationCurveData objects, created by this class (which also
 * implements CurveDataFactory). The raw %terrain elevations (used to
 * compute elevation profiles for curves in ElevationCurveData) are
 * computed on CPU with the help of a CPUElevationProducer (or an equivalent
 * - see CPUElevationProducer#getHeight) referenced by this layer.
 * @ingroup dem
 * @authors Antoine Begault, Eric Bruneton
 */
PROLAND_API class ElevationGraphLayer : public GraphLayer, public CurveDataFactory
{
public:
    /**
     * Creates a new ElevationGraphLayer.
     *
     * @param name the layer type.
     */
    ElevationGraphLayer(const char *name);

    /**
     * Creates a new ElevationGraphLayer.
     *
     * @param name the layer type.
     * @param graphProducer the GraphProducer that produces the Graph tiles that
     *      this layer must draw.
     * @param layerProgram the Program to be used to draw the graphs in this layer.
     * @param elevations the %producer used to compute raw %terrain elevations,
     *      themselves used to compute ElevationCurveData objects.
     * @param displayLevel the tile level to start display. Tiles whole level is
     *      less than displayLevel are not drawn by this Layer, and graphProducer is not
     *      asked to produce the corresponding %graph tiles.
     * @param quality enable or not the quality mode (better display).
     * @param storeGraphTiles forces the graph tiles produced by graphProducer
     *      to stay in cache as long as the tiles produced by this producer
     *      (i.e. the TileProducer to which this Layer belongs) are in use.
     *      When used with the storeParents option in graphProducer, this option
     *      ensures that the graph tiles corresponding to the tiles of this
     *      producer that are in use, and all their ancestors, are in cache.
     *      Boths options are <b>required</b> when an EditGraphOrthoLayer is used
     *      (to locate Node from x,y coordinates efficiently, and to take
     *      advantage of the incremental clipUpdate methods). Otherwise they
     *      are not mandatory.
     * @param deform whether we apply a spherical deformation on the layer or not.
     */
    ElevationGraphLayer(const char *name, ptr<GraphProducer> graphProducer,
        ptr<Program> layerProgram, ptr<TileProducer> elevations, int displayLevel = 0,
        bool quality = true, bool storeGraphTiles = false, bool deform = false);

    /**
     * Deletes this ElevationGraphLayer.
     */
    virtual ~ElevationGraphLayer();

    virtual void getReferencedProducers(vector< ptr<TileProducer> > &producers) const;

    virtual void startCreateTile(int level, int tx, int ty, unsigned int deadline,
        ptr<Task> task, ptr<TaskGraph> result);

    virtual void stopCreateTile(int level, int tx, int ty);

    /**
     * Draws the altitude profile of a curve. The curve is drawn with its total
     * footprint width, which includes not only the curve itself, but the also
     * the area where it may modifies the terrain elevations (e.g. this includes
     * roadsides or riverbanks for roads and %rivers).
     *
     * @param p the Curve to be drawn.
     * @param data the ElevationCurveData containing the elevation profile of p.
     *       This object is normally created by this CurveDataFactory.
     * @param width the total footprint width of the curve.
     * @param nwidth the total footprint width of the curve, divided by its real
     *      width.
     * @param stepLength the maximum distance between two OpenGL vertices on the
     *      tesselation used to draw the curve p. If the vertices of p are too far
     *      apart, intermediate vertices are introduced to respect this maximum distance.
     * @param caps true to draw caps at the extremities of the curve.
     * @param fb the FrameBuffer into which the curve must be drawn.
     * @param prog a program used to draw the curve.
     * @param mesh a vec4 Mesh (containing vertex positions and UV attributes).
     *      The position is set to the vertex x,y coordinates. The u attribute
     *      contains the vertex z coordinate, and the v attribute the signed relative
     *      distance to the curve axis (i.e., plus or minus nwidth).
     */
    void drawCurveAltitude(const vec3d &tileCoords, CurvePtr p, ElevationCurveData *data, float width, float nwidth,
        float stepLength, bool caps, ptr<FrameBuffer> fb, ptr<Program> prog, Mesh<vec4f, unsigned int> &mesh,
        vec2d *nx = NULL, vec2d *ny = NULL, vec2d *lx = NULL, vec2d *ly = NULL);

    virtual void swap(ptr<ElevationGraphLayer> p);

protected:
    /**
     * Initializes this ElevationGraphLayer.
     *
     * @param graphProducer the GraphProducer that produces the Graph tiles that
     *      this layer must draw.
     * @param layerProgram the Program to be used to draw the graphs in this layer.
     * @param elevations the %producer used to compute raw %terrain elevations,
     *      themselves used to compute ElevationCurveData objects.
     * @param displayLevel the tile level to start display. Tiles whole level is
     *      less than displayLevel are not drawn by this Layer, and graphProducer is not
     *      asked to produce the corresponding %graph tiles.
     * @param quality enable or not the quality mode (better display).
     * @param storeGraphTiles forces the graph tiles produced by graphProducer
     *      to stay in cache as long as the tiles produced by this producer
     *      (i.e. the TileProducer to which this Layer belongs) are in use.
     *      When used with the storeParents option in graphProducer, this option
     *      ensures that the graph tiles corresponding to the tiles of this
     *      producer that are in use, and all their ancestors, are in cache.
     *      Boths options are <b>required</b> when an EditGraphOrthoLayer is used
     *      (to locate Node from x,y coordinates efficiently, and to take
     *      advantage of the incremental clipUpdate methods). Otherwise they
     *      are not mandatory.
     * @param deform whether we apply a spherical deformation on the layer or not.
     */
    void init(ptr<GraphProducer> graphProducer, ptr<Program> layerProgram,
        ptr<TileProducer> elevations, int displayLevel = 0, bool quality = true,
        bool storeGraphTiles = false, bool deform = false);

    /**
     * The %producer used to compute raw %terrain elevations, themselves used
     * to compute ElevationCurveData objects.
     */
    ptr<TileProducer> elevations;
};

}

#endif
