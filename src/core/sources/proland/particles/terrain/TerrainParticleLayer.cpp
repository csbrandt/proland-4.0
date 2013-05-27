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

#include "proland/particles/terrain/TerrainParticleLayer.h"

#include "ork/resource/ResourceTemplate.h"
#include "proland/producer/ObjectTileStorage.h"

using namespace std;
using namespace ork;

namespace proland
{

TerrainParticleLayer::TerrainParticleLayer(map<ptr<TileProducer>, TerrainInfo *> infos) :
    ParticleLayer("TerrainParticleLayer", sizeof(TerrainParticle))
{
    init(infos);
}

TerrainParticleLayer::TerrainParticleLayer() :
    ParticleLayer("TerrainParticleLayer", sizeof(TerrainParticle))
{
}

void TerrainParticleLayer::init(map<ptr<TileProducer>, TerrainInfo *> infos)
{
    this->infos = infos;
    this->lifeCycleLayer = NULL;
    this->screenLayer = NULL;
    this->worldLayer = NULL;
}

TerrainParticleLayer::~TerrainParticleLayer()
{
    for (map<ptr<TileProducer>, TerrainInfo*>::iterator i = infos.begin(); i != infos.end(); i++) {
        delete i->second;
    }
    infos.clear();
}

ptr<FlowTile> TerrainParticleLayer::findFlowTile(ptr<TileProducer> producer, TileCache::Tile *t, vec3d &pos)
{
    if (t == NULL) {
        return NULL;
    }
    if (t->task->isDone() == false) {
        return NULL;
    }
    float z = producer->getRootQuadSize();
    if (abs(pos.x) > z / 2 || abs(pos.y) > z / 2) {
        return NULL;
    }
    int width = 1 << t->level;
    float tileWidth = z / width;
    float px = t->tx * tileWidth - z / 2;
    float py = t->ty * tileWidth - z / 2;
    int tx = t->tx * 2;
    int ty = t->ty * 2;
    if (pos.x >= px + tileWidth / 2) {
        tx++;
    }
    if (pos.y >= py + tileWidth / 2) {
        ty++;
    }

    TileCache::Tile *child = producer->findTile(t->level + 1, tx, ty);

    if (child != NULL && child->task->isDone() && child->getData() != NULL) {
        return findFlowTile(producer, child, pos);
    }
    ObjectTileStorage::ObjectSlot* objectData = dynamic_cast<ObjectTileStorage::ObjectSlot*>(t->getData());
    assert(objectData != NULL);
    return objectData->data.cast<FlowTile>();
}

ptr<FlowTile> TerrainParticleLayer::getFlowTile(TerrainParticle *t)
{
    TileCache::Tile *tile = t->producer->findTile(0, 0, 0);
    assert(tile != NULL);
    return findFlowTile(t->producer, tile, t->terrainPos);
}

void TerrainParticleLayer::moveParticles(double dt)
{
    if (worldLayer->isPaused()) {
        return;
    }
    if (infos.size() > 0) {
        vec2d newPos;
        vec2d oldVelocity;
        int type;
        float terrainSize = 0;
        double DT = dt * worldLayer->getSpeedFactor() * 1e-6;
        ptr<ParticleStorage> storage = getOwner()->getStorage();
        vector<ParticleStorage::Particle*>::iterator i = storage->getParticles();
        vector<ParticleStorage::Particle*>::iterator end = storage->end();
        while (i != end) {
            ParticleStorage::Particle *p = *i;
            ++i;
            ScreenParticleLayer::ScreenParticle *s = screenLayer->getScreenParticle(p);
            WorldParticleLayer::WorldParticle *w = worldLayer->getWorldParticle(p);
            TerrainParticle *t = getTerrainParticle(p);
            if (t->producer == NULL) {
                getFlowProducer(p);
            }
            if (t->terrainPos.x == UNINITIALIZED || t->terrainPos.y == UNINITIALIZED || t->terrainPos.z == UNINITIALIZED) {
                // if not inside a terrain, just skip the particle
                continue;
            }
            assert(t->producer != NULL);
            ptr<FlowTile> flowData = getFlowTile(t);
            if (flowData != NULL) {
                newPos = t->terrainPos.xy();
                oldVelocity = t->terrainVelocity;
                if (t->status == FlowTile::INSIDE || t->status == FlowTile::UNKNOWN) {
                    flowData->getVelocity(newPos, t->terrainVelocity, type);
                    if (type == FlowTile::INSIDE) {
                        t->status = FlowTile::INSIDE;
                    } else { //outside
                        if (t->firstVelocityQuery) {
                            t->status = FlowTile::OUTSIDE;
                            int neighborNum;
                            ScreenParticleLayer::ScreenParticle** neighbors = screenLayer->getNeighbors(s, neighborNum);
                            for (int j = 0; j < neighborNum; j++) {
                                if (getTerrainParticle(screenLayer->getParticle(neighbors[j]))->status == FlowTile::INSIDE) {
                                    t->status = FlowTile::NEAR;
                                    lifeCycleLayer->killParticle(p);
                                    break;
                                }
                            }
                            t->terrainVelocity = vec2d(0.f, 0.f);
                        } else {
                            t->status = FlowTile::LEAVING;
                            t->terrainVelocity = oldVelocity;
                        }
                    }
                } else if (t->status == FlowTile::LEAVING) {
                    flowData->getVelocity(newPos, t->terrainVelocity, type);
                    if (type == FlowTile::INSIDE) {
                        t->status = FlowTile::INSIDE;
                    } else {
                        bool neighborInside  = false;
                        int neighborNum;
                        ScreenParticleLayer::ScreenParticle** neighbors = screenLayer->getNeighbors(s, neighborNum);
                        for (int j = 0; j < neighborNum; j++) {
                            if (getTerrainParticle(screenLayer->getParticle(neighbors[j]))->status == FlowTile::INSIDE) {
                                neighborInside = true;
                                break;
                            }
                        }
                        if (neighborInside) {
                            t->terrainVelocity = oldVelocity;
                        } else {
                            t->terrainVelocity = vec2d(0.f, 0.f);
                            t->status = FlowTile::OUTSIDE;
                        }
                    }
                } else if (t->status == FlowTile::OUTSIDE) {
                    int neighborNum;
                    ScreenParticleLayer::ScreenParticle** neighbors = screenLayer->getNeighbors(s, neighborNum);
                    for (int j = 0; j < neighborNum; j++) {
                        if (getTerrainParticle(screenLayer->getParticle(neighbors[j]))->status == FlowTile::INSIDE) {
                            t->status = FlowTile::NEAR;
                            lifeCycleLayer->killParticle(p);
                            break;
                        }
                    }
                }
                terrainSize = t->producer->getRootQuadSize();
                t->firstVelocityQuery = false;
                if (isFinite(t->terrainVelocity.x + t->terrainVelocity.y)) {
                    newPos += t->terrainVelocity * DT;
                    t->terrainPos = vec3d(newPos.x, newPos.y, t->terrainPos.z);
                }
                if (abs(t->terrainPos.x) > terrainSize || abs(t->terrainPos.y) > terrainSize) {
                    // out of current terrain -> we will force to recompte the terrain on which the particle is
                    w->worldPos = vec3d(UNINITIALIZED, UNINITIALIZED, UNINITIALIZED);
                    w->worldVelocity = vec3f(UNINITIALIZED, UNINITIALIZED, UNINITIALIZED);
                    t->terrainPos = vec3d(UNINITIALIZED, UNINITIALIZED, UNINITIALIZED);
                    t->terrainVelocity = vec2d(UNINITIALIZED, UNINITIALIZED);
                    t->producer = NULL;
                    t->terrainId = -1;
                } else {
                    // TODO : How to update altitude???
                    TerrainInfo *n = infos[t->producer];
                    vec4f v = (n->node->getLocalToWorld() * n->terrain->deform->localToDeformed(t->terrainPos.cast<double>())).cast<float>();
                    w->worldPos = (v.xyz() / v.w).cast<double>();
                }
                if (!isFinite(w->worldPos.x + w->worldPos.y + w->worldPos.z + t->terrainPos.x + t->terrainPos.y + t->terrainPos.z)) {
                    printf("ERROR :  : %f:%f:%f -> [%f:%f * %f] %f:%f -> %d:%d\n", t->terrainPos.x, t->terrainPos.y, t->terrainPos.z, t->terrainVelocity.x, t->terrainVelocity.y, DT,s->screenPos.x, s->screenPos.y, t->status, lifeCycleLayer->isFadingOut(p));
                }
            }
        }
    }
}

ptr<TileProducer> TerrainParticleLayer::getFlowProducer(ParticleStorage::Particle *p)
{
    mat4d mat;
    vec4d v;
    vec3d pos;
    WorldParticleLayer::WorldParticle* w = worldLayer->getWorldParticle(p);
    TerrainParticle *t = getTerrainParticle(p);
    if (!isFinite(w->worldPos.x + w->worldPos.y + w->worldPos.z)) {
        t->producer = NULL;
        t->terrainId = -1;
        t->terrainPos = vec3d(UNINITIALIZED, UNINITIALIZED, UNINITIALIZED);
        w->worldPos = vec3d(UNINITIALIZED, UNINITIALIZED, UNINITIALIZED);
        return NULL;
    }

    for (map<ptr<TileProducer>, TerrainInfo*>::iterator i = infos.begin(); i != infos.end(); i++) {
        v = i->second->node->getWorldToLocal() * vec4d(w->worldPos, 1.0);//i->second->node->getLocalToScreen().inverse() * sPos;
        pos = v.xyz() / v.w;
        if (!i->second->node->getLocalBounds().contains(pos)) {
            continue;
        }
        t->terrainPos = i->second->terrain->deform->deformedToLocal(pos);
        t->producer = i->first.get();
        t->terrainId = i->second->id;
        return t->producer;
    }
    t->producer = NULL;
    t->terrainId = -1;
    t->terrainPos = vec3d(UNINITIALIZED, UNINITIALIZED, UNINITIALIZED);
    w->worldPos = vec3d(UNINITIALIZED, UNINITIALIZED, UNINITIALIZED);
    return NULL;
}

void TerrainParticleLayer::initialize()
{
    lifeCycleLayer = getOwner()->getLayer<LifeCycleParticleLayer>();
    screenLayer = getOwner()->getLayer<ScreenParticleLayer>();
    worldLayer = getOwner()->getLayer<WorldParticleLayer>();
    assert(lifeCycleLayer != NULL);
    assert(screenLayer != NULL);
    assert(worldLayer != NULL);
}

void TerrainParticleLayer::initParticle(ParticleStorage::Particle *p)
{
    TerrainParticle *t = getTerrainParticle(p);
    t->producer = NULL;
    t->terrainId = -1;
    t->terrainPos = vec3d(UNINITIALIZED, UNINITIALIZED, UNINITIALIZED);
    t->terrainVelocity = vec2d(UNINITIALIZED, UNINITIALIZED);
    t->status = FlowTile::UNKNOWN;
    t->firstVelocityQuery = true;
}

void TerrainParticleLayer::getReferencedProducers(vector< ptr<TileProducer> > &producers) const
{
    for (map<ptr<TileProducer>, TerrainInfo *>::const_iterator i = infos.begin(); i != infos.end(); i++) {
        producers.push_back(i->first);
    }
}

void TerrainParticleLayer::swap(ptr<TerrainParticleLayer> p)
{
    ParticleLayer::swap(p);
    std::swap(lifeCycleLayer, p->lifeCycleLayer);
    std::swap(screenLayer, p->screenLayer);
    std::swap(worldLayer, p->worldLayer);
    std::swap(infos, p->infos);
}

class TerrainParticleLayerResource : public ResourceTemplate<50, TerrainParticleLayer>
{
public:
    TerrainParticleLayerResource(ptr<ResourceManager> manager, const string &name, ptr<ResourceDescriptor> desc, const TiXmlElement *e = NULL) :
        ResourceTemplate<50, TerrainParticleLayer>(manager, name, desc)
    {
        e = e == NULL ? desc->descriptor : e;
        checkParameters(desc, e, "name,terrains,");

        map<ptr<TileProducer>, TerrainInfo *> infos;

        if (e->Attribute("terrains") != NULL) {
            string names = getParameter(desc, e, "terrains") + ",";
            string::size_type start = 0, s;
            string::size_type index;
            while ((index = names.find(',', start)) != string::npos) {
                string name = names.substr(start, index - start);
                s = name.find('/', 0);
                ptr<SceneNode> n = manager->loadResource(name.substr(0, s)).cast<SceneNode>();
                ptr<TileProducer> flow = n->getField(name.substr(s + 1, name.size())).cast<TileProducer>();
                infos.insert(make_pair(flow, new TerrainInfo(n, infos.size())));
                start = index + 1;
            }
        }
        init(infos);
    }

    virtual bool prepareUpdate()
    {
        oldValue = NULL;
        newDesc = NULL;

        return true;
    }
};

extern const char terrainParticleLayer[] = "terrainParticleLayer";

static ResourceFactory::Type<terrainParticleLayer, TerrainParticleLayerResource> TerrainParticleLayerType;

}
