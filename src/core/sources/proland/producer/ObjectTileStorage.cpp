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

#include "proland/producer/ObjectTileStorage.h"

#include "ork/resource/ResourceTemplate.h"

using namespace std;
using namespace ork;

namespace proland
{

ObjectTileStorage::ObjectSlot::ObjectSlot(TileStorage *owner) : Slot(owner)
{
    data = NULL;
}

ObjectTileStorage::ObjectSlot::~ObjectSlot()
{
    data = NULL;
}

ObjectTileStorage::ObjectTileStorage(int capacity) : TileStorage()
{
    init(capacity);
}

ObjectTileStorage::ObjectTileStorage() : TileStorage()
{
}

void ObjectTileStorage::init(int capacity)
{
    TileStorage::init(0, capacity);
    for (int i = 0; i < capacity; ++i) {
        freeSlots.push_back(new ObjectSlot(this));
    }
}

ObjectTileStorage::~ObjectTileStorage()
{
}

void ObjectTileStorage::swap(ptr<ObjectTileStorage> t)
{
}

class ObjectTileStorageResource : public ResourceTemplate<0, ObjectTileStorage>
{

public:
    ObjectTileStorageResource(ptr<ResourceManager> manager, const string &name, ptr<ResourceDescriptor> desc,
            const TiXmlElement *e = NULL) :
        ResourceTemplate<0, ObjectTileStorage> (manager, name, desc)
    {
        e = e == NULL ? desc->descriptor : e;
        int capacity;
        Resource::checkParameters(desc, e, "name,capacity,");
        Resource::getIntParameter(desc, e, "capacity", &capacity);
        ObjectTileStorage::init(capacity);
    }
};

extern const char objectTileStorage[] = "objectTileStorage";

static ResourceFactory::Type<objectTileStorage, ObjectTileStorageResource> ObjectTileStorageType;

}
