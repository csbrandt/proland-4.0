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

#include "proland/particles/WorldParticleLayer.h"

#include "ork/resource/ResourceTemplate.h"

using namespace std;
using namespace ork;

namespace proland
{

WorldParticleLayer::WorldParticleLayer(float speedFactor) :
    ParticleLayer("WorldParticleLayer", sizeof(WorldParticle))
{
    init(speedFactor);
}

WorldParticleLayer::WorldParticleLayer() :
    ParticleLayer("WorldParticleLayer", sizeof(WorldParticle))
{
}

void WorldParticleLayer::init(float speedFactor)
{
    this->speedFactor = speedFactor;
    this->paused = false;
}

WorldParticleLayer::~WorldParticleLayer()
{
}

float WorldParticleLayer::getSpeedFactor() const
{
    return speedFactor;
}

void WorldParticleLayer::setSpeedFactor(float speedFactor)
{
    this->speedFactor = speedFactor;
}

bool WorldParticleLayer::isPaused() const
{
    return paused;
}

void WorldParticleLayer::setPaused(bool paused)
{
    this->paused = paused;
}

void WorldParticleLayer::moveParticles(double dt)
{
    if (paused) {
        return;
    }
    float DT = dt * speedFactor * 1e-6;
    ptr<ParticleStorage> s = getOwner()->getStorage();
    vector<ParticleStorage::Particle*>::iterator i = s->getParticles();
    vector<ParticleStorage::Particle*>::iterator end = s->end();
    while (i != end) {
        WorldParticle *w = getWorldParticle(*i);
        if (w->worldPos.x != UNINITIALIZED && w->worldPos.y != UNINITIALIZED && w->worldPos.z != UNINITIALIZED && w->worldVelocity.x != UNINITIALIZED && w->worldVelocity.y != UNINITIALIZED && w->worldVelocity.z != UNINITIALIZED) {
            w->worldPos += w->worldVelocity.cast<double>() * DT;
        }
        ++i;
    }
}

void WorldParticleLayer::initParticle(ParticleStorage::Particle *p)
{
    WorldParticle *w = getWorldParticle(p);
    w->worldPos = vec3d(UNINITIALIZED, UNINITIALIZED, UNINITIALIZED);
    w->worldVelocity = vec3f(UNINITIALIZED, UNINITIALIZED, UNINITIALIZED);
}

void WorldParticleLayer::swap(ptr<WorldParticleLayer> p)
{
    ParticleLayer::swap(p);
    std::swap(speedFactor, p->speedFactor);
}

class WorldParticleLayerResource : public ResourceTemplate<50, WorldParticleLayer>
{
public:
    WorldParticleLayerResource(ptr<ResourceManager> manager, const string &name, ptr<ResourceDescriptor> desc, const TiXmlElement *e = NULL) :
        ResourceTemplate<50, WorldParticleLayer>(manager, name, desc)
    {
        e = e == NULL ? desc->descriptor : e;
        checkParameters(desc, e, "name,speedFactor,");

        float speedFactor = 1.0f;

        if (e->Attribute("speedFactor") != NULL) {
            getFloatParameter(desc, e, "speedFactor", &speedFactor);
        }

        init(speedFactor);
    }

    virtual bool prepareUpdate()
    {
        oldValue = NULL;
        newDesc = NULL;

        return true;
    }
};

extern const char worldParticleLayer[] = "worldParticleLayer";

static ResourceFactory::Type<worldParticleLayer, WorldParticleLayerResource> WorldParticleLayerType;

}
