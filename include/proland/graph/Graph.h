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

#ifndef _PROLAND_GRAPH_H_
#define _PROLAND_GRAPH_H_

#include <list>
#include <map>
#include <vector>
#include <set>

#include "ork/core/Object.h"
#include "ork/math/box2.h"
#include "proland/graph/FileWriter.h"
#include "proland/graph/FileReader.h"

using namespace ork;

namespace proland
{

/**
 * @defgroup graph graph
 * Provides a framework to model, clip, refine and %edit vector-based data.
 * @ingroup proland
 */

 // -----------------------------------------------------------------------
 // Class declarations :
 // vectorial elements
 // Smart Pointers on vec. elements
 // Ids for each type
 // Id comparator method
 // -----------------------------------------------------------------------

class Graph;

class Node;

class Curve;

class Area;

class Margin;

class CurvePart;

class GraphListener;

struct Vertex;

typedef ptr<Graph> GraphPtr;

typedef ptr<Node> NodePtr;

typedef ptr<Curve> CurvePtr;

typedef static_ptr<Curve> StaticCurvePtr;

typedef ptr<Area> AreaPtr;

#define NULL_ID ((unsigned int) -1)

/**
 * The identifier of a Node.
 * @ingroup graph
 * @author Antoine Begault
 */
typedef union
{
    unsigned int id;
    Node* ref;
} NodeId;

/**
 * The identifier of a Curve.
 * @ingroup graph
 * @author Antoine Begault
 */
typedef union
{
    unsigned int id;
    Curve* ref;
} CurveId;

/**
 * The identifier of a Graph.
 * @ingroup graph
 * @author Antoine Begault
 */
typedef union
{
    unsigned int id;
    Graph* ref;
} GraphId;

/**
 * The identifier of an Area.
 * @ingroup graph
 * @author Antoine Begault
 */
typedef union
{
    unsigned int id;
    Area* ref;
} AreaId;

