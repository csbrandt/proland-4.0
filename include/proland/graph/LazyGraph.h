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

#ifndef _PROLAND_LAZYGRAPH_H_
#define _PROLAND_LAZYGRAPH_H_

#include <fstream>
#include <iostream>

#include "proland/graph/Area.h"

using namespace std;

namespace proland
{

/**
 * A Graph that will only load the offsets of each elements
 * (node/curve/area) in the input file. It will then fetch the element directly
 * in the file if a required element isn't already loaded. It keeps track of the
 * resources it has loaded, and automatically deletes them when they are unused
 * (i.e. unreferenced). Alternatively, a lazygraph can cache unused resources so
 * that they can be loaded quickly if they are needed again.
 * LazyGraph contains a list for each kind of resource which maps the Id of the
 * resource to the position where the data of that resource can be found in the
 * file used in #fileReader.
 * LazyGraph can then just move the get pointer of the file in #fileReader to be
 * able to retrieve the info about a selected resource.
 * @ingroup graph
 * @author Antoine Begault, Guillaume Piolat
 */
PROLAND_API class LazyGraph : public Graph
{
public:
    /**
     * LazyGraph implementation for the abstract class GraphIterator.
     * Three specializations are defined : one for nodes, one for curves, and
     * one for areas.
     * See Graph#GraphIterator.
     * LazyGraphIterators use the mapped ids/offsets from LazyGraph to fetch
     * items. In this implementation, T corresponds to the elements' Ids, and
     * U to a smart pointer (ptr) to the element.
     */
    template<typename T, typename U>
    class LazyGraphIterator : public Graph::GraphIterator<U>
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
        U next()
        {
            U u = dynamic_cast<LazyGraph *>(owner)->get(iterator->first);
            iterator++;
            return u;
        }

    private:
        typedef map<T, long int> V;

        /**
         * The Graph containing the elements to iterate, and which will be used
         * to fetch the data (via the LazyGraph#get() method).
         */
        Graph *owner;

        typename V::const_iterator iterator;

        typename V::const_iterator end;

        /**
         * Creates a new LazyGraphIterator.
         *
         * @param set a <T, long int> map, which maps an Id to an offset in a file.
         * @param owner the Graph containing the elements to iterate, and which
         *      will be used to fetch the data (via the LazyGraph#get() method).
         */
        LazyGraphIterator(const V &set, Graph* g) :
            owner(g), iterator(set.begin()), end(set.end())
        {
        }

        /**
         * Deletes this LazyGraphIterator.
         */
        ~LazyGraphIterator()
        {
            owner = NULL;
        }

        friend class LazyGraph;

        friend class Graph;
    };

    typedef LazyGraphIterator<NodeId, NodePtr> LazyNodeIterator;

    typedef LazyGraphIterator<CurveId, CurvePtr> LazyCurveIterator;

    typedef LazyGraphIterator<AreaId, AreaPtr> LazyAreaIterator;

    /**
     * Templated cache used to store unused graph items (nodes, curves, areas..).
     */
    template<typename T>
    class GraphCache
    {
    public:
        /**
         * Removes a resource from the cache, if found.
         *
         * @param t the resource to remove.
         * @return true if the resource was in the cache. Otherwise, returns false.
         */
        bool remove(ptr<T> t)
        {
            typename map<ptr<T> , typename list<ptr<T> >::iterator>::iterator i = unusedResources.find(t);
            if (i != unusedResources.end()) {
                unusedResourcesOrder.erase(i->second);
                unusedResources.erase(i);
                return true;
            }
            //t->owner = owner;
            return false;
        }

        /**
         * Adds a resource to the cache. If required (when cache is full), it
         * will delete the least recently used item.
         *
         * @param t the resource to add.
         * @param modified whether the resource was changed or not. This
         *      enables LazyGraphs to keep changed items in memory, instead of
         *      eventually deleting them and restoring an outdated version.
         */
        void add(T* t, bool modified = false)
        {
            typename set<ptr<T> >::iterator i = find(changedResources.begin(), changedResources.end(), t);
            if (i != changedResources.end()) {
                return;
            }

            if (modified == true) {
                changedResources.insert(t);
                return;
            }

            typename list<ptr<T> >::iterator j;
            if (unusedResourcesOrder.size() == size) {
                j = unusedResourcesOrder.begin();
                ptr<T> r = *j;
                unusedResources.erase(r);
                unusedResourcesOrder.erase(j);
                r->setOwner(NULL);
                r = NULL;
            }
            j = unusedResourcesOrder.insert(unusedResourcesOrder.end(), t);
            unusedResources.insert(make_pair(t, j));
        }

