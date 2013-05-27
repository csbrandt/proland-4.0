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

#include "proland/particles/ParticleLayer.h"

namespace proland
{

ParticleLayer::ParticleLayer(const char *type, int particleSize) :
    Object(type)
{
    init(particleSize);
}

ParticleLayer::~ParticleLayer()
{
}

void ParticleLayer::init(int particleSize)
{
    this->owner = NULL;
    // padding to 32 bits words
    this->size = (particleSize / 8) * 8 + 8 * (particleSize % 8 != 0);
    this->offset = 0;
    this->enabled = true;
}

ParticleProducer *ParticleLayer::getOwner()
{
    return owner;
}

bool ParticleLayer::isEnabled()
{
    return enabled;
}

void ParticleLayer::setIsEnabled(bool enable)
{
    this->enabled = enable;
}

int ParticleLayer::getParticleSize()
{
    return size;
}

void ParticleLayer::moveParticles(double dt)
{
}

void ParticleLayer::removeOldParticles()
{
}

void ParticleLayer::addNewParticles()
{
}

void ParticleLayer::initialize()
{
}

void ParticleLayer::initParticle(ParticleStorage::Particle *p)
{
}

void ParticleLayer::swap(ptr<ParticleLayer> p)
{
    std::swap(owner, p->owner);
    std::swap(offset, p->offset);
    std::swap(enabled, p->enabled);
}

}
