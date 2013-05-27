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

#ifndef _PROLAND_HYDROFLOWDATA_H
#define _PROLAND_HYDROFLOWDATA_H

#include "ork/math/vec2.h"
#include "ork/math/vec3.h"
#include "ork/math/vec4.h"

#include "proland/particles/terrain/FlowTile.h"
#include "proland/rivers/graph/HydroCurve.h"

#define MAX_BANK_NUMBER 200 //MAX AMOUNT OF BANKS CONTAINED IN A FLOWDATA

#define MAX_NUM_DIST_CELLS 8 //MAX AMOUNT OF DISTCELLS

namespace proland
{

/**
 * Acceleration data structure used to compute distance to various
 * banks and determine potential values at banks.
 * Provides informations on banks, obstacles, etc... for a given Tile.
 * @ingroup rivers
 * @author Antoine Begault
 */
PROLAND_API class HydroFlowTile: public FlowTile
{
public:
    /**
     * Predefined type for rivers.
     * Used for drawing and managing rivers.
     */
    enum riverType
    {
        OBSTACLE = -1, //!< Floating obstacle or islands.
        RIVER = 0, //!< Basic river.
        INVISIBLE = 1, //!< Used to represent islands, but also invisible rivers, or closing segments for large river extremities.
        BANK = 2//!< Actual visible Banks.
    };

    /**
     * Creates a new HydroFlowTile.
     *
     * @param ox the x coordinate of the lower left corner of this Tile.
     * @param oy the y coordinate of the lower left corner of this Tile.
     * @param size width of this tile.
     * @param inter_power determines the slip conditions.
     * @param cacheSize size of the potentials cache.
     * @param searchRadiusFactor determines the radius of a DistCell coverage.
     */
    HydroFlowTile(float ox, float oy, float size, float inter_power, int cacheSize, float searchRadiusFactor);

    /**
     * Deletes this HydroFlowTile.
     */
    virtual ~HydroFlowTile();

    /**
     * Adds a bank to this FlowTile.
     *
     * @param curves a set of HydroCurves
     * @param maxWidth width of the largest Curve.
     */
    void addBanks(vector< ptr<HydroCurve> > &curves, float maxWidth);

    /**
     * Returns the velocity at a given point, depending on the data contained in this FlowTile.
     *
     * @param pos a XY position inside the viewport of this FlowTile.
     * @param[out] velocity a vec2d containing the 2D velocity at given coordinates.
     * @param[out] type type of data at given coordinates. See #dataType
     */
    virtual void getVelocity(vec2d &pos, vec2d &velocity, int &type);

    /**
     * Checks if a given tile has the corresponding parameters.
     * Returns false if the tile has any of its fields different from those parameters.
     * The tile will then have to be recomputed.
     */
    inline bool equals(unsigned int version, float inter_power, int cacheSize, float searchRadiusFactor) const
    {
        return this->version == version && this->inter_power == inter_power && this->cacheSize == cacheSize && this->searchRadiusFactor == searchRadiusFactor;
    }

    /**
     * Debug print method.
     */
    void print();

private:
    /**
     * Contains distances to each bank and river axis at a given coordinate in this HydroFlowTile.
     * Also stores the closest edge for each Curve.
     * Used to fasten distance computation -> We can directly get the closest edges for each bank and
     * compute their distances to this cell, instead of computing distances to each edges at each frame.
     * The DistCell table should be large enough to avoid over-simplification, and small enough to keep
     * real-time tile creation.
     */
    struct DistCell
    {
    public:
        /**
         * Creates a DistCell.
         */
        DistCell();

        /**
         * Creates a DistCell.
         *
         * @param coords coords of this Cell (x, y, size).
         */
        DistCell(vec3d coords);

        ~DistCell();

        /**
         * Edges contained in this DistCell for each Curve.
         */
        vector<int> edges[MAX_BANK_NUMBER];

        /**
         * Coords of this Cell (x, y, size).
         */
        vec3d coords;

        /**
         * Center of this Cell.
         */
        vec2d center;

        /**
         * Real bounds of this Cell. Can be larger than the actual size of the Cell, in order to include large rivers.
         */
        box2d bounds;

        /**
         * The list of Curves in this Cell's area of search.
         * We don't store distances for curves that are not in the search radius of this Cell,
         * since they shouldn't be used for computing the flow in this area.
         */
        set<int> bankIds;

        /**
         * The list of rivers in this Cell's area of search.
         */
        set<CurveId> riverIds;
    };

    /**
     * Maps a CurveId to a River axis.
     */
    map<CurveId, int> rivers;

    /**
     * Maps a river axis to its associated banks.
     * Each bank is associated to 1 river, and each river should have at least 2 banks (one on each side).
     * riversToBanks::iterator.second.size should always be at least 2.
     */
    map<CurveId, vector<int> > riversToBanks;

    /**
     * The whole Curves list.
     */
    vector<ptr<HydroCurve> > banks;

    /**
     * Distance Table. See DistCell.
     */
    DistCell* distCells;//[MAX_NUM_DIST_CELLS * MAX_NUM_DIST_CELLS];

    /**
     * Temporary array used to compute distances to a point at each frame.
     * Stored here for performance issues.
     */
    float* DISTANCES;

    /**
     * Temporary array used to compute closest edges to a point at each frame.
     * Stored here for performance issues.
     */
    int * CLOSESTEDGESIDS;

    /**
     * Largest River's width.
     */
    float maxWidth;

    /**
     * Actual number of dist cells.
     */
    int numDistCells;

    /**
     * Determines the radius of a DistCell coverage.
     */
    float searchRadiusFactor;

    /**
     * See #searchRadiusFactor.
     */
    float maxSearchDist;

    /**
     * Determines the slip conditions.
     */
    float inter_power;

    /**
     * Cache storing every potentials already computed.
     */
    float *potentials;

    /**
     * Size of the potentials cache.
     */
    int cacheSize;

    /**
     * Version of the Graph used to create this FlowTile. If the Graph changes, we need to update this FlowTile.
     */
    unsigned int version;

    /**
     * Returns true if a given point is inside a river.
     * A point is considered inside a river if it's distance to at least 1 river axis is lower that this river's width.
     *
     * @param pos coordinates of the point.
     * @param distCell DistCell containing the point.
     * @param[out] riverId a river containing pos, if any.
     */
     bool isInRiver(vec2d &pos, DistCell *distCell, int &riverId);

    /**
     * Returns the list of banks linked to a given river axis around a given point.
     * This allows to retrieve quickly the banks that we need.
     *
     * @param pos coordinates of the point
     * @param distCell DistCell contianing the point.
     * @param riverId a river containing pos.
     * @param[out] bankIds the ids of the banks linked to the river.
     */
    void getLinkedEdges(vec2d &pos, DistCell *distCell, int riverId, set<int> &bankIds);

    /**
     * Returns the distances of a given point to the various curves.
     * Only distances to the rivers which id is provided in riverIds will be computed.
     *
     * @param pos coordinates of the point.
     * @param distCell DistCell containing the point.
     * @param[out] distances distances to each borders.
     * @param riverIds the rivers containing pos.
     */
    void getDistancesToBanks(vec2d &pos, DistCell *distCell, set<int> &bankIds, map<int, float> &distances);

    /**
     * Returns the potential value at a given point, depending on the distances to each banks.
     *
     * @param pos coordinates of the point.
     * @param distances distances to each banks.
     * @param[out] potential resulting potential value.
     * @param[out] type resulting type. If everything was fine, should be FlowTile::INSIDE.
     */
    void getPotential(vec2d &pos, map<int, float> &distances, float &potential, int &type);

    /**
     * Returns the potential value at a given point. This computes 4 potentials, which will then be interpolated in
     * #getVelocity() to determine the speed at a given coordinate.
     *
     * @param pos coordinates of the point;
     * @param[out] potentials the 4 resulting potential values.
     * @param type type of data at point pos.
     */
    void getFourPotentials(vec2d &pos, vec4f &potentials, int &type);

    friend class HydroFlowProducer;
};

}

#endif
