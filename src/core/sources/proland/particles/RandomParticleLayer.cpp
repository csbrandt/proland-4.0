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

#include "proland/particles/RandomParticleLayer.h"

#include "ork/resource/ResourceTemplate.h"

using namespace std;
using namespace ork;

namespace proland
{

RandomParticleLayer::RandomParticleLayer(box3f bounds) :
    ParticleLayer("RandomParticleLayer", sizeof(RandomParticle))
{
    init(bounds);
}

RandomParticleLayer::RandomParticleLayer() :
    ParticleLayer("RandomParticleLayer", sizeof(RandomParticle))
{
}

void RandomParticleLayer::init(box3f bounds)
{
    this->bounds = bounds;
}

RandomParticleLayer::~RandomParticleLayer()
{
}

void RandomParticleLayer::addNewParticles()
{
}

void RandomParticleLayer::initParticle(ParticleStorage::Particle *p)
{
    RandomParticle *r = getRandomParticle(p);
    r->randomPos.x = bounds.xmin + (bounds.xmax - bounds.xmin) * (rand() / float(RAND_MAX));
    r->randomPos.y = bounds.ymin + (bounds.ymax - bounds.ymin) * (rand() / float(RAND_MAX));
    r->randomPos.z = bounds.zmin + (bounds.zmax - bounds.zmin) * (rand() / float(RAND_MAX));
}

void RandomParticleLayer::swap(ptr<RandomParticleLayer> p)
{
    ParticleLayer::swap(p);
    std::swap(bounds, p->bounds);
}

class RandomParticleLayerResource : public ResourceTemplate<50, RandomParticleLayer>
{
public:
    RandomParticleLayerResource(ptr<ResourceManager> manager, const string &name, ptr<ResourceDescriptor> desc, const TiXmlElement *e = NULL) :
        ResourceTemplate<50, RandomParticleLayer>(manager, name, desc)
    {
        e = e == NULL ? desc->descriptor : e;
        checkParameters(desc, e, "name,xmin,xmax,ymin,ymax,zmin,zmax,");

        box3f bounds = box3f(0.f, 1.f, 0.f, 1.f, 0.f, 1.f);

        if (e->Attribute("xmin") != NULL) {
            getFloatParameter(desc, e, "xmin", &(bounds.xmin));
        }
        if (e->Attribute("xmax") != NULL) {
            getFloatParameter(desc, e, "xmax", &(bounds.xmax));
        }
        if (e->Attribute("ymin") != NULL) {
            getFloatParameter(desc, e, "ymin", &(bounds.ymin));
        }
        if (e->Attribute("ymax") != NULL) {
            getFloatParameter(desc, e, "ymax", &(bounds.ymax));
        }
        if (e->Attribute("zmin") != NULL) {
            getFloatParameter(desc, e, "zmin", &(bounds.zmin));
        }
        if (e->Attribute("zmax") != NULL) {
            getFloatParameter(desc, e, "zmax", &(bounds.zmax));
        }

        init(bounds);
    }

    virtual bool prepareUpdate()
    {
        oldValue = NULL;
        newDesc = NULL;

        return true;
    }
};

extern const char randomParticleLayer[] = "randomParticleLayer";

static ResourceFactory::Type<randomParticleLayer, RandomParticleLayerResource> RandomParticleLayerType;

}
