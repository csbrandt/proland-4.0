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

#ifndef _PROLAND_RANDOM_PARTICLE_LAYER_H_
#define _PROLAND_RANDOM_PARTICLE_LAYER_H_

#include "ork/math/box3.h"
#include "proland/particles/ParticleProducer.h"

using namespace ork;

namespace proland
{

/**
 * A ParticleLayer to add random 3D coordinates to a %particle. This class provides
 * particle data to store a position for each particle.
 * @ingroup particles
 * @authors Antoine Begault, Eric Bruneton
 */
PROLAND_API class RandomParticleLayer : public ParticleLayer
{
public:
    /**
     * Layer specific particle data for managing random %particles.
     */
    struct RandomParticle
    {
        /**
         * The current particle random position.
         */
        vec3f randomPos;
    };

    /**
     * Creates a new RandomParticleLayer.
     *
     * @param bounds the bounds of the random positions.
     */
    RandomParticleLayer(box3f bounds);

    /**
     * Deletes this RandomParticleLayer.
     */
    virtual ~RandomParticleLayer();

    /**
     * Returns the random specific data of the given particle.
     *
     * @param p a particle.
     */
    inline RandomParticle *getRandomParticle(ParticleStorage::Particle *p)
    {
        return (RandomParticle*) getParticleData(p);
    }

    virtual void addNewParticles();

protected:
    /**
     * Creates an uninitialized RandomParticleLayer.
     */
    RandomParticleLayer();

    /**
     * Initializes this RandomParticleLayer. See #RandomParticleLayer.
     */
    void init(box3f bounds);

    /**
     * Initializes the random position of the given particle.
     */
    virtual void initParticle(ParticleStorage::Particle *p);

    virtual void swap(ptr<RandomParticleLayer> p);

private:
    /**
     * The bounds of the random positions.
     */
    box3f bounds;
};

}

#endif
