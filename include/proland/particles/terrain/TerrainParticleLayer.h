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

#ifndef _PROLAND_TERRAIN_PARTICLE_LAYER_H_
#define _PROLAND_TERRAIN_PARTICLE_LAYER_H_

#include "ork/scenegraph/SceneNode.h"
#include "proland/particles/LifeCycleParticleLayer.h"
#include "proland/particles/WorldParticleLayer.h"
#include "proland/particles/screen/ScreenParticleLayer.h"
#include "proland/particles/terrain/FlowTile.h"
#include "proland/producer/TileProducer.h"
#include "proland/terrain/TerrainNode.h"

using namespace ork;

namespace proland
{

/**
 * @defgroup terrainl terrain
 * Provides a particle layer to advect particles one a terrain.
 * @ingroup particles
 */

/**
 * A ParticleLayer to advect %particles in world space by using a velocity
 * field defined on one or more terrains. This layer requires %particles
 * world positions and velocities to be managed by a WorldParticleLayer.
 * @ingroup terrainl
 * @author Antoine Begault, Guillaume Piolat
 */
PROLAND_API class TerrainParticleLayer : public ParticleLayer
{
public:
    /**
     * Layer specific particle data for managing %particles on terrains.
     */
    struct TerrainParticle
    {
        /**
         * The current particle position in local space inside a terrain.
         */
        vec3d terrainPos;

        /**
         * The current particle velocity in local space inside a terrain.
         */
        vec2d terrainVelocity;

        /**
         * The TileProducer that produces the FlowTile on which this %particle is.
         */
        TileProducer *producer;

        /**
         * Current particle status. See FlowTile#status.
         */
        int status;

        int terrainId;

        /**
         * True if the current particle velocity was not computed yet.
         */
        bool firstVelocityQuery;
    };

    /**
     * Contains a SceneNode and its corresponding TerrainNode.
     * The SceneNode is used to determine on which terrain the
     *   particle is via the worldToLocal methods.
     * The TerrainNode is used to determine if the particle is
     *   in the bounds of the terrain.
     */
    struct TerrainInfo
    {
    public:
        /**
         * Creates a new TerrainInfo.
         *
         * @param n a SceneNode.
         */
        TerrainInfo(ptr<SceneNode> n, int id) {
            this->node = n;
            this->terrain = (node->getField("terrain").cast<TerrainNode>());
            this->id = id;
        }

        /**
         * A SceneNode.
         */
        ptr<SceneNode> node;

        /**
         * The TerrainNode associated to #node.
         */
        ptr<TerrainNode> terrain;

        /**
         * Current terrain id. also used in particles to store the terrain on
         * which they are located.
         */
        int id;
    };

    /**
     * Creates a new TerrainParticleLayer.
     *
     * @param infos each flow producer mapped to its SceneNode.
     */
    TerrainParticleLayer(map<ptr<TileProducer>, TerrainInfo *> infos);

    /**
     * Deletes this LifeCycleParticleLayer.
     */
    virtual ~TerrainParticleLayer();

    /**
     * Returns the screen space specific data of the given particle.
     *
     * @param p a particle.
     */
    inline TerrainParticle *getTerrainParticle(ParticleStorage::Particle *p)
    {
        return (TerrainParticle*) getParticleData(p);
    }

    inline map<ptr<TileProducer>, TerrainInfo *> getTerrainInfos()
    {
        return infos;
    }

    virtual void getReferencedProducers(vector< ptr<TileProducer> > &producers) const;

    virtual void moveParticles(double dt);

protected:
    /**
     * Creates an uninitialized TerrainParticleLayer.
     */
    TerrainParticleLayer();

    /**
     * Initializes this TerrainParticleLayer. See #TerrainParticleLayer.
     */
    void init(map<ptr<TileProducer>, TerrainInfo *> infos);

    virtual void initialize();

    virtual void initParticle(ParticleStorage::Particle *p);

    virtual void swap(ptr<TerrainParticleLayer> p);

    /**
     * Finds the FlowTile from a given TileProducer at given local coordinates.
     *
     * @param producer the TileProducer used to create the FlowTiles.
     * @param t the last browsed FlowTile.
     * @param pos local terrain coordinates for which we need the FlowTile.
     */
    ptr<FlowTile> findFlowTile(ptr<TileProducer> producer, TileCache::Tile *t, vec3d &pos);

    /**
     * Returns the FlowTile required to compute the velocity of a given TerrainParticle.
     */
    ptr<FlowTile> getFlowTile(TerrainParticle *t);

    /**
     * Returns the TileProducer associated to the terrain on which a given Particle is.
     */
    ptr<TileProducer> getFlowProducer(ParticleStorage::Particle *p);

    /**
     * Each flow producer mapped to its SceneNode.
     */
    map<ptr<TileProducer>, TerrainInfo*> infos;

private:
    /**
     * The layer managing the life cycle of %particles.
     */
    LifeCycleParticleLayer *lifeCycleLayer;

    /**
     * The layer managing the %particles in screen space.
     */
    ScreenParticleLayer *screenLayer;

    /**
     * The layer managing the %particles in world space.
     */
    WorldParticleLayer *worldLayer;
};

}

#endif
