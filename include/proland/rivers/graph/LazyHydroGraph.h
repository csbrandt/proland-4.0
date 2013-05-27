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

#ifndef _PROLAND_LAZYHYDROGRAPH_H
#define _PROLAND_LAZYHYDROGRAPH_H

#include "proland/graph/LazyGraph.h"

namespace proland
{

/**
 * A HydroGraph with lazy loading behavior.
 * See graph::LazyGraph and rivers::HydroGraph.
 * @ingroup rivergraph
 * @author Antoine Begault
 */
PROLAND_API class LazyHydroGraph : public LazyGraph
{
public:
    /**
     * Creates a new LazyHydroGraph.
     */
    LazyHydroGraph();

    /**
     * Deletes this LazyHydroGraph.
     */
    virtual ~LazyHydroGraph();

    /**
     * Checks if the provided params count are correct for this graph.
     * A correct amount of parameters may depend on the graph it is used by.
     * For Graph, the amounts must be respectively at least 2, 4, 3, 3, 5, 2, 0.
     * Should be overwritten for graphs containing more data.
     */
    virtual void checkParams(int nodes, int curves, int areas, int curveExtremities, int curvePoints, int areaCurves, int subgraphs);

    /**
     * Adds a curve to this graph.
     *
     * @param parent the parent curve (NULL if none).
     * @param setParent if true, the new curve's parent will be set to the 'parent' parameter.
     * @return the new curve.
     */
    virtual CurvePtr newCurve(CurvePtr parent, bool setParent);

    /**
     * Adds a curve to this graph.
     *
     * @param model a model curve : the new curve will have the same vertices.
     * @param start the start node.
     * @param end the end node.
     * @return the new curve.
     */
    virtual CurvePtr newCurve(CurvePtr model, NodePtr start, NodePtr end);

    /**
     * Returns a new HydroGraph.
     */
    virtual Graph *createChild();

    /**
     * Saves this graph from a basic file.
     *
     * @param fileWriter the FileWriter used to save the file.
     * @param saveAreas if true, will save the subgraphs.
     */
    virtual void save(FileWriter *fileWriter, bool saveAreas = true);

    /**
     * Saves this graph from an indexed file.
     * Indexed files are used to load LazyGraphs faster. It contains the indexes of each element in the file.
     *
     * @param fileWriter the FileWriter used to save the file
     * @param saveAreas if true, will save the subgraphs
     */
    virtual void indexedSave(FileWriter *fileWriter, bool saveAreas = true);

    virtual void movePoint(CurvePtr c, int i, const vec2d &p, set<CurveId> &changedCurves);

    virtual NodePtr addNode(CurvePtr c, int i, Graph::Changes &changed);

protected:
    /**
     * Loads the Curve corresponding to the given Id.
     * The Curve description will be fetched via #fileReader at the offset given as parameter.
     *
     * @param offset the offset of this Curve in the file.
     * @param id the id of this Curve.
     * @return the loaded Curve.
     */
    virtual CurvePtr loadCurve(long int offset, CurveId id);

    friend class LazyHydroCurve;
};

}

#endif
