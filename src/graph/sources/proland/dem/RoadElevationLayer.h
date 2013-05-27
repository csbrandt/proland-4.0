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

#ifndef _PROLAND_ROADELEVATIONLAYER_H_
#define _PROLAND_ROADELEVATIONLAYER_H_

#include "proland/dem/ElevationGraphLayer.h"
#include "proland/dem/ElevationMargin.h"
#include "proland/dem/ElevationCurveData.h"

namespace proland
{

/**
 * An ElevationGraphLayer for road graphs.
 * @ingroup dem
 * @authors Antoine Begault, Eric Bruneton, Guillaume Piolat
 */
PROLAND_API class RoadElevationLayer : public ElevationGraphLayer
{
public:
    /**
     * An ElevationCurveData for road elevation profiles.
     */
    class RoadElevationCurveData : public ElevationCurveData
    {
    public:
        /**
         * Predefined types for roads.
         * Used for drawing and managing roads.
         */
        enum roadType
        {
            BASIC = 0,
            ROAD = 0, //!< Basic road.
            UNKNOWN = 1, //!< Undefined.
            BRIDGE = 2//!< Bridge linking 2 roads, passing on top of another.
        };

        /**
         * Creates a new RoadElevationCurveData.
         *
         * @param id the id of the curve for which we need to store the data.
         * @param flattenCurve the flattened version of the curve for which we need
         *      to store the data.
         * @param elevations the %producer used to compute raw %terrain elevations,
         *      themselves used to compute the elevation profile.
         */
        RoadElevationCurveData(CurveId id, CurvePtr flattenCurve, ptr<TileProducer> elevations);

        /**
         * Deletes this RoadElevationCurveData.
         */
        ~RoadElevationCurveData();

        virtual float getStartHeight();

        virtual float getEndHeight();

        virtual float getAltitude(float s);

        virtual void getUsedTiles(set<TileCache::Tile::Id> &tiles, float rootSampleLength);

        void getBridgesz();

    protected:
        /**
         * Computes the cap length at a given extremity.
         *
         * @param p the Node from which to compute the cap length.
         * @param q a point determining the direction of the cap length.
         * @param path the Curve to compute the cap length from. Must contain p & q.
         */
        virtual float getCapLength(NodePtr p, vec2d q);

        /**
         * True if the starting point of the road is connected to a bridge
         */
        bool startBridge;

        /**
         * True if the ending point of the road is connected to a bridge
         */
        bool endBridge;

        /**
         * Elevation at the starting point of the road, if connected to a bridge.
         */
        float startBridgez;

        /**
         * Elevation at the ending point of the road, if connected to a bridge.
         */
        float endBridgez;

        /**
         * True if extremities were checked.
         */
        bool initBridges;
    };

    /**
     * An ElevationMargin for roads. This margin takes into account the
     * total footprint width of roads, larger than their real widths
     * (see ElevationGraphLayer#drawCurveAltitude).
     */
    class RoadElevationMargin : public ElevationMargin
    {
    public:
        /**
         * Creates a new RoadElevationMargin.
         *
         * @param samplesPerTile number of pixels per elevation tile (without borders).
         * @param borderFactor size of the border in percentage of tile size.
         */
        RoadElevationMargin(int samplesPerTile, float borderFactor);

        /**
         * Deletes this RoadElevationMargin.
         */
        ~RoadElevationMargin();

        virtual double getMargin(double clipSize, CurvePtr p);
    };

    /**
     * Creates a new RoadElevationLayer.
     *
     * @param graphProducer the GraphProducer that produces the Graph tiles that
     *      this layer must draw.
     * @param layerProgram the Program to be used to draw the roads in this layer.
     * @param elevations the %producer used to compute raw %terrain elevations,
     *      themselves used to compute ElevationCurveData objects.
     * @param displayLevel the tile level to start display. Tiles whole level is
     *      less than displayLevel are not drawn by this Layer, and graphProducer is not
     *      asked to produce the corresponding %graph tiles.
     * @param quality enable or not the quality mode (better display).
     * @param deform whether we apply a spherical deformation on the layer or not.
     */
    RoadElevationLayer(ptr<GraphProducer> graphProducer, ptr<Program> layerProgram,
        ptr<TileProducer> elevations, int displayLevel = 0,
        bool quality = true, bool deform = false);

    /**
     * Deletes this RoadOrthoLayer.
     */
    virtual ~RoadElevationLayer();

    virtual void setTileSize(int tileSize, int tileBorder, float rootQuadSize);

    virtual bool doCreateTile(int level, int tx, int ty, TileStorage::Slot *data);

    virtual CurveData *newCurveData(CurveId id, CurvePtr flattenCurve);

protected:
    /**
     * Creates an uninitialized RoadOrthoLayer.
     */
    RoadElevationLayer();

    /**
     * Initializes this RoadElevationLayer.
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
     * @param deform whether we apply a spherical deformation on the layer or not.
     */
    void init(ptr<GraphProducer> graphProducer, ptr<Program> layerProgram,
        ptr<TileProducer> elevations, int displayLevel = 0,
        bool quality = true, bool deform = false);

    virtual void swap(ptr<RoadElevationLayer> p);

private:
    /**
     * The mesh used for drawing road elevation profiles. Contains vertex positions
     * and UV attributes. The position is set to the vertex x,y coordinates. The u
     * attribute contains the vertex z coordinate, and the v attribute the signed
     * distance to the road axis (divided by the road width).
     */
    ptr< Mesh<vec4f, unsigned int> > meshuv;

    ptr<Uniform3f> tileOffsetU;
};

}
#endif
