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

#ifndef _PROLAND_PLANTS_PRODUCER_H_
#define _PROLAND_PLANTS_PRODUCER_H_

#include "ork/scenegraph/SceneNode.h"

#include "proland/producer/TileProducer.h"
#include "proland/terrain/TerrainNode.h"
#include "proland/terrain/TileSampler.h"
#include "proland/plants/Plants.h"

using namespace std;

using namespace ork;

namespace proland
{

/**
 * TODO.
 * @ingroup plants
 * @author Eric Bruneton
 */
PROLAND_API class PlantsProducer : public TileProducer
{
public:
    /**
     * TODO.
     */
    ptr<SceneNode> node;

    /**
     * TODO.
     */
    ptr<TerrainNode> terrain;

    /**
     * TODO.
     */
    vec3d localCameraPos;

    /**
     * TODO.
     */
    vec3d tangentCameraPos;

    /**
     * TODO.
     */
    mat4d localToTangentFrame;

    /**
     * TODO.
     */
    mat4d localToScreen;

    /**
     * TODO.
     */
    mat4d screenToLocal;

    /**
     * TODO.
     */
    vec3d frustumV[8];

    /**
     * TODO.
     */
    vec4d frustumP[6];

    /**
     * TODO.
     */
    vec4d frustumZ;

    /**
     * TODO.
     */
    double zNear;

    /**
     * TODO.
     */
    double zRange;

    /**
     * TODO.
     */
    mat4d tangentFrameToScreen;

    /**
     * TODO.
     */
    mat4d tangentFrameToWorld;

    /**
     * TODO.
     */
    mat3d tangentSpaceToWorld;

    /**
     * TODO.
     */
    mat4d cameraToTangentFrame;

    /**
     * TODO.
     */
    vec3d cameraRefPos;

    /**
     * TODO.
     */
    vec3d tangentSunDir;

    /**
     * TODO.
     */
    int *offsets;

    /**
     * TODO.
     */
    int *sizes;

    /**
     * TODO.
     */
    int count;

    /**
     * TODO.
     */
    int total;

    /**
     * TODO.
     */
    vector<vec4d> plantBounds; // frustum z min max, altitude min max

    /**
     * TODO.
     */
    box3d plantBox; // bounding box in local space

    /**
     * TODO.
     */
    vector< ptr<PlantsProducer> > slaves;

    /**
     * TODO.
     */
    PlantsProducer *master;

    /**
     * TODO.
     */
    PlantsProducer(ptr<SceneNode> node, ptr<TerrainNode> terrain, ptr<Plants> plants,
        ptr<TileSampler> lcc, ptr<TileSampler> z, ptr<TileSampler> n, ptr<TileSampler> occ, ptr<TileCache> cache);

    /**
     * TODO.
     */
    PlantsProducer(PlantsProducer *master);

    /**
     * TODO.
     */
    virtual ~PlantsProducer();

    virtual bool hasTile(int level, int tx, int ty);

    /**
     * TODO.
     */
    void produceTiles();

    /**
     * TODO.
     */
    ptr<MeshBuffers> getPlantsMesh();

    /**
     * TODO.
     */
    static ptr<PlantsProducer> getPlantsProducer(ptr<SceneNode> terrain, ptr<Plants> plants);

protected:
    virtual bool doCreateTile(int level, int tx, int ty, TileStorage::Slot *data);

private:
    struct Tree
    {
        int tileCount;

        bool needTile;

        bool *needTiles;

        TileCache::Tile **tiles;

        Tree *children[4];

        Tree(int tileCount);

        ~Tree();

        void recursiveDelete(TileProducer *owner);
    };

    ptr<Plants> plants;

    ptr<TileSampler> lcc;

    ptr<TileSampler> z;

    ptr<TileSampler> n;

    ptr<TileSampler> occ;

    ptr<Uniform3f> tileOffsetU;

    ptr<UniformMatrix2f> tileDeformU;

    Tree *usedTiles;

    static map<pair<SceneNode*, Plants*>, PlantsProducer*> producers;

    bool mustAmplifyTile(double ox, double oy, double l);

    void putTiles(Tree **t, ptr<TerrainQuad> q);

    void getTiles(Tree **t, ptr<TerrainQuad> q);

    void updateTerrainHeights(ptr<TerrainQuad> q);
};

}

#endif
