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

#ifndef _PROLAND_BASICGRAPH_H_
#define _PROLAND_BASICGRAPH_H_

#include "proland/graph/Area.h"

namespace proland
{

/**
 * A BasicGraph contains a list of nodes, curves and areas, which can also contain graphs.
 * See #Graph.
 * BasicGraphs DO store the information related to their components. It may be used as the level 0 Graph as well as any other level.
 * @ingroup graph
 * @author Antoine Begault, Guillaume Piolat
 */
PROLAND_API class BasicGraph : public Graph
{
public:
    /**
     * BasicGraph implementation for the abstract class GraphIterator.
     * Two specializations are defined : one for nodes, and one for areas.
     * See Graph#GraphIterator.
     * In this implementation, T corresponds to the elements' Ids, and U to a smart pointer (ptr) to the element.
     * Since the list of curves is a multimap in BasicGraph, it was necessary to use a different class than BasicGraphIterator, which uses simple maps.
     */
    template<typename T, typename U> class BasicGraphIterator : public Graph::GraphIterator<U>
    {
    public:
        /**
         * Returns true if the iterator can still be incremented.
         */
        inline bool hasNext()
        {
            return iterator != end;
        }

        /**
        * Returns the next element from the list.
        */
        inline U next()
        {
            return (iterator++)->second;
        }

        typedef map<T, U> V;

        typename V::const_iterator iterator;

        typename V::const_iterator end;

        /**
         * Creates a new BasicGraphIterator.
         *
         * @param set a <T, U> map, containing the data we want to iterate.
         */
        inline BasicGraphIterator(const V &set) :
            iterator(set.begin()), end(set.end())
        {
        }
    };

    typedef BasicGraphIterator<NodeId, NodePtr> BasicNodeIterator;

    typedef BasicGraphIterator<AreaId, AreaPtr> BasicAreaIterator;

    /*
     * BasicGraph implementation for the abstract class GraphIterator.
     * This one is specialized for curves, since they are stored in a multimap here.
     * See #Graph::GraphIterator.
     */
    class BasicCurveIterator : public GraphIterator<CurvePtr>
    {
    public:
        /**
         * Returns true if the iterator can still be incremented.
         */
        inline bool hasNext()
        {
            return iterator != end;
        }

        /**
         * Returns the next element from the list.
         */
        inline CurvePtr next()
        {
            return (iterator++)->second;
        }

        typedef multimap<CurveId, CurvePtr> T;

        T::const_iterator iterator;

        T::const_iterator end;

        /**
         * Creates a new BasicCurveIterator.
         *
         * @param curves a <CurveId, CurvePtr> map containing the curves we want to iterate.
         */
        BasicCurveIterator(const T &curves) :
            iterator(curves.begin()), end(curves.end())
        {
        }

        /**
         * Creates a new BasicCurveIterator.
         *
         * @param begin an iterator to the first element to iterate.
         * @param end an iterator to the last element to iterate.
         */
        BasicCurveIterator(T::const_iterator begin, T::const_iterator end) :
            iterator(begin), end(end)
        {
        }
    };

    /**
     * Creates a new BasicGraph.
     */
    BasicGraph();

    /**
     * Deletes this BasicGraph.
     */
    virtual ~BasicGraph();

    /**
     * Clears the Graph : erases all areas, curves and nodes.
     * Called when deleting a graph or when loading a new graph.
     */
    virtual void clear();

    /*
     * Returns the number of nodes in this graph.
     */
    inline int getNodeCount() const
    {
        return (int) nodes.size();
    }

    /**
     * Returns the number of curves in this graph.
     */
    inline int getCurveCount() const
    {
        return curves.size();
    }

    /*
     * Returns the number of areas in this graph.
     */
    inline int getAreaCount() const
    {
        return areas.size();
    }

    /**
     * Returns a node.
     *
     * @param id the id of the node to return.
     * @return the node corresponding to this Id, NULL if not in this graph.
     */
    inline NodePtr getNode(NodeId id)
    {
        return id.id == NULL_ID ? NULL : id.ref;
    }

    /**
     * Returns a curve.
     *
     * @param id the id of the curve to return.
     * @return the curve corresponding to this Id, NULL if not in this graph.
     */
    inline CurvePtr getCurve(CurveId id)
    {
        return id.id == NULL_ID ? NULL : id.ref;
    }

    /**
     * Returns an area
     *
     * @param id the id of the area to return.
     * @return the area corresponding to this Id, NULL if not in this graph.
     */
    inline AreaPtr getArea(AreaId id)
    {
        return id.id == NULL_ID ? NULL : id.ref;
    }

    /**
     * Returns the childs of a given curve.
     * Child Curves are the curves that were created from another curve by clipping.
     *
     * @param parentId the parent curve.
     * @return a CurveIterator containing the child curves.
     */
    inline ptr<CurveIterator> getChildCurves(CurveId parentId)
    {
        typedef multimap<CurveId, CurvePtr>::const_iterator MI;
        pair<MI, MI> range = curves.equal_range(parentId);
        return new BasicCurveIterator(range.first, range.second);
    }

    /**
     * Returns the child of a given area.
     * A child Area is an area that was created from another area by clipping.
     *
     * @param parentId the parent area.
     * @return the child area.
     */
    AreaPtr getChildArea(AreaId parentId);

    /**
     * Loads a graph. This method determines whether to call load() or loadIndexed() method.
     *
     * @param file the file to load from.
     * @param loadSubgraphs if true, will load the subgraphs.
     */
    virtual void load(const string &file, bool loadSubgraphs = true);

    /**
     * Loads a graph from a basic file.
     *
     * @param the stream to read the data from.
     * @param loadSubgraphs if true, will load the subgraphs.
     */
    virtual void load(FileReader * fileReader, bool loadSubgraphs = true);

    /**
     * Loads a graph from an indexed file.
     * Indexed files are used to load LazyGraphs faster. It contains the indexes of each element in the file.
     *
     * @param the stream to read the data from.
     * @param loadSubgraphs if true, will load the subgraphs.
     */
    virtual void loadIndexed(FileReader * fileReader, bool loadSubgraphs = true);

    /**
     * Adds a node to this graph.
     *
     * @param p a vector containing the coordinates of the new node.
     * @return the new node.
     */
    NodePtr newNode(const vec2d &p);

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
     * @param model a model curve : the new curve will have the same vertices, if any.
     * @param start the start node.
     * @param end the end node.
     * @return the new curve.
     */
    virtual CurvePtr newCurve(CurvePtr model, NodePtr start, NodePtr end);

    /**
     * Adds an area to this graph.
     *
     * @param parent the parent area (NULL if none).
     * @param setParent if true, the new area's parent will be s et to the 'parent' parameter.
     */
    AreaPtr newArea(AreaPtr parent, bool setParent);

protected:
    /**
     * Removes a Node from this graph. This method is called when editing the Graph.
     *
     * @param id the id of a Node that is currently being deleted.
     */
    void removeNode(NodeId id);

    /**
     * Removes a Curve from this graph. This method is called when editing the Graph.
     *
     * @param id the id of a Curve that is currently being deleted.
     */
    void removeCurve(CurveId id);

     /**
     * Removes an Area from this graph. This method is called when editing the Graph.
     *
     * @param id the id of an Area that is currently being deleted.
     */
    void removeArea(AreaId id);

    /**
     * Returns an iterator containing the complete list of nodes in this graph.
     */
    inline ptr<NodeIterator> getNodes()
    {
        return new BasicNodeIterator(nodes);
    }

    /**
     * Returns an iterator containing the complete list of curves in this graph.
     */
    inline ptr<CurveIterator> getCurves()
    {
        return new BasicCurveIterator(curves);
    }

    /**
     * Returns an iterator containing the complete list of areas in this graph.
     */
    inline ptr<AreaIterator> getAreas()
    {
        return new BasicAreaIterator(areas);
    }

    /**
     * Clears the list of removedCurves and removedAreas.
     */
    void clean();

    /**
     * Curves that were removed. Used to update the graph.
     */
    vector<CurvePtr> removedCurves;

    /**
     * Areas that were removed. Used to update the graph.
     */
    vector<AreaPtr> removedAreas;

    /**
     * List of nodes contained in this graph, mapped to their ID.
     */
    map<NodeId, NodePtr> nodes;

    /**
     * List of curves contained in this graph, mapped to their parents, or to themselves if no parent (for root graph for example).
     */
    multimap<CurveId, CurvePtr> curves;

    /**
     * List of areas contained in this graph, mapped to their parents, or to themselves if no parent (for root graph for example).
     */
    map<AreaId, AreaPtr> areas;

};

}

#endif
