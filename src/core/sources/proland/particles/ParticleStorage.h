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

#ifndef _PROLAND_PARTICLE_STORAGE_H_
#define _PROLAND_PARTICLE_STORAGE_H_

#include <string>
#include <vector>
#include <map>

#include "ork/render/TextureBuffer.h"

using namespace ork;

namespace proland
{

/**
 * @defgroup particles particles
 * Provides a framework to produce, animate and store %particles.
 * @ingroup proland
 */

/**
 * A storage to store %particles. This class provides both generic CPU and GPU
 * storages for %particles, and provides generic methods to keep track of the
 * currently allocated %particles in this storage, and to keep track of the
 * free slots that can be used to allocate new %particles.
 * @ingroup particles
 * @authors Eric Bruneton, Antoine Begault, Guillaume Piolat
 */
PROLAND_API class ParticleStorage : public Object
{
public:
    /**
     * An abstract particle stored by a ParticleStorage.
     */
    class Particle
    {
    };

    /**
     * Creates a new ParticleStorage.
     *
     * @param capacity the maximum number of %particles allocated and managed
     *      by this particle storage. This capacity is fixed and cannot change
     *      with time.
     * @param pack true to ensure that new %particles are always created with
     *      the minimum available index. This can be useful to keep allocated
     *      %particles tightly packed in memory. On the other hand the creation
     *      and destruction of %particles takes logarithmic time instead of
     *      constant time.
     */
    ParticleStorage(int capacity, bool pack);

    /**
     * Deletes this ParticleStorage. This deletes the data associated with all
     * the %particles managed by this particle storage.
     */
    virtual ~ParticleStorage();

    /**
     * Initializes the CPU storage for the %particles.
     *
     * @param particleSize the size in bytes, on CPU, of each particle.
     */
    void initCpuStorage(int particleSize);

    /**
     * Initializes a GPU storage for the %particles. The full GPU storage is
     * supposed to be splitted in several textures, each texture storing one
     * or more particle attribute (for instance one texture for positions,
     * another for velocities, etc). All textures are texture buffers,
     * (textures similar to 1D textures, whose pixels are accessible via a
     * GPUBuffer) of #capacity * components pixels. Each texture is associated
     * with a name, so that ParticleLayer can access them with symbolic names.
     *
     * @param name a symbolic name for this storage.
     * @param f the pixel format for this storage.
     * @param components how many components of format f must be stored per
     *      particle in this GPU storage.
     */
    void initGpuStorage(const std::string &name, TextureInternalFormat f, int components);

    /**
     * Returns the maximum number of %particles that can be stored in this
     * storage.
     */
    int getCapacity();

    /**
     * Returns a texture buffer containing %particles data on GPU. The
     * %particles data on GPU is splitted in several textures (see
     * #initGpuStorage()) identified by names. This method returns the texture
     * buffer whose name is given. Its associated GPU buffer can be used in a
     * ork::MeshBuffers (via an ork::AttributeBuffer) to
     * directly use the content of this texture as a vertex array.
     *
     * @param name a gpu storage symbolic name (see #initGpuStorage()).
     * @return the GPU buffer corresponding to this GPU storage.
     */
    ptr<TextureBuffer> getGpuStorage(const std::string &name);

    /**
     * Returns the current number of %particles stored in this storage.
     */
    int getParticlesCount();

    /**
     * Returns an iterator to the first particle currently stored in this
     * storage. Provided the returned iterator is only used in a sequential
     * way, %particles can be added and removed while using this iterator. The
     * iterator will only iterate through the %particles that existed when
     * this method was called, regardless of the creation and destruction
     * of %particles while iterating. This is no longer true, however, if
     * several iterators are used at the same time and if several of them
     * create and destroy %particles while iterating.
     *
     * @return the list of %particles currently stored in this storage.
     */
    std::vector<Particle*>::iterator getParticles();

    /**
     * Returns an iterator just past the last stored particle.
     */
    inline std::vector<Particle*>::iterator end()
    {
        return freeAndAllocatedParticles.end();
    }

    /**
     * Returns the index of the given particle.
     *
     * @return the index of the given particle. This index is between 0 and
     *      #getCapacity() (excluded).
     */
    int getParticleIndex(Particle *p);

    /**
     * Returns a new uninitialized particle.
     *
     * @return a new particle, or NULL if the current number of %particles
     *      stored in this storage is equal to its maximum capacity.
     */
    Particle *newParticle();

    /**
     * Deletes the given particle.
     *
     * @param p a particle created by this particle storage.
     */
    void deleteParticle(Particle *p);

    /**
     * Deletes the entire list of particles.
     */
    void clear();

protected:
    /**
     * Creates a new uninitialized ParticleStorage.
     */
    ParticleStorage();

    /**
     * Initializes this ParticleStorage.
     *
     * See #ParticleStorage
     */
    void init(int capacity, bool pack);

    void swap(ptr<ParticleStorage> p);

private:
    /**
     * The size in bytes, on CPU, of each particle.
     */
    int particleSize;

    /**
     * The maximum number of %particles that can be stored in this storage.
     */
    int capacity;

    /**
     * The number of available slots in #particles to store new %particles.
     */
    int available;

    /**
     * The %particles data on CPU.
     * This memory chunk is of size capacity * particleSize bytes.
     */
    void *particles;

    /**
     * The %particles data on GPU, in the form of texture buffers.
     * See #initGPUStorage().
     */
    std::map<std::string, ptr<TextureBuffer> > gpuTextures;

    /**
     * Pointers to the free and allocated %particles in #particles. This vector
     * is of size #capacity. Its first #available elements contain pointers to
     * the free slots in #particles, arranged in a heap data structure (if
     * #pack is true). The remaining elements contain pointers to the
     * currently allocated particles.
     */
    std::vector<Particle*> freeAndAllocatedParticles;

    /**
     * True to ensure that new %particles are always created with the minimum
     * available index. See #ParticleStorage.
     */
    bool pack;
};

}

#endif
