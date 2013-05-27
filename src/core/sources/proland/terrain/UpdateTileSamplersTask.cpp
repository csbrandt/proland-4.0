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

#include "proland/terrain/UpdateTileSamplersTask.h"

#include "ork/resource/ResourceTemplate.h"
#include "proland/terrain/TileSampler.h"

using namespace std;

namespace proland
{

UpdateTileSamplersTask::UpdateTileSamplersTask() : AbstractTask("UpdateTileSamplersTask")
{
}

UpdateTileSamplersTask::UpdateTileSamplersTask(const QualifiedName &terrain) :
    AbstractTask("UpdateTileSamplersTask")
{
    init(terrain);
}

void UpdateTileSamplersTask::init(const QualifiedName &terrain)
{
    this->terrain = terrain;
}

UpdateTileSamplersTask::~UpdateTileSamplersTask()
{
}

ptr<Task> UpdateTileSamplersTask::getTask(ptr<Object> context)
{
    ptr<SceneNode> n = context.cast<Method>()->getOwner();
    ptr<SceneNode> target = terrain.getTarget(n);
    ptr<TerrainNode> t = NULL;
    if (target == NULL) {
        t = n->getOwner()->getResourceManager()->loadResource(terrain.name).cast<TerrainNode>();
    } else {
        t = target->getField(terrain.name).cast<TerrainNode>();
    }
    if (t == NULL) {
        if (Logger::ERROR_LOGGER != NULL) {
            Logger::ERROR_LOGGER->log("TERRAIN", "UpdateTileSamplers : cannot find terrain '" + terrain.target + "." + terrain.name + "'");
        }
        throw exception();
    }
    ptr<TaskGraph> result = new TaskGraph();
    SceneNode::FieldIterator i = n->getFields();
    while (i.hasNext()) {
        ptr<TileSampler> u = i.next().cast<TileSampler>();
        if (u != NULL) {
            ptr<Task> ut = u->update(n->getOwner(), t->root);
            if (ut.cast<TaskGraph>() == NULL || !ut.cast<TaskGraph>()->isEmpty()) {
                result->addTask(ut);
            }
        }
    }
    return result;
}

void UpdateTileSamplersTask::swap(ptr<UpdateTileSamplersTask> t)
{
    std::swap(*this, *t);
}

class UpdateTileSamplersTaskResource : public ResourceTemplate<40, UpdateTileSamplersTask>
{
public:
    UpdateTileSamplersTaskResource(ptr<ResourceManager> manager, const string &name, ptr<ResourceDescriptor> desc, const TiXmlElement *e = NULL) :
        ResourceTemplate<40, UpdateTileSamplersTask>(manager, name, desc)
    {
        e = e == NULL ? desc->descriptor : e;
        checkParameters(desc, e, "name,");
        string n = getParameter(desc, e, "name");
        init(QualifiedName(n));
    }
};

extern const char updateTileSamplers[] = "updateTileSamplers";

static ResourceFactory::Type<updateTileSamplers, UpdateTileSamplersTaskResource> UpdateTileSamplersTaskType;

}
