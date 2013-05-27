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

#include "proland/producer/CPUTileStorage.h"

#include "ork/resource/ResourceTemplate.h"

using namespace std;
using namespace ork;

namespace proland
{

template<class T>
class CPUTileStorageResource : public ResourceTemplate<0, CPUTileStorage<T> >
{
public:
    CPUTileStorageResource(ptr<ResourceManager> manager, const string &name, ptr<ResourceDescriptor> desc, const TiXmlElement *e = NULL) :
        ResourceTemplate<0, CPUTileStorage<T> >(manager, name, desc)
    {
        e = e == NULL ? desc->descriptor : e;
        int tileSize;
        int channels;
        int capacity;
        Resource::checkParameters(desc, e, "name,tileSize,channels,capacity,");
        Resource::getIntParameter(desc, e, "tileSize", &tileSize);
        Resource::getIntParameter(desc, e, "channels", &channels);
        Resource::getIntParameter(desc, e, "capacity", &capacity);
        CPUTileStorage<T>::init(tileSize, channels, capacity);
    }
};

extern const char cpuByteTileStorage[] = "cpuByteTileStorage";

extern const char cpuFloatTileStorage[] = "cpuFloatTileStorage";

static ResourceFactory::Type<cpuByteTileStorage, CPUTileStorageResource<unsigned char> > CPUByteTileStorageType;

static ResourceFactory::Type<cpuFloatTileStorage, CPUTileStorageResource<float> > CPUFloatTileStorageType;

}
