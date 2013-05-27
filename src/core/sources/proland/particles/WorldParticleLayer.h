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

#ifndef _PROLAND_WORLD_PARTICLE_LAYER_H_
#define _PROLAND_WORLD_PARTICLE_LAYER_H_

#include "ork/math/vec3.h"
#include "proland/particles/ParticleProducer.h"

using namespace ork;

namespace proland
{

#define UNINITIALIZED -1e9

/**
 * A ParticleLayer to manage %particles in world space. This class provides
 * particle data to store a position and a velocity for each particle. It
 * updates the positions based on velocities at each frame, but it does not
 * compute the velocities itself. This should be done by another layer.
 * @ingroup particles
 * @authors Eric Bruneton, Antoine Begault
 */
PROLAND_API class WorldParticleLayer : public ParticleLayer
{
public:
    /**
     * Layer specific particle data for managing %particles in world space.
     */
    struct WorldParticle
    {
        /**
         * The current particle position in world space.
         */
        vec3d worldPos;

        /**
         * The current particle velocity in world space.
         */
        vec3f worldVelocity;
    };

    /**
     * Creates a new WorldParticleLayer.
     *
     * @param speedFactor a global scaling factor to be applied to all particle
     *      velocities.
     */
    WorldParticleLayer(float speedFactor);

    /**
     * Deletes this WorldParticleLayer.
     */
    virtual ~WorldParticleLayer();

    /**
     * Returns the global scaling factor to be applied to all particle
     * velocities.
     */
    float getSpeedFactor() const;

    /**
     * Sets the global scaling factor to be applied to all particle
     * velocities.
     */
    void setSpeedFactor(float speedFactor);

    /**
     * Returns true if this WorldParticleLayer is in paused state.
     */
    bool isPaused() const;

    /**
     * Sets the paused state of this WorldParticleLayer.
     *
     * @param paused if true, particles position won't be updated.
     */
    void setPaused(bool paused);

    /**
     * Returns the world space specific data of the given particle.
     *
     * @param p a particle.
     */
    inline WorldParticle *getWorldParticle(ParticleStorage::Particle *p)
    {
        return (WorldParticle*) getParticleData(p);
    }

    /**
     * Moves the %particles based on their velocity. The velocities are not
     * updated. This should be done by another layer.
     */
    virtual void moveParticles(double dt);

protected:
    /**
     * Creates an uninitialized WorldParticleLayer.
     */
    WorldParticleLayer();

    /**
     * Initializes this WorldParticleLayer. See #WorldParticleLayer.
     */
    void init(float speedFactor);

    /**
     * Initializes the world position and velocity of the given particle.
     */
    virtual void initParticle(ParticleStorage::Particle *p);

    virtual void swap(ptr<WorldParticleLayer> p);

private:
    /**
     * Global scaling factor to be applied to all particle velocities.
     */
    float speedFactor;

    /**
     * If true, particles position won't be updated.
     */
    bool paused;
};

}

#endif
