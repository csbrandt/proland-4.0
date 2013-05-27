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

#include "proland/particles/LifeCycleParticleLayer.h"

#include "ork/resource/ResourceTemplate.h"

using namespace std;
using namespace ork;

namespace proland
{

LifeCycleParticleLayer::LifeCycleParticleLayer(float fadeInDelay, float activeDelay, float fadeOutDelay) :
    ParticleLayer("LifeCycleParticleLayer", sizeof(LifeCycleParticle))
{
    init(fadeInDelay, activeDelay, fadeOutDelay);
}

LifeCycleParticleLayer::LifeCycleParticleLayer() :
    ParticleLayer("LifeCycleParticleLayer", sizeof(LifeCycleParticle))
{
}

void LifeCycleParticleLayer::init(float fadeInDelay, float activeDelay, float fadeOutDelay)
{
    this->fadeInDelay = fadeInDelay;
    this->activeDelay = activeDelay;
    this->fadeOutDelay = fadeOutDelay;
    this->time = 0.0f;
}

LifeCycleParticleLayer::~LifeCycleParticleLayer()
{
}

bool LifeCycleParticleLayer::isFadingIn(ParticleStorage::Particle *p)
{
    float age = time - getBirthDate(p);
    return age < fadeInDelay;
}

bool LifeCycleParticleLayer::isActive(ParticleStorage::Particle *p)
{
    float age = time - getBirthDate(p);
    return age >= fadeInDelay && age < fadeInDelay + activeDelay;
}

bool LifeCycleParticleLayer::isFadingOut(ParticleStorage::Particle *p)
{
    float age = time - getBirthDate(p);
    return age >= fadeInDelay + activeDelay;
}

void LifeCycleParticleLayer::setFadingOut(ParticleStorage::Particle *p)
{
    // making sure that the intensity won't pop to 1.0 when deleting a fading in particle
    float i = getIntensity(p);
    if (!isFadingOut(p)) {
        getLifeCycle(p)->birthDate = time - (fadeInDelay + activeDelay + (1.0f - i) * fadeOutDelay);
    }
}

float LifeCycleParticleLayer::getFadeInDelay() const
{
    return fadeInDelay;
}

float LifeCycleParticleLayer::getActiveDelay() const
{
    return activeDelay;
}

float LifeCycleParticleLayer::getFadeOutDelay() const
{
    return fadeOutDelay;
}

void LifeCycleParticleLayer::setFadeInDelay(float delay)
{
    fadeInDelay = delay;
}

void LifeCycleParticleLayer::setActiveDelay(float delay)
{
    activeDelay = delay;
}

void LifeCycleParticleLayer::setFadeOutDelay(float delay)
{
    fadeOutDelay = delay;
}

void LifeCycleParticleLayer::killParticle(ParticleStorage::Particle *p)
{
    float minBirthDate = time - (fadeInDelay + activeDelay + fadeOutDelay);
    getLifeCycle(p)->birthDate = minBirthDate - 1.0f;
}

float LifeCycleParticleLayer::getIntensity(ParticleStorage::Particle *p)
{
    float t = time - getBirthDate(p);
    if (t < fadeInDelay) {
        return t / fadeInDelay;
    } else {
        t -= fadeInDelay;
        if (t < activeDelay) {
            return 1.0f;
        } else {
            t -= activeDelay;
            return max(0.0f, 1.0f - t / fadeOutDelay);
        }
    }
}

void LifeCycleParticleLayer::moveParticles(double dt)
{
    time += dt;
}

void LifeCycleParticleLayer::removeOldParticles()
{
    // all particles with a birth date less than minBirthDate must be deleted
    float minBirthDate = time - (fadeInDelay + activeDelay + fadeOutDelay);

    ptr<ParticleStorage> s = getOwner()->getStorage();
    vector<ParticleStorage::Particle*>::iterator i = s->getParticles();
    vector<ParticleStorage::Particle*>::iterator end = s->end();
    while (i != end) {
        ParticleStorage::Particle *p = *i;
        if (getBirthDate(p) <= minBirthDate) {
            s->deleteParticle(p);
        }
        ++i;
    }
}

void LifeCycleParticleLayer::initParticle(ParticleStorage::Particle *p)
{
    getLifeCycle(p)->birthDate = time;
}

void LifeCycleParticleLayer::swap(ptr<LifeCycleParticleLayer> p)
{
    ParticleLayer::swap(p);
    std::swap(fadeInDelay, p->fadeInDelay);
    std::swap(fadeOutDelay, p->fadeOutDelay);
    std::swap(activeDelay, p->activeDelay);
    std::swap(time, p->time);
}

class LifeCycleParticleLayerResource : public ResourceTemplate<50, LifeCycleParticleLayer>
{
public:
    LifeCycleParticleLayerResource(ptr<ResourceManager> manager, const string &name, ptr<ResourceDescriptor> desc, const TiXmlElement *e = NULL) :
        ResourceTemplate<50, LifeCycleParticleLayer>(manager, name, desc)
    {
        e = e == NULL ? desc->descriptor : e;
        checkParameters(desc, e, "name,fadeInDelay,fadeOutDelay,activeDelay,unit,");

        float fadeInDelay = 5.0f;
        float fadeOutDelay = 5.0f;
        float activeDelay = 30.0f;

        float unit = 1000000.0f; // delays are converted into seconds.

        if (e->Attribute("unit") != NULL) {
            if (strcmp(e->Attribute("unit"), "s") == 0) {
                unit = 1000000.0f;
            } else if (strcmp(e->Attribute("unit"), "ms") == 0) {
                unit = 1000.0f;
            } else if (strcmp(e->Attribute("unit"), "us") == 0) {
                unit = 10.f;
            }
        }

        //delays are taken in seconds
        getFloatParameter(desc, e, "fadeInDelay", &fadeInDelay);
        getFloatParameter(desc, e, "fadeOutDelay", &fadeOutDelay);
        getFloatParameter(desc, e, "activeDelay", &activeDelay);

        init(fadeInDelay * unit, activeDelay * unit, fadeOutDelay * unit);
    }

    virtual bool prepareUpdate()
    {
        oldValue = NULL;
        newDesc = NULL;

        return true;
    }
};

extern const char lifeCycleParticleLayer[] = "lifeCycleParticleLayer";

static ResourceFactory::Type<lifeCycleParticleLayer, LifeCycleParticleLayerResource> LifeCycleParticleLayerType;

}