    private:
        /**
         * Creates a new GraphCache.
         *
         * @param g graph that uses this cache.
         * @param size the cache size : number of items it can contain
         *      (doesn't include modified items).
         */
        GraphCache(Graph* g, unsigned int size = 0) :
            owner(g), size(size)
        {
        }

        /**
         * Deletes this GraphCache.
         */
        ~GraphCache()
        {
            unusedResources.clear();
            unusedResourcesOrder.clear();
            changedResources.clear();
        }

        /**
         * Graph that uses this cache.
         */
        Graph *owner;

        /**
         * This lists allows to get the least recently used item in order to
         * delete it when cache is full.
         */
        list<ptr<T> > unusedResourcesOrder;

        /**
         * Complete list of unused resources.
         */
        map<ptr<T>, typename list<ptr<T> >::iterator> unusedResources;

        /**
         * Complete list of changed resources. These resources doesn't count in
         * the number of unused resources.
         */
        set<ptr<T> > changedResources;

        /**
         * Maximum size of the cache.
         */
        unsigned int size;

        friend class LazyNode;

        friend class LazyCurve;

        friend class LazyArea;

        friend class LazyGraph;
    };

    /**
     * Creates a new LazyGraph.
     */
    LazyGraph();

    /**
     * Deletes this LazyGraph.
     */
    virtual ~LazyGraph();

    /**
     * Clears the cache, and deletes every element of this graph.
     * Necessary when deleting the LazyGraph or when loading new data.
     */
    virtual void clear();

    /**
     * Initializes LazyGraph's cache and vectors.
     */
    virtual void init();

    /**
     * Returns the number of curves in this graph.
     */
    int getCurveCount() const;

    /**
     * Returns the number of nodes in this graph.
     */
    int getNodeCount() const;

    /**
     * Returns the number of areas in this graph.
     */
    int getAreaCount() const;

    /**
     * Sets the Node Cache size, i.e. the number of nodes that can be kept in
     * memory at the same time.
     *
     * @param size the size.
     */
    void setNodeCacheSize(int size);

    /**
     * Sets the Curve Cache size, i.e. the number of curves that can be kept in
     * memory at the same time.
     *
     * @param size the size.
     */
    void setCurveCacheSize(int size);

    /**
     * Sets the Area Cache size, i.e. the number of areas that can be kept in
     * memory at the same time.
     *
     * @param size the size.
     */
    void setAreaCacheSize(int size);

    /**
     * Loads a graph. This method determines whether to call load() or
     * loadIndexed() method.
     *
     * @param file the file to load from.
     * @param loadSubgraphs if true, will load the subgraphs.
     */
    void load(const string &file, bool loadSubgraphs = 0);

    /**
     * Loads a graph from a basic file, using #fileReader.
     *
     * @param loadSubgraphs if true, will load the subgraphs.
     */
    void load(bool loadSubgraphs = 0);

    /**
     * Loads a graph from an indexed file, using #fileReader.
     * Indexed files are used to load LazyGraphs faster. It contains the
     * indexes of each element in the file.
     *
     * @param loadSubgraphs if true, will load the subgraphs.
     */
    void loadIndexed(bool loadSubgraphs = 0);

    /**
     * Loads a graph from a basic file. This method is only used for
     * loading subgraphs. Empty implementation for LazyGraph.
     *
     * @param the stream to read the data from.
     * @param loadSubgraphs if true, will load the subgraphs.
     */
    void load(FileReader *fileReader, bool loadsubgraphs = 0);

    /**
     * Calls the #getNode() method. Used for generic Iterators.
     *
     * @param id the id of the node we want to get/load.
     */
    NodePtr get(NodeId id);

    /**
     * Calls the #getCurve-) method. Used for generic Iterators.
     *
     * @param id the id of the curve we want to get/load.
     */
    CurvePtr get(CurveId id);

    /**
     * Calls the #getArea() method. Used for generic Iterators.
     *
     * @param id the id of the area we want to get/load.
     */
    AreaPtr get(AreaId id);

    /**
     * Will get the Node corresponding to the given Id. If has already been
     * loaded, it will only get it from the cache. Otherwise, it will load it.
     *
     * @param id the NodeId of the desired Node.
     * @return the corresponding Node.
     */
    NodePtr getNode(NodeId id);

