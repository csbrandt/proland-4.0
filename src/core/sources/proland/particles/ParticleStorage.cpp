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

#include "proland/particles/ParticleStorage.h"

#include <algorithm>
#include "pmath.h"

#include "ork/resource/ResourceTemplate.h"

using namespace std;
using namespace ork;

namespace proland
{

ParticleStorage::ParticleStorage(int capacity, bool pack) : Object("ParticleStorage")
{
    init(capacity, pack);
}

ParticleStorage::ParticleStorage() : Object("ParticleStorage")
{
}

void ParticleStorage::init(int capacity, bool pack)
{
    this->capacity = capacity;
    this->available = capacity;
    this->particles = NULL;
    this->pack = pack;
}

ParticleStorage::~ParticleStorage()
{
    if (particles != NULL) {
        delete[] ((unsigned char*) particles);
    }
}

void ParticleStorage::initCpuStorage(int particleSize)
{
    assert(particleSize > 0);
    // we reserve additional space in each particle to store the index in
    // #freeAndAllocatedParticles of the element that points to this particle
    particleSize += sizeof(int);

    if (particles != NULL) {
        assert(particleSize == this->particleSize);
        return;
    }
    this->particleSize = particleSize;
    unsigned char *data = new unsigned char[capacity * particleSize];
    for (int i = 0; i < capacity; ++i) {
        freeAndAllocatedParticles.push_back((Particle*) (data + i * particleSize));
    }
    if (pack) {
        make_heap(freeAndAllocatedParticles.begin(), freeAndAllocatedParticles.end(), greater<Particle*>());
    }
    particles = (void*) data;
}

void ParticleStorage::initGpuStorage(const string &name, TextureInternalFormat f, int components)
{
    int pixelSize;
    switch(f) {
    case R8:
        pixelSize = 1;
        break;
    case RG8:
    case R16F:
        pixelSize = 2;
        break;
    case RGBA8:
    case RG16F:
    case R32F:
        pixelSize = 4;
        break;
    case RG32F:
    case RGBA16F:
        pixelSize = 8;
        break;
    case RGBA32F:
        pixelSize = 16;
        break;
    default:
        // unsupported formats
        throw exception();
    }
    ptr<GPUBuffer> b = new GPUBuffer();
    b->setData(capacity * components * pixelSize, NULL, STREAM_DRAW);
    ptr<TextureBuffer> t = new TextureBuffer(f, b);
    gpuTextures[name] = t;
}

int ParticleStorage::getCapacity()
{
    return capacity;
}

ptr<TextureBuffer> ParticleStorage::getGpuStorage(const string &name)
{
    map< string, ptr<TextureBuffer> >::iterator i = gpuTextures.find(name);
    if (i != gpuTextures.end()) {
        return i->second;
    }
    return NULL;
}

int ParticleStorage::getParticlesCount()
{
    return capacity - available;
}

vector<ParticleStorage::Particle*>::iterator ParticleStorage::getParticles()
{
    return freeAndAllocatedParticles.begin() + available;
}

int ParticleStorage::getParticleIndex(Particle *p)
{
    return (((unsigned char*) p) - ((unsigned char*) particles)) / particleSize;
}

ParticleStorage::Particle *ParticleStorage::newParticle()
{
    assert(particles != NULL);
    if (available == 0) {
        return NULL;
    }
    if (pack) {
        pop_heap(freeAndAllocatedParticles.begin(), freeAndAllocatedParticles.begin() + available, greater<Particle*>());
    }
    Particle *p = freeAndAllocatedParticles[--available];
    *((int*) (((unsigned char*) p) + particleSize - sizeof(int))) = available;
    return p;
}

void ParticleStorage::deleteParticle(Particle *p)
{
    assert(particles != NULL && p != NULL && available < capacity);
    Particle *q = freeAndAllocatedParticles[available];
    int index = *((int*) (((unsigned char*) p) + particleSize - sizeof(int)));
    *((int*) (((unsigned char*) q) + particleSize - sizeof(int))) = index;
    freeAndAllocatedParticles[index] = q;
    freeAndAllocatedParticles[available++] = p;
    if (pack) {
        push_heap(freeAndAllocatedParticles.begin(), freeAndAllocatedParticles.begin() + available, greater<Particle*>());
    }
}

void ParticleStorage::clear()
{
    available = capacity;
}

void ParticleStorage::swap(ptr<ParticleStorage> p)
{
    std::swap(particleSize, p->particleSize);
    std::swap(capacity, p->capacity);
    std::swap(available, p->available);
    std::swap(particles, p->particles);
    std::swap(gpuTextures, p->gpuTextures);
    std::swap(freeAndAllocatedParticles, p->freeAndAllocatedParticles);
    std::swap(pack, p->pack);
}

class ParticleStorageResource : public ResourceTemplate<50, ParticleStorage>
{
public:
    ParticleStorageResource(ptr<ResourceManager> manager, const string &name, ptr<ResourceDescriptor> desc, const TiXmlElement *e = NULL) :
        ResourceTemplate<50, ParticleStorage>(manager, name, desc)
    {
        e = e == NULL ? desc->descriptor : e;
        checkParameters(desc, e, "name,capacity,pack,");

        bool pack = true;
        int capacity;
        getIntParameter(desc, e, "capacity", &capacity);

        if (e->Attribute("pack") != NULL) {
            pack = strcmp(e->Attribute("pack"), "true") == 0;
        }

        init(capacity, pack);

    }
};

extern const char particleStorage[] = "particleStorage";

static ResourceFactory::Type<particleStorage, ParticleStorageResource> ParticleStorageType;

}
