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

#ifndef _PROLAND_WATERELEVATIONLAYER_H_
#define _PROLAND_WATERELEVATIONLAYER_H_

#include "proland/dem/ElevationGraphLayer.h"
#include "proland/dem/ElevationMargin.h"
#include "proland/dem/ElevationCurveData.h"

namespace proland
{

/**
 * An ElevationGraphLayer for rivers and lakes graphs.
 * @ingroup dem
 * @authors Antoine Begault, Eric Bruneton, Guillaume Piolat
 */
PROLAND_API class WaterElevationLayer : public ElevationGraphLayer
{
public:
    /**
     * An ElevationCurveData for river elevation profiles.
     */
    class WaterElevationCurveData : public ElevationCurveData
    {
    public:
        /**
         * Predefined types for rivers.
         * Used for drawing and managing rivers.
         */
        enum riverType
        {
            BORDER = -2, //!< River Banks. Curves with this type have as ancestor the curve representing the center of the river they belong to.
            OBSTACLE = -1, //!< Floating obstacle or islands.
            RIVER = 0, //!< Basic river.
            ISLAND = 1, //!< Represents islands when area1 != NULL.
            LAKE = 2//!< Lake.
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
        WaterElevationCurveData(CurveId id, CurvePtr flattenCurve, ptr<TileProducer> elevations);

        /**
         * Deletes this WaterElevationCurveData.
         */
        ~WaterElevationCurveData();

        virtual float getAltitude(float s);

        virtual float getSampleLength(CurvePtr c);

    protected:
        /**
         * Computes the cap length at a given extremity.
         *
         * @param p the Node from which to compute the cap length.
         * @param q a point determining the direction of the cap length.
         * @param path the Curve to compute the cap length from. Must contain p & q.
         */
        virtual float getCapLength(NodePtr p, vec2d q);
    };

    /**
     * An ElevationMargin for rivers and lakes. This margin takes into account the
     * total footprint width of rivers and lakes, larger than their real widths
     * (see ElevationGraphLayer#drawCurveAltitude).
     */
    class WaterElevationMargin : public ElevationMargin
    {
    public:
        /**
         * Creates a new WaterElevationMargin.
         *
         * @param samplesPerTile number of pixels per elevation tile (without borders).
         * @param borderFactor size of the border in percentage of tile size.
         */
        WaterElevationMargin(int samplesPerTile, float borderFactor);

        /**
         * Deletes this WaterElevationMargin.
         */
        ~WaterElevationMargin();

        virtual double getMargin(double clipSize, CurvePtr p);

        virtual double getMargin(double clipSize, AreaPtr a);
    };

    /**
     * Creates a new WaterElevationLayer.
     *
     * @param graphProducer the GraphProducer that produces the Graph tiles that
     *      this layer must draw.
     * @param layerProgram the Program to be used to draw the rivers in this layer.
     * @param fillProg the Program to be used to draw the large rivers and the
     *      lakes in this layer.
     * @param elevations the %producer used to compute raw %terrain elevations,
     *      themselves used to compute ElevationCurveData objects.
     * @param displayLevel the tile level to start display. Tiles whole level is
     *      less than displayLevel are not drawn by this Layer, and graphProducer is not
     *      asked to produce the corresponding %graph tiles.
     * @param quality enable or not the quality mode (better display).
     * @param deform whether we apply a spherical deformation on the layer or not.
     */
    WaterElevationLayer(ptr<GraphProducer> graphProducer, ptr<Program> layerProgram,
        ptr<Program> fillProg, ptr<TileProducer> elevations, int displayLevel = 0,
        bool quality = true, bool deform = false);

    /**
     * Deletes this WaterOrthoLayer.
     */
    virtual ~WaterElevationLayer();

    virtual void setTileSize(int tileSize, int tileBorder, float rootQuadSize);

    virtual bool doCreateTile(int level, int tx, int ty, TileStorage::Slot *data);

    virtual CurveData *newCurveData(CurveId id, CurvePtr flattenCurve);

protected:
    /**
     * The Program to be used to draw the large rivers and the lakes in this layer.
     */
    ptr<Program> fillProg;

    /**
     * Creates an uninitialized WaterOrthoLayer.
     */
    WaterElevationLayer();

    /**
     * Initializes this WaterElevationLayer.
     *
     * @param graphProducer the GraphProducer that produces the Graph tiles that
     *      this layer must draw.
     * @param layerProgram the Program to be used to draw the rivers in this layer.
     * @param fillProg the Program to be used to draw the large rivers and the
     *      lakes in this layer.
     * @param elevations the %producer used to compute raw %terrain elevations,
     *      themselves used to compute ElevationCurveData objects.
     * @param displayLevel the tile level to start display. Tiles whole level is
     *      less than displayLevel are not drawn by this Layer, and graphProducer is not
     *      asked to produce the corresponding %graph tiles.
     * @param quality enable or not the quality mode (better display).
     * @param deform whether we apply a spherical deformation on the layer or not.
     */
    void init(ptr<GraphProducer> graphProducer, ptr<Program> layerProgram,
        ptr<Program> fillProg, ptr<TileProducer> elevations, int displayLevel = 0,
        bool quality = true, bool deform = false);

    virtual void swap(ptr<WaterElevationLayer> p);

private:
    /**
     * The mesh used for drawing tesselated areas (large rivers and lakes).
     * Contains vertex positions.
     */
    ptr< Mesh<vec2f, unsigned int> > mesh;

    /**
     * The mesh used for drawing small rivers and the axis of large rivers.
     * Contains vertex positions and UV attributes. The position is set to
     * the vertex x,y coordinates. The u attribute contains the vertex z
     * coordinate, and the v attribute the signed distance to the river axis
     * (divided by the river width).
     */
    ptr< Mesh<vec4f, unsigned int> > meshuv;

    /**
     * The tesselator used for drawing areas (large rivers and lakes).
     */
    ptr<Tesselator> tess;

    ptr<Uniform3f> tileOffsetU;
    ptr<Uniform1i> riverU;

    ptr<Uniform3f> fillOffsetU;
    ptr<Uniform1f> depthU;
    ptr<Uniform4f> colorU;

};

}
#endif