    /**
     * Will get the Curve corresponding to the given Id. If has already been
     * loaded, it will only get it from the cache. Otherwise, it will load it.
     *
     * @param id the CurveId of the desired Curve.
     * @return the corresponding Curve.
     */
    CurvePtr getCurve(CurveId id);

    /**
     * Will get the Area corresponding to the given Id. If has already been
     * loaded, it will only get it from the cache. Otherwise, it will load it.
     *
     * @param id the AreaId of the desired Area.
     * @return the corresponding Area.
     */
    AreaPtr getArea(AreaId id);

    /**
     * Will get the subgraph corresponding to the given AreaId. Only used
     * when loading an area.
     *
     * @param id the Id of the area containing the subgraph.
     * @return the corresponding subgraph.
     */
    GraphPtr getSubgraph(AreaId id);

    /**
     * Returns the childs of a given curve.
     * Child Curves are the curves that were created from another curve by
     * clipping.
     *
     * @param parentId the parent curve.
     * @return a CurveIterator containing the child curves.
     */
    ptr<CurveIterator> getChildCurves(CurveId parentId);

    /**
     * Returns the child of a given area.
     * A child Area is an area that was created from another area by clipping.
     *
     * @param parentId the parent area.
     * @return the child area.
     */
    AreaPtr getChildArea(AreaId parentId);

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
     * @param setParent if true, the new curve's parent will be set to the
     *      'parent' parameter.
     * @return the new curve.
     */
    CurvePtr newCurve(CurvePtr parent, bool setParent);

    /**
     * Adds a curve to this graph.
     *
     * @param model a model curve : the new curve will have the same vertices,
     *      if any.
     * @param start the start node.
     * @param end the end node.
     * @return the new curve.
     */
    CurvePtr newCurve(CurvePtr model, NodePtr start, NodePtr end);

    /**
     * Adds an area to this graph.
     *
     * @param parent the parent area (NULL if none).
     * @param setParent if true, the new area's parent will be s et to the
     *      'parent' parameter.
     */
    AreaPtr newArea(AreaPtr parent, bool setParent);

    /**
     * Moves a control point or a node on a given curve.
     *
     * @param c the curve to edit.
     * @param i the index of the control point (or node, if i is 0 or
     * c->getSize()-1) to move on c.
     * @param p the new control point or node position.
     */
    virtual void movePoint(CurvePtr c, int i, const vec2d &p);

    /**
     * Deletes a resource from this graph. This method is called from the
     * resource destructor when a resource gets deleted (for example when a
     * node is deleted in the #releaseNode method).
     *
     * @param *Id a resource's Id which is currently being deleted.
     */
    void deleteNode(NodeId id);

    /**
     * See #deleteNode(NodeId id).
     */
    void deleteCurve(CurveId id);

    /**
     * See #deleteNode(NodeId id).
     */
    void deleteArea(AreaId id);

    /**
     * Returns the Node Cache.
     */
    GraphCache<Node> *getNodeCache();

    /**
     * Returns the Curve Cache.
     */
    GraphCache<Curve> *getCurveCache();

    /**
     * Returns the Area Cache.
     */
    GraphCache<Area> *getAreaCache();

    /**
     * Releases an unused resource. If there is a cache of unused resource
     * then this resource is put in this cache (the oldest resource in the
     * cache is evicted if the cache is full). Otherwise if there is no cache,
     * the resource is deleted directly.
     *
     * @param NodeId an unused Node Id, i.e. an unreferenced resource (See ptr class).
     */
    void releaseNode(NodeId id);

    /**
     * see #releaseNode(NodeId id)
     */
    void releaseCurve(CurveId id);

    /**
     * see #releaseNode(NodeId id)
     */
    void releaseArea(AreaId id);

protected:
    /**
     * Loads the Node corresponding to the given Id.
     * The Node description will be fetched via #fileReader at the offset given
     * as parameter.
     *
     * @param offset the offset of this Node in the file.
     * @param id the id of this Node.
     * @return the loaded Node.
     */
    virtual NodePtr loadNode(long int offset, NodeId id);

    /**
     * Loads the Curve corresponding to the given Id.
     * The Curve description will be fetched via #fileReader at the offset given
     * as parameter.
     *
     * @param offset the offset of this Curve in the file.
     * @param id the id of this Curve.
     * @return the loaded Curve.
     */
    virtual CurvePtr loadCurve(long int offset, CurveId id);

