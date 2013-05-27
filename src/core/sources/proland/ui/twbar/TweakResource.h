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

#ifndef _PROLAND_TWEAKRESOURCE_H_
#define _PROLAND_TWEAKRESOURCE_H_

#include <vector>
#include "tinyxml/tinyxml.h"

#include "ork/resource/ResourceManager.h"
#include "proland/ui/twbar/TweakBarHandler.h"

using namespace ork;

namespace proland
{

/**
 * A TweakBarHandler to %edit resources.
 * @ingroup twbar
 * @authors Eric Bruneton, Antoine Begault
 */
PROLAND_API class TweakResource : public TweakBarHandler
{
public:
    /**
     * Abstract data class used for tweak bar callbacks.
     */
    class Data
    {
    public:
        /**
         * Creates a new tweak bar data.
         */
        Data();

        /**
         * Deletes this tweak bar data.
         */
        virtual ~Data();
    };

    /**
     * Creates a new TweakResource.
     *
     * @param name the name of this TweakResource.
     * @param manager a resource manager to load the resources defined in e.
     * @param e an XML description of the controls that must be provided by
     *      this TweakResource.
     */
    TweakResource(std::string name, ptr<ResourceManager> manager, const TiXmlElement *e);

    /**
     * Deletes this TweakResource.
     */
    virtual ~TweakResource();

    virtual void updateBar(TwBar *bar);

protected:
    /**
     * Creates an uninitialized TweakResource.
     */
    TweakResource();

    /**
     * Initializes this TweakResource.
     * See #TweakResource.
     */
    void init(std::string name, ptr<ResourceManager> manager, const TiXmlElement *e);

    void swap(ptr<TweakResource> p);

private:
    /**
     * A resource manager to load the resources defined in #e.
     */
    ptr<ResourceManager> manager;

    /**
     * An XML description of the controls that must be provided by this
     * TweakResource.
     */
    TiXmlElement *e;

    /**
     * The tweak bar data managed by this TweakResource.
     */
    std::vector<Data*> datas;

    void clearData();

    void setParam(TwBar *bar, const TiXmlElement *el, const char* param);
};

}

#endif
