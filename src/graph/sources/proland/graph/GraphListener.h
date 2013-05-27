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

#ifndef _PROLAND_GRAPH_LISTENER_H_
#define _PROLAND_GRAPH_LISTENER_H_

namespace proland
{

/**
 * Abstract class used to monitor changes on a graph.
 * @ingroup graph
 * @author Antoine Begault
 */
PROLAND_API class GraphListener
{
public:
    /**
     * Creates a new GraphListener.
     */
    GraphListener();

    /**
     * Deletes this GraphListener.
     */
    virtual ~GraphListener();

    /**
     * This virtual method must be called when updating the graph.
     * It will determine what has to be done on this listener when the graph has changed.
     */
    virtual void graphChanged() = 0;
};

}

#endif
