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

#include "proland/ortho/EmptyOrthoLayer.h"

#include "ork/resource/ResourceTemplate.h"
#include "ork/render/FrameBuffer.h"
#include "ork/scenegraph/SceneManager.h"

using namespace std;
using namespace ork;

namespace proland
{

EmptyOrthoLayer::EmptyOrthoLayer() :
    TileLayer("EmptyOrthoLayer")
{
}

EmptyOrthoLayer::EmptyOrthoLayer(vec4f color) :
    TileLayer("EmptyOrthoLayer")
{
    TileLayer::init(false);
    init(color);
}

void EmptyOrthoLayer::init(vec4f color)
{
    this->color = color;
}

EmptyOrthoLayer::~EmptyOrthoLayer()
{
}

bool EmptyOrthoLayer::doCreateTile(int level, int tx, int ty, TileStorage::Slot *data)
{
    if (Logger::DEBUG_LOGGER != NULL) {
        ostringstream oss;
        oss << "Empty tile " << getProducerId() << " " << level << " " << tx << " " << ty;
        Logger::DEBUG_LOGGER->log("ORTHO", oss.str());
    }

    ptr<FrameBuffer> fb = SceneManager::getCurrentFrameBuffer();
    fb->setClearColor(color);
    fb->clear(true, false, false);
    fb->setClearColor(vec4f::ZERO);

    return true;
}

void EmptyOrthoLayer::swap(ptr<EmptyOrthoLayer> p)
{
    TileLayer::swap(p);
    std::swap(color, p->color);
}

class EmptyOrthoLayerResource : public ResourceTemplate<40, EmptyOrthoLayer>
{
public:
    EmptyOrthoLayerResource(ptr<ResourceManager> manager, const string &name, ptr<ResourceDescriptor> desc,
            const TiXmlElement *e = NULL) :
        ResourceTemplate<40, EmptyOrthoLayer> (manager, name, desc)
    {
        e = e == NULL ? desc->descriptor : e;
        vec4f color = vec4f(1.0f, 1.0f, 1.0f, 1.0f);

        checkParameters(desc, e, "name,color,");

        if (e->Attribute("color") != NULL) {
            string c = getParameter(desc, e, "color") + ",";
            string::size_type start = 0;
            string::size_type index;
            for (int i = 0; i < 3; i++) {
                index = c.find(',', start);
                color[i] = (float) atof(c.substr(start, index - start).c_str()) / 255;
                start = index + 1;
            }
        }

        init(color);
    }

    virtual bool prepareUpdate()
    {
        oldValue = NULL;
        newDesc = NULL;

        return true;
    }
};

extern const char emptyOrthoLayer[] = "emptyOrthoLayer";

static ResourceFactory::Type<emptyOrthoLayer, EmptyOrthoLayerResource> EmptyOrthoLayerType;

}
