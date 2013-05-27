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

#ifndef _PROLAND_TWEAKDEMEDITOR_H_
#define _PROLAND_TWEAKDEMEDITOR_H_

#include "proland/ui/twbar/TweakBarHandler.h"

namespace proland
{

/**
 * A TweakBarHandler to control a proland::EditElevationProducer.
 * @ingroup twbar
 * @authors Eric Bruneton, Antoine Begault
 */
PROLAND_API class TweakDemEditor : public TweakBarHandler
{
public:
    /**
     * Creates a new TweakDemEditor.
     *
     * @param active true if this TweakBarHandler must be initialy active.
     */
    TweakDemEditor(bool active = true);

    /**
     * Deletes this TweakDemEditor.
     */
    virtual ~TweakDemEditor();

    virtual void setActive(bool active);

protected:
    virtual void updateBar(TwBar *bar);
};

}

#endif