    /**
     * Loads the Area corresponding to the given Id.
     * The Area description will be fetched via #fileReader at the offset given
     * as parameter.
     *
     * @param offset the offset of this Area in the file.
     * @param id the id of this Area.
     * @return the loaded Area.
     */
    virtual AreaPtr loadArea(long int offset, AreaId id);

    /**
     * Loads a subgraph corresponding to a given AreaId. Only used when loading
     * an Area. The Graph description will be fetched via #fileReader at the
     * offset given as parameter.
     *
     * @param offset the offset of this Graph in the file.
     * @param the id of the Area containing this Graph.
     * @return the loaded Graph.
     */
    virtual GraphPtr loadSubgraph(long int offset, AreaId id);

    /**
     * Removes a Node from this graph. This method is called when editing
     * the Graph.
     *
     * @param id the id of a Node that is currently being deleted.
     */
    void removeNode(NodeId id);

    /**
     * Removes a Curve from this graph. This method is called when editing
     * the Graph.
     *
     * @param id the id of a Curve that is currently being deleted.
     */
    void removeCurve(CurveId id);

    /**
     * Removes an Area from this graph. This method is called when editing
     * the Graph.
     *
     * @param id the id of an Area that is currently being deleted.
     */
    void removeArea(AreaId id);

    /**
     * Calls #removeNode().
     *
     * @param n a Node to remove.
     */
    void remove(Node * n);

    /**
     * Calls #removeCurve().
     *
     * @param c a Curve to remove.
     */
    void remove(Curve * c);

    /**
     * Calls #removeArea().
     *
     * @param a an Area to remove.
     */
    void remove(Area * a);

    friend class Node;

    friend class Curve;

    friend class Area;

    friend class LazyNode;

    friend class LazyCurve;

    friend class LazyArea;

    virtual void clean();

    /**
     * Reads the data file to pass through a given graph.
     * Used for loading : we only want to get the offset of graphs,
     * So, we keep it, and then directly jump to the next subgraph.
     */
    void readSubgraph();

    /**
     * Returns the list of offsets for each node.
     * See LazyGraph description.
     */
    map<NodeId, long int> getNodeOffsets() const;

    /**
     * Returns the list of offsets for each curve.
     * See LazyGraph description.
     */
    map<CurveId, long int> getCurveOffsets() const;

    /**
     * Returns the list of offsets for each area.
     * See LazyGraph description.
     */
    map<AreaId, long int> getAreaOffsets() const;

    /**
     * Returns an iterator containing the Nodes of this Graph.
     */
    ptr<NodeIterator> getNodes();

    /**
     * Returns an iterator containing the Curves of this Graph.
     */
    ptr<CurveIterator> getCurves();

    /**
     * Returns an iterator containing the Areas of this Graph.
     */
    ptr<AreaIterator> getAreas();

    /**
     * The entire list of curves in this graph.
     */
    map<CurveId, Curve*> curves;

    /**
     * The entire list of nodes in this graph.
     */
    map<NodeId, Node*> nodes;

    /**
     * The entire list of areas in this graph.
     */
    map<AreaId, Area*> areas;

    /**
     * Id of the next node that will be created. Initialized at 0.
     **/
    NodeId nextNodeId;

    /**
     * Id of the next curve that will be created. Initialized at 0.
     **/
    CurveId nextCurveId;

    /**
     * Id of the next area that will be created. Initialized at 0.
     */
    AreaId nextAreaId;

    /**
     * The offsets of each Node in the input file.
     * Loaded in the #load() function.
     **/
    map<NodeId, long int> nodeOffsets;

    /**
     * The offsets of each Curve in the input file.
     * Loaded in the #load() function.
     **/
    map<CurveId, long int> curveOffsets;

    /**
     * The offsets of each Area in the input file.
     * Loaded in the #load() function.
     **/
    map<AreaId, long int> areaOffsets;

    /**
     * The offsets of each subgraph in the input file.
     * Loaded in the #load() function.
     **/
    map<AreaId, long int> subgraphOffsets;

    /**
     * Cache of unused and modified Nodes.
     */
    GraphCache<Node> *nodeCache;

    /**
     * Cache of unused and modified Curves.
     */
    GraphCache<Curve> *curveCache;

    /**
     * Cache of unused and modified Areas.
     */
    GraphCache<Area> *areaCache;

    /**
    * File descriptor for loading graph elements.
    */
    FileReader *fileReader;
};

}

#endif
