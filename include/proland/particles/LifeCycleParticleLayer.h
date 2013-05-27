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

#ifndef _PROLAND_LIFECYCLE_PARTICLE_LAYER_H_
#define _PROLAND_LIFECYCLE_PARTICLE_LAYER_H_

#include "proland/particles/ParticleProducer.h"

namespace proland
{

/**
 * A ParticleLayer to manage the lifecycle of %particles. This class manages
 * a simple lifecycle where %particles can be fading in, active, or fading out.
 * The transitions between the three states are based on the particle's age,
 * and on globally defined fading in, active, and fading out delays.
 * @ingroup particles
 * @authors Eric Bruneton, Antoine Begault
 */
PROLAND_API class LifeCycleParticleLayer : public ParticleLayer
{
public:
    /**
     * Layer specific particle data for managing the lifecycle of %particles.
     */
    struct LifeCycleParticle
    {
        /**
         * The birth date of this particle, in microseconds. This birth date
         * allows one to compute the age of the particle. If this age is
         * between 0 and fadeInDelay the particle is fading in. If this age is
         * between fadeInDelay and fadeInDelay + activeDelay, the particle
         * is active. Otherwise it is fading out. Note that we do not store the
         * particle's age directly to avoid updating it at each frame.
         */
        float birthDate;
    };

    /**
     * Creates a new LifeCycleParticleLayer.
     *
     * @param fadeInDelay the fade in delay of %particles, in microseconds.
     *      0 means that %particles are created directly in active state.
     * @param activeDelay the active delay of %particles, in microseconds.
     * @param fadeOutDelay the fade out delay of %particles, in microseconds.
     *      0 means that %particles are deleted when the become inactive.
     */
    LifeCycleParticleLayer(float fadeInDelay, float activeDelay, float fadeOutDelay);

    /**
     * Deletes this LifeCycleParticleLayer.
     */
    virtual ~LifeCycleParticleLayer();

    /**
     * Returns the fade in delay of %particles, in microseconds.
     * 0 means that %particles are created directly in active state.
     */
    float getFadeInDelay() const;

    /**
     * Sets the fade in delay of %particles, in microseconds.
     * 0 means that %particles are created directly in active state.
     *
     * @param delay the new fade in delay.
     */
    void setFadeInDelay(float delay);

    /**
     * Returns the active delay of %particles, in microseconds.
     */
    float getActiveDelay() const;

    /**
     * Sets the active delay of %particles, in microseconds.
     *
     * @param delay the new active delay.
     */
    void setActiveDelay(float delay);

    /**
     * Returns the fade out delay of %particles, in microseconds.
     * 0 means that %particles are deleted when they become inactive.
     */
    float getFadeOutDelay() const;

    /**
     * Sets the fade out delay of %particles, in microseconds.
     * 0 means that %particles are deleted when they become inactive.
     *
     * @param delay the new fade out delay.
     */
    void setFadeOutDelay(float delay);

    /**
     * Returns the lifecycle specific data of the given particle.
     *
     * @param p a particle.
     */
    inline LifeCycleParticle *getLifeCycle(ParticleStorage::Particle *p)
    {
        return (LifeCycleParticle*) getParticleData(p);
    }

    /**
     * Returns the birth date of the given particle.
     *
     * @param p a particle.
     * @return the birth date of the given particle, in microseconds.
     */
    inline float getBirthDate(ParticleStorage::Particle *p)
    {
        return getLifeCycle(p)->birthDate;
    }

    /**
     * Returns true if the given particle is fading in.
     */
    bool isFadingIn(ParticleStorage::Particle *p);

    /**
     * Returns true if the given particle is active.
     */
    bool isActive(ParticleStorage::Particle *p);

    /**
     * Returns true if the given particle is fading out.
     */
    bool isFadingOut(ParticleStorage::Particle *p);

    /**
     * Forces the given particle to start fading out.
     */
    void setFadingOut(ParticleStorage::Particle *p);

    /**
     * Forces the given particle to be deleted immediatly.
     */
    void killParticle(ParticleStorage::Particle *p);

    /**
     * Returns an intensity for the given particle, based on its current
     * state. This intensity varies between 0 to 1 during fade in, stays equal
     * to 1 when the particle is active, and varies from 1 to 0 during fade out.
     */
    float getIntensity(ParticleStorage::Particle *p);

    /**
     * Updates the current time. We don't need to update the %particles
     * because we store their birth date instead of their age.
     */
    virtual void moveParticles(double dt);

    /**
     * Deletes the %particles that have completely faded out.
     */
    virtual void removeOldParticles();

protected:
    /**
     * Creates an uninitialized LifeCycleParticleLayer.
     */
    LifeCycleParticleLayer();

    /**
     * Initializes this LifeCycleParticleLayer. See #LifeCycleParticleLayer.
     */
    void init(float fadeInDelay, float activeDelay, float fadeOutDelay);

    /**
     * Initializes the birth date of the given particle to #time.
     */
    virtual void initParticle(ParticleStorage::Particle *p);

    virtual void swap(ptr<LifeCycleParticleLayer> p);

private:
    /**
     * The fade in delay of %particles, in microseconds. 0 means that
     * %particles are created directly in active state.
     */
    float fadeInDelay;

    /**
     * The active delay of %particles, in microseconds.
     */
    float activeDelay;

    /**
     * The fade out delay of %particles, in microseconds. 0 means that
     * %particles are deleted when the become inactive.
     */
    float fadeOutDelay;

    /**
     * The current time, in microseconds. This time is updated in
     * #moveParticles and used to set the birth date of particles in
     * #initParticle.
     */
    float time;
};

}

#endif
