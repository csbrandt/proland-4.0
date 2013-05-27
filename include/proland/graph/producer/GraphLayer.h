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

#ifndef _PROLAND_GRAPHLAYER_H_
#define _PROLAND_GRAPHLAYER_H_

#include "ork/render/Mesh.h"
#include "ork/render/Program.h"
#include "ork/render/FrameBuffer.h"

#include "proland/graph/producer/GraphProducer.h"
#include "proland/graph/producer/CurveData.h"
#include "proland/graph/producer/Tesselator.h"

using namespace ork;

namespace proland
{

/**
 * An abstract Layer sub class for layers using graphs. Contains
 * drawing methods to draw curves and areas using a user defined GLSL Program.
 * @ingroup producer
 * @author Antoine Begault, Guillaume Piolat
 */
PROLAND_API class GraphLayer : public TileLayer
{
public:
    /**
     * Creates a new GraphLayer.
     *
     * @param name the name of the implementing class.
     */
    GraphLayer(const char *name);

    /**
     * Creates a new GraphLayer.
     *
     * @param name the name of the implementing class.
     * @param graphProducer the GraphProducer that produces the Graph tiles that
     *      this Layer must draw.
     * @param layerProgram the GLSL Program to be used to draw the graphs in this
     *      Layer.
     * @param displayLevel the tile level to start display. Tiles whole level is
     *      less than displayLevel are not drawn by this Layer, and graphProducer
     *      is not asked to produce the corresponding graph tiles.
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
    GraphLayer(const char *name, ptr<GraphProducer> graphProducer,
            ptr<Program> layerProgram, int displayLevel = 0, bool quality = true,
            bool storeGraphTiles = false, bool deform = false);

    /**
     * Deletes this GraphLayer.
     */
    virtual ~GraphLayer();

    virtual void getReferencedProducers(vector< ptr<TileProducer> > &producers) const;

    virtual void useTile(int level, int tx, int ty, unsigned int deadline);

    virtual void unuseTile(int level, int tx, int ty);

    virtual void setTileSize(int tileSize, int tileBorder, float rootQuadSize);

    virtual void prefetchTile(int level, int tx, int ty);

    virtual void startCreateTile(int level, int tx, int ty,
            unsigned int deadline, ptr<Task> task, ptr<TaskGraph> result);

    virtual void beginCreateTile();

    virtual void endCreateTile();

    virtual void stopCreateTile(int level, int tx, int ty);

    /**
     * Draws a curve in the given FrameBuffer.
     * Adds UV coordinates to the mesh, in order to compute stripes inside the
     * shader. Allows to draw stripes only on a certain part of the curve.
     *
     * @param p the curve to be drawn.
     * @param data the CurveData containing data about Curve p.
     * @param l0 the length without stripes at the begining of the curve.
     * @param l1 the length without stripes at the end of the curve.
     * @param width the curve's width (can be different from the initial
     *      curve's width).
     * @param scale the scale, depending of the display level.
     * @param fb a FrameBuffer.
     * @param prog a Program.
     * @param mesh a vec4 Mesh (containing vertex positions and UV attributes).
     */
    void drawCurve(const vec3d &tileCoords, CurvePtr p, CurveData *data, float l0, float l1, float width,
             float scale, ptr<FrameBuffer> fb, ptr<Program> prog, Mesh<vec4f, unsigned int> &mesh,
             vec2d *nx = NULL, vec2d *ny = NULL, vec2d *lx = NULL, vec2d *ly = NULL);

    // TODO find cap parameter definition ??
    /**
     * Draws a curve in the given FrameBuffer.
     * Adds UV coordinates to the mesh, in order to compute stripes inside the
     * shader. Also adds a cap information, which is used to ... ?
     *
     * @param p the curve to be drawn.
     * @param data the CurveData containing data about Curve p.
     * @param width the curve's width (can be different from the initial
     *      curve's width).
     * @param cap the width cap.
     * @param scale the scale, depending of the display level.
     * @param fb a FrameBuffer.
     * @param prog a Program.
     * @param mesh a vec4 Mesh (containing vertex positions and UV attributes).
     */
    void drawCurve(const vec3d &tileCoords, CurvePtr p, CurveData *data, float width, float cap,
            float scale, ptr<FrameBuffer> fb, ptr<Program> prog, Mesh<vec4f, unsigned int> &mesh,
            vec2d *nx = NULL, vec2d *ny = NULL, vec2d *lx = NULL, vec2d *ly = NULL);

    /**
     * Draws a curve in the given FrameBuffer.
     * Basic drawing, no stripes.
     *
     * @param p the curve to be drawn.
     * @param width the curve's width (can be different from the initial
     *      curve's width).
     * @param scale the scale, depending of the display level.
     * @param fb a FrameBuffer.
     * @param prog a Program.
     * @param mesh a vec2 Mesh (containing vertex positions).
     */
    void drawCurve(const vec3d &tileCoords, CurvePtr p, float width, float scale, ptr<FrameBuffer> fb, ptr<Program> prog,
            Mesh<vec2f, unsigned int> &mesh, vec2d *nx = NULL, vec2d *ny = NULL,
            vec2d *lx = NULL, vec2d *ly = NULL);

    /**
     * Draws a curve in the given FrameBuffer.
     *
     * @param p the curve to be drawn.
     * @param part coords to add to each of p's vertices.
     * @param fb a FrameBuffer.
     * @param prog a Program.
     * @param mesh a vec4 Mesh (containing vertex positions and UV attributes).
     */
    void drawCurve(const vec3d &tileCoords, CurvePtr p, const vec4f &part, ptr<FrameBuffer> fb, ptr<Program> prog,
            Mesh<vec4f, unsigned int> &mesh, vec2d *nx = NULL, vec2d *ny = NULL,
            vec2d *dx = NULL, vec2d *ly = NULL);

    /**
     * Checks whether a Node is the extremity of a Curve.
     *
     * @param curve the Curve to check.
     * @param p the node to check.
     * @return true if curve is the only curve of p, or if all other curves
     *      sharing p are not roads (type == 0).
     */
    bool extremity(CurvePtr curve, NodePtr p);

    /**
     * Draws an area.
     *
     * @param a the Area to be drawn.
     * @param tess the tesselator associated to the drawing mesh.
     */
    void drawArea(const vec3d &tileCoords, AreaPtr a, Tesselator &tess);

    /**
     * Adds a list of tiles used by each tile of this layer. They will
     * require a call to TileProducer#put() when the task has been done.
     *
     * @param level the level of the tile.
     * @param tx x coordinate of the tile.
     * @param ty y coordinate of the tile.
     * @param producer the Producer from which the Tiles were produced.
     * @param tiles the used tiles that will need to be released.
     */
    void addUsedTiles(int level, int tx, int ty, TileProducer *producer,
            set<TileCache::Tile*> tiles);

protected:
    /**
     * Initializes the fields of a GraphLayer.
     *
     * See #GraphLayer.
     */
    void init(ptr<GraphProducer> graphProducer, ptr<Program> layerProgram,
            int displayLevel = 0, bool quality = true, bool storeGraphTiles = false,
            bool deform = false);

    /**
     * The GraphProducer that produces the Graph tiles that this Layer must draw.
     */
    ptr<GraphProducer> graphProducer;

    /**
     * The GLSL Program to be used to draw the graphs in this Layer.
     */
    ptr<Program> layerProgram;

    /**
     * The tile level to start display. Tiles whole level is less than displayLevel
     * are not drawn by this Layer, and graphProducer is not asked to produce
     * the corresponding graph tiles.
     */
    int displayLevel;

    /**
     * Determines the drawing method for layers. If true, the display will be
     * better, if implemented.
     */
    bool quality;

    virtual void swap(ptr<GraphLayer> p);

private:
    /**
     * Forces the graph tiles produced by graphProducer to stay in cache as
     * long as the tiles produced by this producer (i.e. the TileProducer to
     * which this Layer belongs) are in use. When used with the storeParents
     * option in graphProducer, this option ensures that the graph tiles
     * corresponding to the tiles of this producer that are in use, and all
     * their ancestors, are in cache. Boths options are <b>required</b> when
     * an EditGraphOrthoLayer is used (to locate Node from x,y coordinates
     * efficiently, and to take advantage of the incremental clipUpdate
     * methods). Otherwise they are not mandatory.
     */
    bool storeGraphTiles;

    /**
     * The tiles currently in use. These tiles cannot be evicted from the cache
     * and from the TileStorage, until they become unused. Maps tile identifiers
     * to used tiles and to the TileProducer that produces those tiles.
     */
    map<TileCache::Tile::Id, pair<TileProducer *, set<TileCache::Tile*> > > usedTiles;
};

}

#endif
