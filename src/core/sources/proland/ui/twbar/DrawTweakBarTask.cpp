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

#include "proland/ui/twbar/DrawTweakBarTask.h"

#include "AntTweakBar.h"

#include "ork/render/FrameBuffer.h"
#include "ork/resource/ResourceTemplate.h"
#include "ork/scenegraph/SceneManager.h"

using namespace std;
using namespace ork;

namespace proland
{

DrawTweakBarTask::DrawTweakBarTask() : AbstractTask("DrawTweakBarTask")
{
}

DrawTweakBarTask::~DrawTweakBarTask()
{
}

ptr<Task> DrawTweakBarTask::getTask(ptr<Object> context)
{
    return new Impl();
}

void DrawTweakBarTask::swap(ptr<DrawTweakBarTask> a)
{
}

DrawTweakBarTask::Impl::Impl() :
    Task("DrawTweakBar", true, 0)
{
}

DrawTweakBarTask::Impl::~Impl()
{
}

bool DrawTweakBarTask::Impl::run()
{
    static bool initialized = false;
    if (!initialized) {
        vec4<GLint> vp = SceneManager::getCurrentFrameBuffer()->getViewport();
        TwWindowSize(vp.z - vp.x, vp.w - vp.y);
        initialized = true;
    }
    FrameBuffer::resetAllStates();
    return 0 != TwDraw();
}

class DrawTweakBarTaskResource : public ResourceTemplate<0, DrawTweakBarTask>
{
public:
    DrawTweakBarTaskResource(ptr<ResourceManager> manager, const string &name, ptr<ResourceDescriptor> desc, const TiXmlElement *e = NULL) :
        ResourceTemplate<0, DrawTweakBarTask>(manager, name, desc)
    {
    }
};

extern const char drawTweakBar[] = "drawTweakBar";

static ResourceFactory::Type<drawTweakBar, DrawTweakBarTaskResource> DrawTweakBarTaskType;

}
