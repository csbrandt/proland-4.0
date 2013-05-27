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

#ifndef _PROLAND_PARTICLE_PRODUCER_H_
#define _PROLAND_PARTICLE_PRODUCER_H_

#include "ork/render/Texture2D.h"
#include "proland/particles/ParticleLayer.h"

using namespace ork;

namespace proland
{

/**
 * An abstract %producer of %particles. A ParticleProducer uses an associated
 * ParticleStorage to store its %particles. <i>This storage must not be shared
 * with other ParticleProducer instances</i>. The main methods of a particle
 * producer are #moveParticles(), #removeOldParticles() and #addNewParticles().
 * Their name indicates their role quite clearly. A particle producer can have
 * layers, each layer needing its own fields per particle, and providing its
 * own #moveParticles(), #removeOldParticles() and  #addNewParticles() methods
 * using these layer specific fields (and possibly the fields provided by other
 * layers). A producer with layers allocates %particles with a size that is
 * sufficient to store the fields of all its layers (plus its own fields). It
 * also calls the three above methods of each layer.
 * The %particles can be stored on CPU or on GPU. In the GPU case some data may
 * be replicated on the CPU, like the %particles birth date, in order to know on
 * CPU when to remove old particles and when to create new ones (this storage
 * management can only be done on CPU). In the CPU case the %particles data can
 * also be copied on GPU if necessary (see #copyToTexture()).
 * @ingroup particles
 * @authors Eric Bruneton, Antoine Begault
 */
PROLAND_API class ParticleProducer : public Object
{
public:
    /**
     * Callback used to get the parameters of a particle in #copyToTexture().
     *
     * @param producer a particle producer.
     * @param p a particle produced by 'producer'.
     * @param[out] params the parameters for the given particle.
     */
    typedef bool (*getParticleParams)(ParticleProducer *producer, ParticleStorage::Particle *p, float *params);

    /**
     * Creates a new ParticleProducer.
     *
     * @param type the type of this ParticleProducer.
     * @param storage the storage to create and store the %particles.
     */
    ParticleProducer(const char* type, ptr<ParticleStorage> storage);

    /**
     * Deletes this ParticleProducer.
     */
    virtual ~ParticleProducer();

    /**
     * Returns the ParticleStorage used by this %producer to create and store
     * its %particles.
     */
    ptr<ParticleStorage> getStorage() const;

    /**
     * Returns the number of layers of this %producer.
     */
    int getLayerCount() const;

    /**
     * Returns the layer of this %producer whose index is given.
     *
     * @param index a layer index between 0 and #getLayerCount (exclusive).
     */
    ptr<ParticleLayer> getLayer(int index) const;

    /**
     * Returns the first found layer of type T.
     *
     * @templateparam T the type of the layer to be looked for.
     * @return the first found layer of type T.
     */
    template <typename T> T* getLayer() const
    {
        for (int i = 0; i < getLayerCount(); ++i) {
            ptr<ParticleLayer> l = getLayer(i);
            ptr<T> r = l.cast<T>();
            if (r != NULL) {
                return r.get();
            }
        }
        return NULL;
    }

    /**
     * Returns true if the list of layers is not empty. False otherwise.
     */
    bool hasLayers() const;

    /**
     * Adds a Layer to this %producer.
     *
     * @param l the layer to be added.
     */
    void addLayer(ptr<ParticleLayer> l);

    /**
     * Returns the tile producers used by this ParticleProducer.
     *
     * @param[out] producers the tile producers used by this ParticleProducer.
     */
    virtual void getReferencedProducers(std::vector< ptr<TileProducer> > &producers) const;

    /**
     * Returns the size in bytes of the data that must be stored for each
     * particle. The default implementation of this method returns 0.
     */
    virtual int getParticleSize();

    /**
     * Updates the %particles produced by this %producer. This method calls
     * #moveParticles(), #removeOldParticles() and #addNewParticles(), in
     * this order.
     *
     * @param dt the elapsed time since the last call to this method, in
     *      microseconds.
     */
    virtual void updateParticles(double dt);

    /**
     * Moves the existing %particles.
     * The default implementation of this method calls the corresponding
     * method on each layer of this %producer.
     *
     * @param dt the elapsed time since the last call to this method, in
     *      microseconds.
     */
    virtual void moveParticles(double dt);

    /**
     * Removes old %particles.
     * The default implementation of this method calls the corresponding
     * method on each layer of this %producer.
     */
    virtual void removeOldParticles();

    /**
     * Adds new particles.
     * The default implementation of this method calls the corresponding
     * method on each layer of this %producer.
     */
    virtual void addNewParticles();

    /**
     * Returns a new and initialized particle.
     * The default implementation of this method creates a new particle with
     * ParticleStorage#newParticle and initializes it by calling on each layer
     * the ParticleLayer#initParticle method.
     */
    virtual ParticleStorage::Particle *newParticle();

    /**
     * Copies the %particles data to the given texture. The texture size must
     * be ceil(paramCount / 4) times maxParticles, where maxParticles is the
     * ParticleStorage capacity. Each particle is stored in its own row of
     * the texture, with parameters stored in columns, four per column.
     *
     * @param t the destination texture, or NULL to create a new one.
     * @param paramCount the number of parameters to be stored per particle.
     * @param getParams the function used to get the parameters of each
     *      particle.
     * @param useFuncRes whether the user-defined function's return value
     *      has an influence on what should be stored in the texture.
            If true, the user will have to determine how to access to the particles
            on GPU. Otherwise, the particles will be stored depending on their
            index in the storage. Default is false.
     * @return the given texture, or a new one if t was not of correct size.
     */
    ptr<Texture2D> copyToTexture(ptr<Texture2D> t, int paramCount, getParticleParams getParams, bool useFuncRes = false);

protected:
    /**
     * Creates an uninitialized ParticleProducer.
     */
    ParticleProducer();

    /**
     * Creates an uninitialized ParticleProducer.
     *
     * @param type the type of this %producer.
     */
    ParticleProducer(const char* type);

    /**
     * Initializes this ParticleProducer. See #ParticleProducer.
     */
    void init(ptr<ParticleStorage> storage);

    void swap(ptr<ParticleProducer> p);

private:
    /**
     * The ParticleStorage used by this %producer to create and store its
     * %particles.
     */
    ptr<ParticleStorage> storage;

    /**
     * The ParticleLayer associated with this %producer.
     */
    std::vector< ptr<ParticleLayer> > layers;

    /**
     * The size of the #params array.
     */
    int paramSize;

    /**
     * Temporary array used in #copyToTexture.
     */
    float *params;

    /**
     * True if this %producer and its layers have been initialized.
     */
    bool initialized;

    /**
     * Initializes the storage and the layers associated with this %producer.
     */
    void initialize();
};

}

#endif