 // -----------------------------------------------------------------------
 // Definition of the operators to compare Id's
 // Used for maps
 // -----------------------------------------------------------------------
inline bool operator <(const CurveId &u, const CurveId &v)
{
    return u.id < v.id;
}

inline bool operator <(const GraphId &u, const GraphId &v)
{
    return u.id < v.id;
}

inline bool operator <(const NodeId &u, const NodeId &v)
{
    return u.id < v.id;
}

inline bool operator <(const AreaId &u, const AreaId &v)
{
    return u.id < v.id;
}

inline bool operator ==(const CurveId &u, const CurveId &v)
{
    return u.id == v.id;
}

inline bool operator ==(const NodeId &u, const NodeId &v)
{
    return u.id == v.id;
}

inline bool operator ==(const AreaId &u, const AreaId &v)
{
    return u.id == v.id;
}

inline bool operator ==(const GraphId &u, const GraphId &v)
{
    return u.id == v.id;
}

/**
 * A Graph contains vectorial data representing areas, roads, rivers...
 * It handles creation, modification, and deletion of each element.
 * There are two types of graphs : Basic & Lazy. See each one for further desc.
 * Vectorial Data consists of nodes (points), curves (how points are
 * linked to each other), and areas (groups of curves).
 * @ingroup graph
 * @author Antoine Begault, Guillaume Piolat
 */
PROLAND_API class Graph : public Object
{
public:
    /**
     * An abstract iterator to iterate over the elements of a Graph. This
     * template is specialized for nodes, curves and areas, and two
     * implementations are provided (one for LazyGraph and one for BasicGraph).
     */
    template<typename T> class GraphIterator : public Object
    {
    public:
        /**
         * Returns true if the iterator can still be incremented.
         */
        virtual bool hasNext() = 0;

        /**
         * Returns the next element from the list.
         */
        virtual T next() = 0;

        /**
         * Creates a new GraphIterator.
         */
        GraphIterator() :
            Object("Iterator")
        {
        }
    };

    typedef GraphIterator< ptr<Node> > NodeIterator;

    typedef GraphIterator< ptr<Curve> > CurveIterator;

    typedef GraphIterator< ptr<Area> > AreaIterator;

    /**
     * A set of changes that occured to a Graph. The changes are specified
     * with sets of removed and added curves and areas. These structs are
     * created when a Graph is modified, and they are used in #clipUpdate
     * to update a clipped graph incrementally, without doing a full #clip.
     */
    struct Changes
    {
        list<AreaId> changedArea;

        /**
         * List of removed Curves.
         */
        set<CurveId> removedCurves;

        /**
         * List of added Curves.
         */
        set<CurveId> addedCurves;

        /**
         * List of removed areas.
         */
        set<AreaId> removedAreas;

        /**
         * List of added areas.
         */
        set<AreaId> addedAreas;

        /**
         * Returns true if this struct is empty (if none of the lists
         * contain data).
         */
        bool empty();

        /**
         * Clears each lists.
         */
        void clear();

        /**
         * Inserts a list of changes in the current struct.
         * @param c a Graph::Changes struct.
         */
        void insert(Changes c);

        /**
         * Checks if this Changes equals another.
         *
         * @param c another Changes struct.
         * @return true if the two structs contain the same data.
         */
        bool equals(Changes c) const;

        /**
         * Print method for debug only.
         */
        void print() const;
    };

    /**
     * Creates a new Graph.
     */
    Graph();

    /**
     * Deletes this Graph.
     */
    virtual ~Graph();

    /**
     * Displays the graph's content.
     * Debug only.
     *
     * @param detailed if true, will show info about every item in graph.
     *      If false, only displays elements count.
     */
    virtual void print(bool detailed = true);

    /**
     * Deletes all items when deleting the graph.
     */
    virtual void clear() = 0;

    /**
     * Returns the ancestor Graph (Furthest parent).
     * See #getParent().
     */
    inline Graph* getAncestor()
    {
        assert(parent != this);
        if (parent == NULL) {
            return this;
        }
        return parent->getAncestor();
    }

    /**
     * Sets the parent graph of this graph. The parent graph is the graph
     * from which this graph was created by clipping, or NULL is this
     * graph was not created by clipping (e.g. the root graph tile in a
     * GraphProducer).
     *
     * @param p the parent graph of this graph.
     */
    virtual void setParent(Graph* p);

    /**
     * Returns the parent Graph of this graph. See #setParent().
     */
    inline Graph* getParent() const
    {
        return parent;
    }

    /**
     * Returns the number of nodes in this graph.
     */
    virtual int getNodeCount() const = 0;

    /**
     * Returns the number of curves in this graph.
     */
    virtual int getCurveCount() const = 0;

    /**
     * Returns the number of areas in this graph.
     */
    virtual int getAreaCount() const = 0;

    /**
     * Seaches in #mapping if given coordinates correspond to a point.
     * If they do, it returns the node, NULL otherwise.
     *
     * @param pos the node's position.
     */
    virtual Node *findNode(vec2d &pos) const;

    /**
     * Returns a node.
     *
     * @param id the id of the node to be returned.
     * @return the corresponding node.
     */
    virtual NodePtr getNode(NodeId id) = 0;

    /**
     * Returns a curve.
     *
     * @param id the id of the curve to be returned.
     * @return the corresponding curve.
     */
    virtual CurvePtr getCurve(CurveId id) = 0;

    /**
     * Returns an area.
     *
     * @param id the id of the area to be returned.
     * @return the corresponding area.
     */
    virtual AreaPtr getArea(AreaId id) = 0;

    /**
     * Returns the child curves of a given curve of the parent graph. A child
     * curve of a curve C of the parent graph is a curve that was created by
     * the clipping of C, during the creation of this graph. The clipping of
     * a curve can result in several clipped curves, hence a curve can have
     * several child curves.
     *
     * @param parentId the id of a curve of the parent graph of this graph.
     * @return an iterator containing all the child curves of this parent curve.
     */
    virtual ptr<CurveIterator> getChildCurves(CurveId parentId) = 0;

    /**
     * Returns the child area of a given area of the parent graph. A child area
     * of an area A of the parent graph is an area that was created by the
     * clipping of A, during the creation of this graph. The clipping of an
     * area can result in only one clipped area.
     *
     * @param parentId the id of an area of the parent graph of this graph.
     * @return the child area.
     */
    virtual AreaPtr getChildArea(AreaId parentId) = 0;

    /**
     * Returns an iterator containing the entire list of nodes in this graph.
     * See #GraphIterator.
     */
    virtual ptr<NodeIterator> getNodes() = 0;

    /**
     * Returns an iterator containing the entire list of curves in this graph.
     * See #GraphIterator.
     */
    virtual ptr<CurveIterator> getCurves() = 0;

    /**
     * Returns an iterator containing the entire list of areas in this graph.
     * See #GraphIterator.
     */
    virtual ptr<AreaIterator> getAreas() = 0;

    /**
     * Gets the list of areas containing the specified curves.
     *
     * @param curves the set of curves to search.
     * @param areas the set of areas linked to the specified curves.
     */
    void getAreasFromCurves(const set<CurveId> &curves, set<AreaId> &areas);

    /**
     * Gets the list of points contained in the given curves.
     *
     * @param curves the set of curves to search.
     * @param orientations the orientations of each curves.
     *      Determines the final result.
     * @param points the resulting vector of points.
     */
    void getPointsFromCurves(vector<CurveId> &curves,
            map<CurveId, int> orientations, vector<Vertex> &points);

    /**
     * Loads a graph. This method determines whether to call load() or
     * loadIndexed() method.
     *
     * @param file the file to load from.
     * @param loadSubgraphs if true, will load the subgraphs.
     */
    virtual void load(const string &file, bool loadSubgraphs = true) = 0;

    /**
     * Loads a graph from a basic file.
     *
     * @param the stream to read the data from.
     * @param loadSubgraphs if true, will load the subgraphs.
     */
    virtual void load(FileReader *fileReader, bool loadSubgraphs = true) = 0;

    /**
     * Checks if the provided params count are correct for this graph.
     * A correct amount of parameters may depend on the graph it is used by.
     * For Graph, the amounts must be respectively at least 2, 3, 3, 1, 3, 2, 0.
     * Should be overwritten for graphs containing more data.
     */
    virtual void checkParams(int nodes, int curves, int areas,
            int curveExtremities, int curvePoints, int areaCurves, int subgraphs);

    /**
     * Asserts that the provided params count are the default Graph parameter counts.
     * Prevents from loading unsuitable graphs in a subclass of Graph.
     * A correct amount of parameters may depend on the graph it is used by.
     * For Graph, the amounts must be respectively 2, 3, 3, 1, 3, 2, 0.
     * Should be overwritten for graphs containing more data.
     */
    virtual void checkDefaultParams(int nodes, int curves, int areas,
            int curveExtremities, int curvePoints, int areaCurves, int subgraphs);

    /**
     * Saves this graph. This method determines whether to call save() or
     * indexed save() method.
     *
     * @param file the file to save into.
     * @param saveAreas if true, will save the subgraphs.
     * @param isBinary if true, will save in binary mode.
     * @param isIndexed if true, will save in indexed mode, used to
     *      accelerate LazyGraph loading.
     */
    virtual void save(const string &file, bool saveAreas = true,
            bool isBinary = true, bool isIndexed = false);

    /**
     * Saves this graph from a basic file.
     *
     * @param fileWriter the FileWriter used to save the file.
     * @param saveAreas if true, will save the subgraphs.
     */
    virtual void save(FileWriter *fileWriter, bool saveAreas = true);

    /**
     * Saves this graph from an indexed file.
     * Indexed files are used to load LazyGraphs faster. It contains
     * the indexes of each element in the file.
     *
     * @param fileWriter the FileWriter used to save the file
     * @param saveAreas if true, will save the subgraphs
     */
    virtual void indexedSave(FileWriter *fileWriter, bool saveAreas = true);

    /**
     * Subdivides the curves of this graph where necessary to satisfy the
     * given maximum error bound. See Curve#flatten().
     *
     * @param squareFlatness square of the maximum allowed distance between
     *      a limit curve and its polyline approximation.
     */
    void flatten(float squareFlatness);

    /**
     * Subdivides the given curves where necessary to satisfy the given
     * maximum error bound. See Curve#flatten().
     *
     * @param changedCurves a list of curves that have changed.
     * @param squareFlatness square of the maximum allowed distance between
     *      a limit curve and its polyline approximation.
     */
    void flattenUpdate(const Changes &changes, float squareFlatness);

    /**
     * Clips this graph with the given clip region. A specific margin is
     * added to the clip region for each curve and area that will be clipped.
     *
     * @param clip the clip region.
     * @param margin the object to be used to compute the specific margins for
     * each curve and area.
     * @return the clipped graph.
     */
    virtual Graph* clip(const box2d &clip, Margin *margin);

    /**
     * Updates a clipped graph based on a set of changed curves and areas.
     * The old clipped curves and areas corresponding to the changed curves
     * and areas are removed from the clipped graph, and the changed curves
     * and areas are then clipped again and added to the clipped graph.
     *
     * @param changedCurves a list of changed curves.
     * @param clip the clip region that was used to compute the original
     * clipped graph.
     * @param margin the object that was used to compute the original clipped
     * graph (see #clip()).
     * @param result the clipped graph that must be updated.
     */
    void clipUpdate(const Changes &srcChanges, const box2d &clip,
            Margin *margin, Graph &result, Changes &dstChanges);

    /**
     * Adds a Curve, copy of a given CurvePart, into this graph.
     *
     * @param cp a CurvePart to be added.
     * @param addedCurves a set of CurveId. The added curve will be added
     *      into this.
     * @param setParent If true, the return Curve's parent will be cp's parent.
     * @return the added Curve
     */
    virtual CurvePtr addCurvePart(CurvePart &cp, set<CurveId> *addedCurves,
            bool setParent = true);

    /**
     * Adds a Curve, copy of a given CurvePart, into a given area of this graph.
     *
     * @param cp a CurvePart to be added.
     * @param addedCurves a set of CurveId. The added curve will be added
     *      into this.
     * @param visited a set of CurveId. Used in clip to determine which
     *      curves has already been clipped. Will add cp into this.
     * @param a The area in which we will add the new Curve.
     */
    void addCurvePart(CurvePart &cp, set<CurveId> *addedCurves,
            set<CurveId> &visited, AreaPtr a);

    /**
     * Computes the maximal curves of this graph. This method removes all
     * nodes that connect exactly two curves, and merges the two curves
     * into a single one. In fact the graph is not modified, but a copy
     * with these maximal curves is created in the given graph.
     *
     * @param useType true if curves of different types must not be merged.
     * @param result the graph to which the maximal curves must be added.
     */
    void buildCurves(bool useType, GraphPtr result);

    /**
     * Builds an area, starting from a given Curve, and a given Node in it.
     * The result is a list of curves and their orientations.
     *
     * @param begin the starting curve.
     * @param excluded the list of excluded curves (for example the ones
     *      that are not in a loop).
     * @param used After function call, it will contain the list of curves
     *      for the new area. Empty if no area found.
     * @param orientations After function call, it will contain
     *      correspondence betwenn each curve and its orientation in the
     *      area. Empty if no area found.
     * @param orientation If 0, will start the search from the
     *      begin->getStart(). Otherwise, from begin->getEnd().
     * @return true if a valid area was found.
     */
    bool buildArea(CurvePtr begin, set<CurveId> &excluded,
            vector<CurveId> &used, map<CurveId, int> &orientations,
            int orientation);

    /**
     * Computes the areas of this graph. This method follows each curve in
     * both directions and calls Area#build for each, in order to find all
     * the areas of the graph.
     */
    void buildAreas();

    /**
     * Merge connected nodes below a minimum distance.
     * Slow, intended for preprocessing.
     * COMPLETELY BROKEN DO NOT USE
     */
#if 0
    void decimate(float minDistance);
#endif

    /**
     * Merge vertex in Curves which are longer than minDistance.
     */
    void decimateCurves(float minDistance);

    /**
     * Adds the connected components of the given graph as subgraphs of this
     * graph. Each connected component of the given graph is associated with
     * the area of this graph that contains its interior points. The bounding
     * box of this area is then added to the connected component. The exterior
     * points of the connected component (i.e., those connected to only one
     * curve) are then moved on this bounding box, and finally this completed
     * connected component is build (see #build).
     *
     * @param subgraph the graph whose connex components must be added as
     * subgraphs of this graph.
     */
    void buildSubgraphs(const Graph &subgraphs);

    /**
     * Build the maximal curves and the areas of this graph. The result is
     * added to the given graph.
     *
     * @param useType see #buildCurves.
     * @param result the graph to which the maximal curves and the areas must
     * be added.
     */
    void build(bool useType, GraphPtr result);

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
     * Moves a node.
     *
     * @param n the node to move.
     * @param p the place to go.
     */
    void moveNode(NodePtr n, const vec2d &p);

    /**
     * Moves a control point or a node on a given curve and returns the list
     * of changed Curves.
     *
     * @param c the curve to edit.
     * @param i the index of the control point (or node, if i is 0 or
     *      c->getSize()-1)  to move on c.
     * @param p the new control point or node position.
     * @param changedCurves List of changed curves.
     */
    virtual void movePoint(CurvePtr c, int i, const vec2d &p,
            set<CurveId> &changedCurves);

    /**
     * Splits a curve by changing a Vertex into a Node.
     *
     * @param c the curve to split.
     * @param i the rank of the Vertex to change into a node.
     * @param changed the list of changed areas & curves.
     * @return the added node.
     */
    virtual NodePtr addNode(CurvePtr c, int i, Graph::Changes &changed);

    /**
     * Merges two curves by changing a Node into a Vertex.

     * @param first the first curve to merge.
     * @param second the second curve.
     * @param p the XY coords of the node to remove.
     * @param changed the list of changed areas & curves.
     * @param selectedPoint After function call, will contain the rank of the
     *      selectedPoint into selectedCurve. Used for editGraphOrthoLayer.
     * @return the resulting curve.
     */
    virtual CurvePtr removeNode(CurvePtr first, CurvePtr second,
            const vec2d &p, Graph::Changes &changed, int &selectedPoint);

    /**
     * Adds a curve from 2 new Points.
     *
     * @param start first point.
     * @param end second point.
     * @param changed the list of changed areas & curves.
     * @return the new Curve.
     */
    virtual CurvePtr addCurve(vec2d start, vec2d end, Graph::Changes &changed);

    /**
     * Adds a curve from 1 existing Node & 1 new Point.
     *
     * @param start first point.
     * @param end second point.
     * @param changed the list of changed areas & curves.
     * @return the new Curve.
     */
    virtual CurvePtr addCurve(NodeId start, vec2d end, Graph::Changes &changed);

    /**
     * Adds a curve from 2 existing Nodes.
     *
     * @param start first point.
     * @param end second point.
     * @param changed the list of changed areas & curves.
     * @return the new Curve.
     */
    virtual CurvePtr addCurve(NodeId start, NodeId end, Graph::Changes &changed);

    /**
     * Removes a Vertex from a curve.
     *
     * @param[inout] curve the Curve from which to remove the point. After
     *     function call, will contain the selected Curve. Used for editGraphOrthoLayer.
     * @param[out] selectedSegment After function call, will contain the
     *     selected segment in selectedCurve.
     * @param[inout] selectedPoint the vertex to remove from in#curve. After
     *     function call, will be -1.
     * @param[out] changed the list of changed areas & curves.
     */
    virtual void removeVertex(CurvePtr &curve, int &selectedSegment,
            int &selectedPoint, Graph::Changes &changed);

    /**
     * Removes a Curve.
     *
     * @param id the id of the curve to remove.
     * @param changed the list of changed areas & curves.
     */
    virtual void removeCurve(CurveId id, Graph::Changes &changed);

    /**
     * Check if this contains the same data as another graph.
     *
     * @param g A graph to compare.
     * @return true if the graphs contains the same data. False otherwise.
     */
    bool equals(Graph* g);

    /**
     * Adds a listener to this graph.
     * Listeners are used to update resources using graphs when a modification
     * occured on it (Example : GraphProducers).
     *
     * @param p a GraphListener based object.
     */
    void addListener(GraphListener *p);

    /**
     * Removes a listener from this graph.
     *
     * @param p a GraphListener.
     */
    void removeListener(GraphListener *p);

    /**
     * Returns the number of GraphListeners for this graph.
     */
    int getListenerCount();

    /**
     * Calls the GraphListener#GraphChanged() method for each listener
     * on this graph.
     */
    void notifyListeners();

    /**
     * Contains all changes on this graph which will be spread amongst
     * child graphs.
     *
     * Added & removed areas.
     * Added & removed curves.
     */
    Changes changes;

    /**
     * Static method to merge 2 successive changes.
     * For example, if a curve was first changed and then deleted, it
     * will just be deleted.
     *
     * @param old the first Graph::Changes.
     * @param c the second Graph::Changes.
     */
    static Graph::Changes merge(Graph::Changes old, Graph::Changes c);

    /**
     * Graph's Version. Allows to know if the Graph needs an update.
     */
    unsigned int version;

    /**
     * Adds a node to this graph.
     *
     * @param p a vector containing the coordinates of the new node.
     * @return the new node.
     */
    virtual NodePtr newNode(const vec2d &p) = 0;

    /**
     * Adds a curve to this graph.
     *
     * @param parent the parent curve (NULL if none).
     * @param setParent if true, the new curve's parent will be set to the
     *      'parent' parameter.
     * @return the new curve.
     */
    virtual CurvePtr newCurve(CurvePtr parent, bool setParent) = 0;

    /**
     * Adds a curve to this graph.
     *
     * @param model a model curve : the new curve will have the same vertices.
     * @param start the start node.
     * @param end the end node.
     * @return the new curve.
     */
    virtual CurvePtr newCurve(CurvePtr model, NodePtr start, NodePtr end) = 0;

    /**
     * Adds an area to this graph.
     *
     * @param parent the parent area (NULL if none).
     * @param setParent if true, the new area's parent will be s et to the
     *      'parent' parameter.
     */
    virtual AreaPtr newArea(AreaPtr parent, bool setParent) = 0;

    /**
     * Returns a new CurvePart whose type depends on the current Graph's type.
     * Default is BasicCurvePart.
     * A new type should be used for curves with more data than normal curves.
     *
     * @param p the whole curve.
     * @param orientation the curve part orientation (see CurvePart#orientation).
     * @param start start of the interval in p to which this part corresponds.
     * @param end end of the interval in p to which this part corresponds.
     */
    virtual CurvePart *createCurvePart(CurvePtr p, int orientation, int start, int end);

    /**
     * Returns a new BasicGraph. Should be overrided for different graph types.
     */
    virtual Graph *createChild();

    /**
     * Returns the Bezier polygon that fits to a given set of digitized points.
     *
     * @param points digitized points.
     * @param[out] output the Bezier polygon.
     * @param error the maximum reconstruction error allowed.
     */
    static void fitCubicCurve(vector<vec2d> points, vector<vec2d> &output, float error);

    /**
     * Checks if a moved point from a curve has an opposite ControlPoint.
     * A ControlPoint should be moved if its opposite is moved, in order
     * to keep the continuity of the Curve.
     *
     * @param p the Curve to which the point belongs.
     * @param i rank of the point to move.
     * @param di degree of the Curve. Determines which points will be checked.
     * @param q coordinates of the point that should move too (opposite) if any.
     * @param id CurveId of the Curve to which q belongs to (if any).
     * @param j rank of the opposite ControlPoint on its Curve.
     */
    static bool hasOppositeControlPoint(CurvePtr p, int i, int di, vec2d &q, CurveId &id, int &j);

protected:
    /**
     * Removes a Node from this graph. This method is called when editing
     * the Graph.
     *
     * @param id the id of a Node that is currently being deleted.
     */
    virtual void removeNode(NodeId id) = 0;

    /**
     * Removes a Curve from this graph. This method is called when editing
     * the Graph.
     *
     * @param id the id of a Curve that is currently being deleted.
     */
    virtual void removeCurve(CurveId id) = 0;

    /**
     * Merges two nodes. Every Curve between them is deleted.
     * One of the node is deleted.
     * Curves neighbouring the deleted Node now belongs to the remaining one.
     *
     * @param ida the id of the remaining Node
     * @param idb the id of the deleted Node
     *
     *  BUGGY AS HELL DO NOT USE
     */
    void mergeNodes(NodeId ida, NodeId idb);

    /**
     * Removes an Area from this graph. This method is called when editing
     * the Graph.
     *
     * @param id the id of an Area that is currently being deleted.
     */
    virtual void removeArea(AreaId id) = 0;

    /**
     * Amount of parameters for each Node in the file from which it is
     * loaded. Default is 2.
     */
    int nParamsNodes;

    /**
     * Amount of parameters for each Curve in the file from which it is
     * loaded. Default is 2.
     */
    int nParamsCurves;

    /**
     * Amount of parameters for each Area in the file from which it is
     * loaded. Default is 2.
     */
    int nParamsAreas;

    /**
     * Amount of parameters for each Curve's Node in the file from which
     * it is loaded. Default is 2.
     */
    int nParamsCurveExtremities;

    /**
     * Amount of parameters for each Curve's Vertex in the file from
     * which it is loaded. Default is 2.
     */
    int nParamsCurvePoints;

    /**
     * Amount of parameters for each Area's Curve in the file from which
     * it is loaded. Default is 2.
     */
    int nParamsAreaCurves;

    /**
     * Amount of parameters for each Subgraph in the file from which
     * it is loaded. Default is 2.
     */
    int nParamsSubgraphs;

private:
    /**
     * Less method for #mapping ordering.
     */
    struct Cmp : public less<vec2d>
    {
        bool operator()(const vec2d &u, const vec2d &v) const;
    };

    /**
     * A map of nodes. Maps the coordinates of the nodes to their pointer.
     */
    map<vec2d, Node*, Cmp> *mapping;

    /**
     * Parent graph. See #getParent().
     */
    Graph* parent;

    /**
     * This graph's bounds. Only computed in #getBounds().
     */
    box2d bounds;

    /**
     * List of listeners on this graph.
     * GraphListeners are used to monitor changes on a Graph.
     */
    vector<GraphListener *> listeners;

    /**
     * Checks if two points are symmetric with respect to another point.
     *
     * @param p first point.
     * @param q point supposed to be the center point.
     * @param r second point.
     * @return true if they are symmetrical.
     */
    static bool isOpposite(const vec2d &p, const vec2d &q, const vec2d &r);

    /**
     * Enlarges a box to make it contain a given Curve.
     * The function can take each di point.
     *
     * @param area the box2d to enlarge.
     * @param p the Curve that will be used to enlarge the box.
     * @param i the rank of the first point we want to be contained in the box.
     * @param di the increment of i. For example, to use only a point every two
     *      points, put 2 (to avoid non-controlPoints points).
     * @return rank of the first encountered point that is not a control Point.
     */
    int enlarge(box2d &area, CurvePtr p, int i, int di);

    /**
     * Scans the list of nodes in order to find a loop.
     *
     * @param prev the previously found node.
     * @param cur the current node.
     * @param result list of nodes that forms an area.
     * @param visited list of visited nodes, that will be excluded from the search.
     * @param invert If true, search backwards.
     * @param useType If true, will only search in curves with type == 0.
     * @param width After function call, will contain the max curve's width.
     */
    static void followHalfCurve(NodePtr prev, NodePtr cur, vector<NodePtr> &result,
            set<CurvePtr> &visited, bool invert, bool useType, float *width);

    /**
     * Follows a given curve in order to find an area.
     *
     * @param path the first curve to follow.
     * @param useType If true, will only search in curves with type == 0.
     * @param visited list of visited curves, that will be excluded from the search.
     * @param width After function call, will contain the max curve's width.
     * @param type After function call, will contain the max curve's width.
     */
    static void followCurve(CurvePtr c, bool useType, set<CurvePtr> &visited,
            float *width, int *type, vector<NodePtr> &result);

    /**
     * Clean method to erase the changes in the graph.
     */
    virtual void clean() = 0;

    friend class BasicGraph;

    friend class LazyGraph;

    friend class LazyArea;

    friend class LazyCurve;

    friend class LazyNode;
};

}

#endif
