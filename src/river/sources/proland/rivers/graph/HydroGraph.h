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

#ifndef _PROLAND_HYDROGRAPH_H_
#define _PROLAND_HYDROGRAPH_H_

#include "proland/graph/BasicGraph.h"
#include "proland/rivers/graph/HydroCurve.h"

namespace proland
{

/**
 * @defgroup rivergraph graph
 * Provides graphs with river related information.
 * @ingroup rivers
 */

/**
 * A Graph with additional, river specific data. This data consists of:
 * - Rivers: Basically, they are regular Curves.
 * - Banks : Corresponds to a part of the boundaries of a given River.
 * Important to improve performances. Also contains a potential Value.
 * Areas are treated just like regular areas.
 * @ingroup rivergraph
 * @author Antoine Begault, Guillaume Piolat
 */
PROLAND_API class HydroGraph : public BasicGraph
{
public:
    /**
     * Creates a new HydroGraph.
     */
    HydroGraph();

    /**
     * Deletes this HydroGraph.
     */
    virtual ~HydroGraph();

    virtual void load(FileReader * fileReader, bool loadSubgraphs = true);

    virtual void loadIndexed(FileReader * fileReader, bool loadSubgraphs = true);

    virtual void checkParams(int nodes, int curves, int areas, int curveExtremities, int curvePoints, int areaCurves, int subgraphs);

    virtual CurvePtr newCurve(CurvePtr parent, bool setParent);

    virtual CurvePtr newCurve(CurvePtr model, NodePtr start, NodePtr end);

    virtual CurvePtr addCurvePart(CurvePart &cp, set<CurveId> *addedCurves, bool setParent = true);

    virtual Graph *createChild();

    virtual void save(FileWriter *fileWriter, bool saveAreas = true);

    virtual void indexedSave(FileWriter *fileWriter, bool saveAreas = true);

    /**
     * Saves a graph from a basic file.
     *
     * @param graph the graph to save
     * @param fileWriter the FileWriter used to save the file.
     * @param saveAreas if true, will save the subgraphs.
     */
    static void save(Graph *graph, FileWriter *fileWriter, bool saveAreas = true);

    /**
     * Saves a graph from an indexed file.
     * Indexed files are used to load LazyGraphs faster. It contains the indexes of each element in the file.
     *
     * @param graph the graph to save
     * @param fileWriter the FileWriter used to save the file
     * @param saveAreas if true, will save the subgraphs
     */
    static void indexedSave(Graph *graph, FileWriter *fileWriter, bool saveAreas = true);

    virtual void movePoint(CurvePtr c, int i, const vec2d &p, set<CurveId> &changedCurves);

    virtual NodePtr addNode(CurvePtr c, int i, Graph::Changes &changed);

    virtual void print(bool detailed);
};

}

#endif
