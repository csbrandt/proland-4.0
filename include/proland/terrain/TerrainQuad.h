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

#ifndef _PROLAND_TERRAIN_QUAD_H_
#define _PROLAND_TERRAIN_QUAD_H_

#include "ork/scenegraph/SceneManager.h"

using namespace ork;

namespace proland
{

class TerrainNode;

/**
 * A quad in a %terrain quadtree. The quadtree is subdivided based only
 * on the current viewer position. All quads are subdivided if they
 * meet the subdivision criterion, even if they are outside the view
 * frustum. The quad visibility is stored in #visible. It can be used
 * in TileSampler to decide whether or not data must be produced
 * for invisible tiles (we recall that the %terrain quadtree itself
 * does not store any %terrain data).
 * @ingroup terrain
 * @authors Eric Bruneton, Antoine Begault, Guillaume Piolat
 */
PROLAND_API class TerrainQuad : public Object
{
public:
    /**
     * The parent quad of this quad.
     */
    const TerrainQuad *parent;

    /**
     * The level of this quad in the quadtree (0 for the root).
     */
    const int level;

    /**
     * The logical x coordinate of this quad (between 0 and 2^level).
     */
    const int tx;

    /**
     * The logical y coordinate of this quad (between 0 and 2^level).
     */
    const int ty;

    /**
     * The physical x coordinate of the lower left corner of this quad
     * (in local space).
     */
    const double ox;

    /**
     * The physical y coordinate of the lower left corner of this quad
     * (in local space).
     */
    const double oy;

    /**
     * The physical size of this quad (in local space).
     */
    const double l;

    /**
     * The minimum %terrain elevation inside this quad. This field must
     * be updated manually by users (the TileSamplerZ class can
     * do this for you).
     */
    float zmin;

    /**
     * The maximum %terrain elevation inside this quad. This field must
     * be updated manually by users (the TileSamplerZ class can
     * do this for you).
     */
    float zmax;

    /**
     * The four subquads of this quad. If this quad is not subdivided,
     * the four values are NULL. The subquads are stored in the
     * following order: bottomleft, bottomright, topleft, topright.
     */
    ptr<TerrainQuad> children[4];

    /**
     * The visibility of the bounding box of this quad from the current
     * viewer position. The bounding box is computed using #zmin and
     * #zmax, which must therefore be up to date to get a correct culling
     * of quads out of the view frustum. This visibility only takes frustum
     * culling into account.
     */
    SceneManager::visibility visible;

    /**
     * True if the bounding box of this quad is occluded by the bounding
     * boxes of the quads in front of it.
     */
    bool occluded;

    /**
     * True if the quad is invisible, or if all its associated tiles are
     * produced and available in cache (this may not be the case if the
     * asynchronous mode is used in a TileSampler).
     */
    bool drawable;

    /**
     * Creates a new TerrainQuad.
     *
     * @param owner the TerrainNode to which the %terrain quadtree belongs.
     * @param parent the parent quad of this quad.
     * @param tx the logical x coordinate of this quad.
     * @param ty the logical y coordinate of this quad.
     * @param ox the physical x coordinate of the lower left corner of this quad.
     * @param oy the physical y coordinate of the lower left corner of this quad.
     * @param l the physical size of this quad.
     * @param zmin the minimum %terrain elevation inside this quad.
     * @param zmax the maximum %terrain elevation inside this quad.
     */
    TerrainQuad(TerrainNode *owner, const TerrainQuad *parent, int tx, int ty, double ox, double oy, double l, float zmin, float zmax);

    /**
     * Deletes this TerrainQuad.
     */
    virtual ~TerrainQuad();

    /**
     * Returns the TerrainNode to which the %terrain quadtree belongs.
     */
    TerrainNode *getOwner();

    /**
     * Returns true if this quad is not subdivided.
     */
    bool isLeaf() const;

    /**
     * Returns the number of quads in the tree below this quad.
     */
    int getSize() const;

    /**
     * Returns the depth of the tree below this quad.
     */
    int getDepth() const;

    /**
     * Subdivides or unsubdivides this quad based on the current
     * viewer distance to this quad, relatively to its size. This
     * method uses the current viewer position provided by the
     * TerrainNode to which this quadtree belongs.
     */
    void update();

private:
    /**
     * The TerrainNode to which this %terrain quadtree belongs.
     */
    TerrainNode *owner;

    /**
     * Creates the four subquads of this quad.
     */
    void subdivide();

    friend class TerrainNode;
};

}

#endif
