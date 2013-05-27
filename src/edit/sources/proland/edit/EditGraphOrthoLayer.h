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

#ifndef _PROLAND_EDITGRAPHORTHOLAYER_H_
#define _PROLAND_EDITGRAPHORTHOLAYER_H_

#include "ork/render/Mesh.h"
#include "ork/render/Program.h"
#include "ork/ui/Window.h"
#include "proland/graph/producer/GraphProducer.h"
#include "proland/terrain/TerrainNode.h"

using namespace ork;

namespace proland
{

/**
 * An OrthoGPUProducer layer to %edit Graph objects.
 * It handles :
 * - Switching between graphs.
 * - Display of non-flattened graph.
 * - Graph-modifying-functions calls.
 * - Updating the corresponding graphProducer tiles.
 * - Creation and update of Tweakbar that enables the user to edit manually everything, as an alternative to keyboard shortcuts.
 * - Mouse & keyboard entries.
 * - The Tweakbar & EventHandler are static in order to be able to handle multiple EditGraphLayers at the same time on a scene.
 * @ingroup edit
 * @authors Antoine Begault, Eric Bruneton, Guillaume Piolat
 */
PROLAND_API class EditGraphOrthoLayer : public TileLayer
{
public:
    /**
     * Data for a given Vertex. Used to pass arguments in TweakBar Callbacks (Creates a link between edited Data and the TweakBar).
     */
    struct VertexData
    {
        /**
         * Index of this Data in EditGraphOrthoLayer#selectedCurveData.
         */
        int i;

        /**
         * The curve containing the Vertex associated to this data.
         */
        Curve* c;

        /**
         * The EditGraphOrthoLayer containing this data.
         */
        EditGraphOrthoLayer* editor;

        /**
         * Creates a new TWVertexData.
         *
         * @param curve the curve containing the Vertex associated to this data.
         * @param e the EditGraphOrthoLayer containing this data.
         * @param rank index of this Data in EditGraphOrthoLayer#selectedCurveData.
         */
        VertexData(Curve *curve, EditGraphOrthoLayer *e, int rank)
        {
            assert(curve != NULL);
            assert(e != NULL);
            i = rank;
            c = curve;
            editor = e;
        }

        /**
         * Moves the vertex to the given coordinates.
         *
         * @param nx new X coordinate.
         * @param ny new Y coordinate.
         */
        void movePoint(double nx, double ny)
        {
            editor->movePoint(nx, ny, i);
            editor->update();
        }

        /**
         * Changes the S coordinate of the Vertex.
         *
         * @param ns new S coordinate.
         */
        void setS(float ns)
        {
            c->setS(i, ns);
            editor->updateSelectedCurve();
        }

        /**
         * Changes the state of the Vertex.
         *
         * @param b whether to set this Vertex as a control point or not.
         */
        void setControlPoint(bool b)
        {
            c->setIsControl(i, b);
            editor->updateSelectedCurve();
        }
    };

    /**
     * Contains Data on the current selection. Creates a link between TweakBar and the edited Curve.
     */
    struct SelectionData
    {
        /**
         * Displayed name of the selected Curve.
         */
        std::string selectedCurve;

        /**
         * Current selected point.
         */
        int selectedPoint;

        /**
         * Current selected segment.
         */
        int selectedSegment;

        /**
         * Current selected Curve.
         */
        Curve *c;

        /**
         * The EditGraphOrthoLayer that needs this Data.
         */
        EditGraphOrthoLayer *editor;

        /**
         * List of Datas for each Vertex of the current selected Curve.
         */
        vector<VertexData> points;

        /**
         * Current mouse position.
         */
        vec3d mousePosition;

        /**
         * Creates a new SelectionData.
         */
        SelectionData()
        {
            editor = NULL;
            selectedCurve = "None";
            c = NULL;
            selectedPoint = -1;
            selectedSegment = -1;
            mousePosition = vec3d(0.0, 0.0, 0.0);
        }

        /**
         * Creates a new TWEditBarDara.
         *
         * @param e the EditGraphOrthoLayer that needs this Data.
         */
        SelectionData(EditGraphOrthoLayer *e)
        {
            editor = e;
            selectedCurve = "None";
            c = NULL;
            selectedPoint = -1;
            selectedSegment = -1;
            mousePosition = vec3d(0.0, 0.0, 0.0);
        }

        void print()
        {
            printf("Editor :%d\n selection: %d:%d:%d [%d]\nMouse Position:%f:%f\n",
                editor == NULL ? (unsigned int) -1 : *(int*) editor, c == NULL ? (unsigned int) -1 : c->getId().id,
                selectedPoint, selectedSegment, (int) points.size(), mousePosition.x, mousePosition.y);
        }
    };

    /**
     * The EventHandler associated to EditGraphOrthoLayer.
     */
    class EditGraphHandler : public EventHandler
    {
    public:
        /**
         * Current edition mode.
         */
        enum edit_mode
        {
            DEFAULT_MODE, // < when doing nothing
            EDIT_MODE, // < when moving anything
            //END_EDIT_MODE, // < when we want the changes to be applied
            SMOOTHING_POINT,  // < when smoothing a single point (moving its tangents)
            //END_SMOOTHING_POINT, // < when we want to apply changes on the smoothed point
            CREATING_SMOOTH_CURVE, // < when creating a smooth curve (upper case + click)
            END_SMOOTH_CURVE // < when we want the smooth curve to be created
        };

        /**
         * Terrain name in xml file.
         */
        string t;

        /**
         * Node containing the terrain.
         */
        SceneNode *terrain;

        /**
         * Terrain on which this EditGraphOrthoLayer is applied.
         */
        TerrainNode *terrainNode;

        /**
         * Manager creating the scene.
         */
        ResourceManager *manager;

        /**
         * EditGraphOrthoLayer associated to this TweakGraphLayer.
         */
        EditGraphOrthoLayer* editor;

        /**
         * Current edition mode.
         */
        int mode;

        /**
         * Determines if we need to call #update() at #redisplay() call (i.e. if a modification occured).
         */
        bool edited;

        /**
         * Last click coordinates.
         */
        vec3d newPos;

        /**
         * Previous point position, used in #undo() method.
         */
        vec3d prevPos;

        /**
         * Previous frame mouse position in screen space.
         */
        vec2i lastScreenPos;

        /**
         * True if #terrain has been initialized.
         */
        bool initialized;

        double lastUpdate;

        /**
         * GLSL Program used to draw editor::displayedPoints.
         */
        static static_ptr<Program> displayPointsProgram;

        /**
         * Current window size.
         */
        static static_ptr<Uniform2f> windowSizeU;

        /**
         * Creates a new EditGraphHandler.
         */
        EditGraphHandler();

        /**
         * Creates a new EditGraphHandler.
         *
         * @param e the associated EditGraphOrthoLayer on which the updates will occur.
         * @param r the RessourceManager used to load terrain Info.
         * @param t the name of the terrain from which we want to load info.
         */
        EditGraphHandler(EditGraphOrthoLayer *e, ptr<ResourceManager> r, string t);

        /**
         * Deletes this EditGraphHandler.
         */
        virtual ~EditGraphHandler();

    //TODO : Choix appliquer a tous graphs ou juste graph edition
        /**
         * Cancels the last point move.
         */
        bool undo();

        /**
         * See EventHandler#redisplay.
         */
        virtual void redisplay(double t, double dt);

        /**
         * See EventHandler#mouseClick.
         */
        virtual bool mouseClick(button b, state s, modifier m, int x, int y);

        /**
         * See EventHandler#mouseMotion.
         */
        virtual bool mouseMotion(int x, int y);

        /**
         * See EventHandler#keyTyped.
         */
        virtual bool keyTyped(unsigned char c, modifier m, int x, int y);

        /**
         * See EventHandler#specialKey.
         */
        virtual bool specialKey(key k, modifier m, int x, int y);

        /**
         * Returns the world position of a given screen coordinate, if it exists (i.e. if the point is on a terrain node).
         */
        vec3d getWorldCoordinates(int x, int y);

        /**
         * Returns the screen position of a given world coordinate, if it exists (i.e. if the point is on a terrain node).
         */
        vec3i getScreenCoordinates(double x, double y, double z);

        /**
         * Updates the state of many values depending of the current selection and what has been changed.
         * Calls graph->notifyListeners if anything changed.
         */
        void update();
    };

    /**
     * Handles the multiple EditGraphHandlers associated to each EditGraphOrthoLayer.
     * There should only be one static instance of this object.
     */
    class EditGraphHandlerList : public EventHandler
    {
    public:
        /**
         * Creates a new EditGraphHandlerList.
         */
        EditGraphHandlerList() : EventHandler("EditGraphHandlerList")
        {
        }

        /**
         * Deletes this EditGraphHandlerList.
         */
        ~EditGraphHandlerList()
        {
            for(map<EditGraphOrthoLayer *, EditGraphOrthoLayer::EditGraphHandler*>::iterator i = handlers.begin(); i != handlers.end(); i++) {
                delete i->second;
            }
            handlers.clear();
        }

        /**
         * Adds an EditGraphHandlerList in the list of handlers.
         */
        void addHandler(EditGraphOrthoLayer *e, EditGraphOrthoLayer::EditGraphHandler *t)
        {
            if ((int) handlers.size() == 0) {
                defaultCurveType = e->getDefaultCurveType();
                defaultCurveWidth = e->getDefaultCurveWidth();
            }
            handlers.insert(make_pair(e, t));
        }

        /**
         * Removes an EditGraphHandlerList from the list of handlers.
         */
        void removeHandler(EditGraphOrthoLayer *e)
        {
            delete handlers[e];
            handlers.erase(e);
        }

        /**
         * Calls #redisplay method for each stored handler.
         */
        void redisplay(double t, double dt)
        {
            MapIterator<EditGraphOrthoLayer *, EditGraphOrthoLayer::EditGraphHandler*> i(handlers);
            while(i.hasNext()) {
                i.next()->redisplay(t, dt);
            }
        }

        /**
         * Calls #mouseClick method for each stored handler.
         */
        bool mouseClick(button b, state s, modifier m, int x, int y)
        {
            bool res = false;
            MapIterator<EditGraphOrthoLayer *, EditGraphOrthoLayer::EditGraphHandler*> i(handlers);
            while(i.hasNext()) {
                res |= i.next()->mouseClick(b, s, m, x, y);
            }

            return res;
        }

        /**
         * Calls #mouseMotion method for each stored handler.
         */
        bool mouseMotion(int x, int y)
        {
            bool res = false;
            MapIterator<EditGraphOrthoLayer *, EditGraphOrthoLayer::EditGraphHandler*> i(handlers);
            while(i.hasNext()) {
                res |= i.next()->mouseMotion(x, y);
            }

            return res;
        }

        /**
         * Calls #keyTyped method for each stored handler.
         */
        bool keyTyped(unsigned char c, modifier m, int x, int y)
        {
            bool res = false;
            MapIterator<EditGraphOrthoLayer *, EditGraphOrthoLayer::EditGraphHandler*> i(handlers);
            while(i.hasNext()) {
                res |= i.next()->keyTyped(c, m, x, y);
            }

            return res;
        }

        /**
         * Calls #specialKey method for each stored handler.
         */
        bool specialKey(key k, modifier m, int x, int y)
        {
            bool res = false;
            MapIterator<EditGraphOrthoLayer *, EditGraphOrthoLayer::EditGraphHandler*> i(handlers);
            while(i.hasNext()) {
                res |= i.next()->specialKey(k, m, x, y);
            }

            return res;
        }

        /**
         * The list of handled handlers.
         */
        map<EditGraphOrthoLayer*, EditGraphOrthoLayer::EditGraphHandler*> handlers;

        int getEditedGraph()
        {
            if ((int) handlers.size() == 0) {
                return -1;
            } else {
                return handlers.begin()->first->getEditedGraph();
            }
        }

        void setEditedGraph(int graph)
        {
            for (map<EditGraphOrthoLayer*, EditGraphOrthoLayer::EditGraphHandler*>::iterator i = handlers.begin(); i != handlers.end(); i++) {
                i->first->setEditedGraph(graph);
                i->first->update();
            }
        }

        int getGraphCount()
        {
            if ((int) handlers.size() == 0) {
                return -1;
            } else {
                return handlers.begin()->first->getGraphCount();
            }
        }

        float getDefaultCurveWidth()
        {
            return defaultCurveWidth;
        }

        int getDefaultCurveType()
        {
            return defaultCurveType;
        }

        void setDefaultCurveWidth(float w)
        {
            defaultCurveWidth = w;
            for (map<EditGraphOrthoLayer*, EditGraphOrthoLayer::EditGraphHandler*>::iterator i = handlers.begin(); i != handlers.end(); i++) {
                i->first->setDefaultCurveWidth(w);
            }
        }

        void setDefaultCurveType(int t)
        {
            for (map<EditGraphOrthoLayer*, EditGraphOrthoLayer::EditGraphHandler*>::iterator i = handlers.begin(); i != handlers.end(); i++) {
                i->first->setDefaultCurveType(t);
            }
            defaultCurveType = t;
        }

        void getTypeNames(vector<string> &e)
        {
            if ((int) handlers.size() == 0) {
                return;
            }
            return handlers.begin()->first->getTypeNames(e);
        }

        /**
         * Contains data on the current selection. see SelectionData class.
         */
        SelectionData selectedCurveData;

        vec3d getWorldCoordinates(EditGraphOrthoLayer *editor, int x, int y)
        {
            return handlers[editor]->getWorldCoordinates(x, y);
        }

        float defaultCurveWidth;

        int defaultCurveType;
    };

    /**
     * Creates a new EditGraphOrthoLayer.
     *
     * @param graphs a vector of GraphProducers that will be handled by this layer.
     * @param layerProgram the GLSL Program to be used to draw the graphs in this
     *      Layer.
     * @param displayLevel the tile level to start display. Tiles whole level is less than displayLevel are not drawn by this Layer,
     *          and graphProducer is not asked to produce the corresponding graph tiles.
     * @param tolerance tolerance parameter for screen selection of points.
     * @param softEdition true to only call the update() method once the user release the mouse when editing.
     *          Usefull for avoiding costly updates when editing.
     * @param softEditionDelay minimum amount of time between two updates if softEdition is false.
     * @param bool deform whether we apply a spherical deformation on the layer or not.
     * @param terrain name of the terrain on which this EditGraphOrthoLayer is located. only required if the terrainNode has been deformed or moved.
     * @param manager the ResourceManager used to create the scene.
     */
    EditGraphOrthoLayer(const vector< ptr<GraphProducer> > &graphs, ptr<Program> layerProg, int displayLevel = 0, float tolerance = 8.0f / 192.0f, bool softEdition = true, double softEditionDelay = 100000.0, bool deform = false, string terrain = "", ptr<ResourceManager> manager = NULL);

    /**
     * Deletes this EditGraphOrthoLayer.
     */
    virtual ~EditGraphOrthoLayer();

    /**
     * Sets tileSize, tileBorder, and rootQuadSize values.
     *
     * @param tileSize current tile size.
     * @param tileBorder size of the tiles' border.
     * @param rootQuadSize size of the root tile.
     */
    virtual void setTileSize(int tileSize, int tileBorder, float rootQuadSize);

    /**
     * Notifies this Layer that the given tile of its TileProducer is in use.
     * This happens when this tile was not previously used, and has been
     * requested with TileProducer#getTile (see also TileCache#getTile).
     *
     * @param level the tile's quadtree level.
     * @param tx the tile's quadtree x coordinate.
     * @param ty the tile's quadtree y coordinate.
     * @param deadline the deadline at which the tile data must be ready. 0 means
     *      the current frame.
     */
    virtual void useTile(int level, int tx, int ty, unsigned int deadline);

    /**
     * Notifies this Layer that the given tile of its TileProducer is unused.
     * This happens when the number of users of this tile becomes null, after a
     * call to TileProducer#putTile (see also TileCache#putTile).
     *
     * @param level the tile's quadtree level.
     * @param tx the tile's quadtree x coordinate.
     * @param ty the tile's quadtree y coordinate.
     */
    virtual void unuseTile(int level, int tx, int ty);

    /**
     * Calls the startCreateTile() method for the currently selected GraphProducer.
     *
     * @param level the tile's quadtree level.
     * @param tx the tile's quadtree x coordinate.
     * @param ty the tile's quadtree y coordinate.
     * @param deadline the deadline at which the tile data must be ready. 0 means
     *      the current frame.
     * @param task the task to produce the tile itself.
     * @param result the %taskgraph that contains the dependencies and tasks to execute.
     *               This method adds the graph-clipping task in this taskgraph.
     */
    virtual void startCreateTile(int level, int tx, int ty, unsigned int deadline, ptr<Task> task, ptr<TaskGraph> result);

    /**
     * Sets the execution context for the Task that produce the tile data.
     * This is only needed for GPU tasks (see Task#begin).
     * The default implementation of this method does nothing.
     */
    virtual void beginCreateTile();

    /**
     * Iterative method which draws a given Graph on a given Mesh.
     *
     * @param g the Graph to be drawn.
     * @param pointSize drawing size of each point.
     * @param mesh the mesh to draw into.
     */
    virtual void drawGraph(const vec3d &tileCoords, GraphPtr g, float pointSize, ptr<Mesh<vec3f, unsigned int> >mesh, vec2d &nx, vec2d &ny, vec2d &lx, vec2d &ly);

    /**
     * Draws the edited graph onto the ortho tile.
     *
     * @param level the tile's quadtree level.
     * @param tx the tile's quadtree x coordinate.
     * @param ty the tile's quadtree y coordinate.
     * @param data the tile to modify.
     */
    virtual bool doCreateTile(int level, int tx, int ty, TileStorage::Slot *data);

    /**
     * Restores the execution context.
     */
    virtual void endCreateTile();

    /**
     * Stops the creation of a tile of this %producer. This method is used for
     * producers that need tiles produced by other producers to create a tile.
     * In these cases this method must release these other tiles with #putTile
     * so that these tiles can be evicted from the cache after use.
     *
     * @param level the tile's quadtree level.
     * @param tx the tile's quadtree x coordinate.
     * @param ty the tile's quadtree y coordinate.
     */
    virtual void stopCreateTile(int level, int tx, int ty);

    /**
     * Returns the Graph edited in this editor.
     */
    vector< ptr<GraphProducer> > getGraphs();

    /**
     * Returns the number of graphs handled by this layer.
     */
    int getGraphCount();

    /**
     * Gets the index of the currently edited graph.
     */
    int getEditedGraph();

    /**
     * Sets the index of the currently edited graph.
     */
    void setEditedGraph(int index);

    /**
     * Selects a node / segment in the graph corresponding to given coords.
     * This function finds node XOR segment (if selected point != NULL, selected segment == NULL for example).
     *
     * @param t the tile to search into.
     * @param x x coord.
     * @param y y coord.
     * @param tolerance tolerance parameter for node / segment search.
     * @param curve will contain the selected curve, NULL if none.
     * @param segment will contain the selected segment's rank in 'curve', -1 if none.
     * @param point will contain the selected point's rank in 'curve', -1 if none.
     * @return true if something was selected.
     */
    bool select(TileCache::Tile *t, double x, double y, float tolerance, GraphPtr &graph, list<AreaId> &areas, CurvePtr &curve, int &segment, int &point);

    /**
     * Interface function for selecting a curve node/segment.
     * Calls #select(float, float, float, CurvePtr&, int&, int&).
     *
     * @param x x coord.
     * @param y y coord.
     * @param tolerance tolerance parameter for node / segment search.
     * @return true if something was selected.
     */
    bool select(double x, double y, float tolerance);

    /**
     * Returns true if a point or segment is currently selected.
     */
    bool selection();

    /**
     * Returns the current selection.
     *
     * @param[out] curve will contain the current selected curve.
     * @param[out] point will contain the current selected point.
     * @param[out] segment will contain the current selected segment.
     */
    void getSelection(CurvePtr &curve, int &point, int &segment);

    /**
     * Changed the current selection.
     *
     * @param[out] curve the newly selected curve.
     * @param[out] point the newly selected point.
     * @param[out] segment the newly selected segment.
     */
    void setSelection(CurvePtr curve, int point, int segment);

    /**
     * Iterative method for finding a Curve at given coordinates.
     *
     * @param p the Graph to search into.
     * @param x x coordinate of the point to search.
     * @param y y coordinate of the point to search.
     * @param[out] graph owner graph of the selected Curve, if any.
     * @param[out] areas see #selectedArea.
     * @param[out] curve will contain the selected curve, NULL if none.
     * @param[out] segment will contain the selected segment's rank in 'curve', -1 if none.
     * @param[out] point will contain the selected point's rank in 'curve', -1 if none.
     * @return true if something was selected.
     */
    bool findCurve(GraphPtr p, double x, double y, float tolerance, GraphPtr &graph, list<AreaId> &areas, CurvePtr &curve, int  &segment, int &point);

    /**
     * Initiates the node / segment search through the graphs.
     *
     * @param x x coord.
     * @param y y coord.
     * @param tolerance tolerance parameter for node / segment search.
     * @param[out] graph owner graph of the selected Curve, if any.
     * @param[out] areas see #selectedArea.
     * @param[out] curve will contain the selected curve, NULL if none.
     * @param[out] segment will contain the selected segment's rank in 'curve', -1 if none.
     * @param[out] point will contain the selected point's rank in 'curve', -1 if none.
     * @return true if something was selected.
     */
    bool select(double x, double y, float tolerance, GraphPtr &graph, list<AreaId> &areas, CurvePtr &curve, int &segment, int &point);

    /**
     * Updates the root Graph#changes field according to the current selection.
     */
    bool updateSelectedCurve();

    /**
     * Moves the selected point to the given coords.
     *
     * @param x x coord.
     * @param y y coord.
     * @param i rank of the point to move in the curve. if i = -1, selectedPoint will be used.
     * @return true if changes occured.
     */
    bool movePoint(double x, double y, int i = -1);

    /**
     * Interface function for adding elements to the graph.
     * Adds elements to the graph depending on selectedCurve, selectedPoint and selectedSegment values :
     * - If selectedCurve == NULL : adds a new curve with 2 new points.
     * - else if selectedPoint == NULL && selectedSegment != NULL : adds a new Vertex into the curve.
     * - else if selectedPoint != NULL && selectedSegment == NULL : adds a new curve with the existing node and a node at pos (XY). If no Node was there, a new one will be created.
     *
     * @param x X coord of the mouse.
     * @param y Y coord of the mouse.
     * @param tolerance tolerance for node search.
     * @return true if changes occured.
     */
    bool add(double x, double y, float tolerance);

    /**
     * Interface function for changing a selected node.
     * If selectedPoint is a node, the method turns it into a ControlPoint, and vice-versa.
     *
     * @return true if changes occured.
     */
    bool change();

    /**
     * Interface function for removing an element.
     * Removes elements to the graph depending on selectedCurve, selectedPoint and selectedSegment values.
     * If a curve if selected  but with no specific node, we remove the whole curve.
     * Otherwise only the Node/Vertex is removed.
     *
     * @return true if changes occured.
     */
    bool remove();

    /**
     * Interface function for inverting the selected curve.
     * Inverts every vertices from the selected Curve.
     *
     * @return true if changes occured.
     */
    bool invert();

    /**
     * Adds a Vertex inside the selected Curve.
     * Also returns the new selection for interface functions.
     *
     * @param x X coord of the new point.
     * @param y Y coord of the new point.
     * @param g the graph to modify.
     * @param[out] curve will contain the selected curve, NULL if none.
     * @param[out] segment will contain the selected segment's rank in 'curve', -1 if none.
     * @param[out] point will contain the selected point's rank in 'curve', -1 if none.
     * @param[out] changes will contain the list of changes that occured in g.
     * @return true if changes occured.
     */
    bool addVertex(double x, double y, GraphPtr g, CurvePtr &curve, int &segment, int &point, Graph::Changes &changes);

    /**
     * Interface function for changing the state of a Vertex.
     * Calls transformVertex(GraphPtr, CurvePtr &, int&, Graph::Changes &);
     */
    bool transformVertex();

    /**
     * Changes the selected point into a node.
     * Also returns the new selection for interface functions.
     *
     * @param g the graph to modify.
     * @param[out] curve will contain the selected curve, NULL if none.
     * @param[out] point will contain the selected point's rank in 'curve', -1 if none.
     * @param[out] changes will contain the list of changes that occured in g.
     * @return true if changes occured.
     */
    bool transformVertex(GraphPtr g, CurvePtr &curve, int &point, Graph::Changes &changes);

    /**
     * Interface function for smoothing the Vertices of the selected Curve.
     * Calls smoothCurve(GraphPtr, CurvePtr &, int&, int&, Graph::Changes &, bool);
     */
    bool smoothCurve(bool smooth);

    /**
     * Interface function for smoothing the selected Node from the selected Curve.
     * Calls smoothNode(GraphPtr, CurvePtr &, int&, int&, Graph::Changes &, bool);
     */
    bool smoothNode(bool smooth);

    /**
     * Adds ControlPoint around the given Node on the Curves where it is necessary to make the Node entirely smooth.
     * Also returns the new selection for interface functions.
     *
     * @param g the graph to modify.
     * @param[out] curve will contain the selected curve, NULL if none.
     * @param[out] point will contain the selected point's rank in 'curve', -1 if none.
     * @param[out] changes will contain the list of changes that occured in g.
     * @param smooth whether we want to smooth or to unsmooth the curve.
     * @return true if changes occured.
     */
    bool smoothNode(GraphPtr g, CurvePtr &curve, int &point, Graph::Changes &changes, bool smooth);

    /**
     * Adds ControlPoint everywhere it is necessary to make the curve entirely smooth.
     * Also returns the new selection for interface functions.
     *
     * @param g the graph to modify.
     * @param[out] curve will contain the selected curve, NULL if none.
     * @param[out] point will contain the selected point's rank in 'curve', -1 if none.
     * @param[out] changes will contain the list of changes that occured in g.
     * @param smooth whether we want to smooth or to unsmooth the curve.
     * @return true if changes occured.
     */
    bool smoothCurve(GraphPtr g, CurvePtr &curve, int &point, int &segment, Graph::Changes &changes, bool smooth);

    /**
     * Interface function for smoothing the Vertices of all the Curves from the currently selected Curve's area.
     * Calls smoothArea(GraphPtr, CurvePtr &, int&, int&, Graph::Changes &, bool);
     */
    bool smoothArea(bool smooth);

    /**
     * Adds ControlPoint everywhere it is necessary to make the curves of an area entirely smooth.
     * Also returns the new selection for interface functions.
     *
     * @param g the graph to modify.
     * @param[out] curve will contain the selected curve, NULL if none.
     * @param[out] point will contain the selected point's rank in 'curve', -1 if none.
     * @param[out] changes will contain the list of changes that occured in g.
     * @param smooth whether we want to smooth or to unsmooth the curve.
     * @return true if changes occured.
     */
    bool smoothArea(GraphPtr g, CurvePtr &curve, int &point, int &segment, Graph::Changes &changes, bool smooth);

    /**
     * Splits the selected curve in two new curves by transforming the selected point into a node.
     * Calls #addNode with current selection parameters.
     */
    bool addNode();

    /**
     * Splits the selected curve in two new curves by transforming the selected point into a node.
     * Also returns the new selection for interface functions.
     *
     * @param g the graph to modify.
     * @param[out] curve will contain the selected curve, NULL if none.
     * @param[out] point will contain the selected point's rank in 'curve', -1 if none.
     * @param[out] changes will contain the list of changes that occured in g.
     * @return true if changes occured.
     */
    bool addNode(GraphPtr g, CurvePtr &curve, int &point, Graph::Changes &changes);

    /**
     * Removes the selected node. The effect depends on the number of curves linked to this node :
     * - if 2 curves = Merges the curves.
     * - else removes all curves and the selected node.
     * Also returns the new selection for interface functions.
     *
     * @param g the graph to modify.
     * @param[out] curve will contain the selected curve, NULL if none.
     * @param[out] segment will contain the selected segment's rank in 'curve', -1 if none.
     * @param[out] point will contain the selected point's rank in 'curve', -1 if none.
     * @param[out] changes will contain the list of changes that occured in g.
     * @return true if changes occured.
     */
    bool removeNode(GraphPtr g, CurvePtr &curve, int &segment, int &point, Graph::Changes &changes);

    /**
     * Adds a curve from selected point to given coords. If given coords doesn't corresponds to a given node, it creates it.
     * Also returns the new selection for interface functions.
     *
     * @param x X coord of second node.
     * @param y Y coord of second node.
     * @param tolerance tolerance parameter for node searching.
     * @param g the graph to modify.
     * @param[out] curve will contain the selected curve, NULL if none.
     * @param[out] point will contain the selected point's rank in 'curve', -1 if none.
     * @param[out] changes will contain the list of changes that occured in g.
     * @return true if changes occured.
     */
    virtual bool addCurve(double x, double y, float tolerance, GraphPtr g, CurvePtr &curve, int &point, Graph::Changes &changes);

    /**
     * Adds a Curve between two new nodes.
     * Also returns the new selection for interface functions.
     *
     * @param x1 X coord of first node.
     * @param y1 Y coord of first node.
     * @param x2 X coord of second node.
     * @param y2 Y coord of second node.
     * @param g the graph to modify.
     * @param[out] curve will contain the selected curve, NULL if none.
     * @param[out] point will contain the selected point's rank in 'curve', -1 if none.
     * @param[out] changes will contain the list of changes that occured in g.
     * @return true if changes occured.
     */
    virtual bool addCurve(double x1, double y1, double x2, double y2, float tolerance, GraphPtr g, CurvePtr &curve, int &segment, int &point, Graph::Changes &changes);

    /**
     * Creates a bezier Curve that fits the points stored in #displayedPoints.
     */
    virtual bool fitCurve();

    /**
     * Removes the selected Curve, if any.
     * Also returns the new selection for interface functions.
     *
     * @param g the graph to modify.
     * @param[out] curve will contain the selected curve, NULL if none.
     * @param[out] changes will contain the list of changes that occured in g.
     * @return true if changes occured.
     */
    bool removeCurve(GraphPtr g, CurvePtr &curve, Graph::Changes &changes);

    /**
     * Invalidates required tiles.
     * Called each time a change occured (if any of the above functions returned true).
     */
    virtual void update();

    /**
     * Returns #tolerance selection parameter.
     */
    float getTolerance() const
    {
        return tolerance;
    }

    /**
     * Returns #selectedPoint.
     */
    int getSelectedPoint() const
    {
        return selectedPoint;
    }

    /**
     * Returns #selectedCurve.
     */
    CurvePtr getSelectedCurve() const
    {
        return selectedCurve;
    }

    /**
     * Returns #selectedSegment.
     */
    int getSelectedSegment() const
    {
        return selectedSegment;
    }

    /**
     * Returns #editGraph.
     */
    ptr<GraphProducer> getEditedGraphPtr() const
    {
        return editGraph;
    }

    /**
     * Returns #defaultCurveType.
     */
    int getDefaultCurveType() const
    {
        return defaultCurveType;
    }

    /**
     * Sets #defaultCurveType.
     */
    void setDefaultCurveType(int t)
    {
        defaultCurveType = t;
    }

    /**
     * Returns #defaultCurveWidth.
     */
    float getDefaultCurveWidth() const
    {
        return defaultCurveWidth;
    }

    /**
     * Sets #defaultCurveWidth.
     */
    void setDefaultCurveWidth(float t)
    {
        defaultCurveWidth= t;
    }

    /**
     * The TweakGraphLayerList which contains each TweakGraphLayer handlers.
     */
    static static_ptr<EditGraphHandlerList> HANDLER;

    /**
     * Returns customized type names for each curve type.
     *
     * @param[out] will contain the names for each type.
     */
    virtual void getTypeNames(vector<string> &names);

    /**
     * List of points displayed on the Tile when drawing. They indicates current edition.
     */
    vector<vec2i> displayedPoints;

    /**
     * The mesh used for drawing curves.
     */
    static static_ptr<Mesh<vec3f, unsigned int> > mesh;

    static ptr<EventHandler> getEventHandler();

protected:

    /**
     * Initializes EditGraphOrthoLayer fields.
     *
     * @param graphs a vector of GraphProducers that will be handled by this layer.
     * @param layerProgram the GLSL Program to be used to draw the graphs in this
     *      Layer.
     * @param displayLevel the tile level to start display. Tiles whole level is less than displayLevel are not drawn by this Layer,
     *          and graphProducer is not asked to produce the corresponding graph tiles.
     * @param tolerance tolerance parameter for screen selection of points.
     * @param softEdition true to only call the update() method once the user release the mouse when editing.
     *          Usefull for avoiding costly updates when editing.
     * @param softEditionDelay minimum amount of time between two updates if softEdition is false.
     * @param deform whether we apply a spherical deformation on the layer or not.
     * @param terrain name of the terrain on which this EditGraphOrthoLayer is located. only required if the terrainNode has been deformed or moved.
     * @param manager the ResourceManager used to create the scene.
     */
    virtual void init(const vector< ptr<GraphProducer> > &graphs, ptr<Program> layerProg, int displayLevel = 0, float tolerance = 8.0f / 192.0f, bool softEdition = true, double softEditionDelay = 100000.0, bool deform = false, string terrain = "", ptr<ResourceManager> manager = NULL);

    /**
     * Creates a new EditGraphOrthoLayer.
     */
    EditGraphOrthoLayer();

    void swap(ptr<EditGraphOrthoLayer> p);

    /**
     * A vector of GraphProducers handled by this layer.
     */
    vector< ptr<GraphProducer> > graphs;

    /**
     * Default Curve Type when creating a new Curve.
     */
    int defaultCurveType;

    /**
     * Default Curve Width when creating a new Curve.
     */
    float defaultCurveWidth;

    /**
     * Tolerance parameter for screen selection of points.
     */
    float tolerance;

    /**
     * Index of currently edited graph in #graphs, or -1 if not graph currently edited.
     */
    int editedGraph;

    /**
     * The GraphProducer of the currently edited graph. This GraphProducer is
     * not directly one of the #graphs elements, because it may need different
     * options (such as no doFlatten).
     */
    ptr<GraphProducer> editGraph;

    /**
     * List of used Tiles. Used to know on which tiles the #useTile() method was called, so they can be released when not used anymore.
     */
    set<TileCache::Tile::Id> usedTiles;

    /**
     * Position of the begining of a Curve being created. curveStart.xy contains the coordinates. curveStart.z indicates whether we are creating a new Curve or not.
     */
    vec3f curveStart;

    /**
     * The tile level to start display. Tiles whole level is less than displayLevel are not drawn by this Layer,
     * and graphProducer is not asked to produce the corresponding graph tiles.
     */
    int displayLevel;

    /**
     * Graph containing the selected Curve. Modified in #select and used in interface functions.
     * This is required for subgraph edition, and always used with #selectedArea if selectedGraph is different from root graph.
     */
    GraphPtr selectedGraph;

    /**
     * Iterative list of areas to which the selected graph belongs.
     * If selectedGraph == editGraph->getRoot(), selectedArea will be empty.
     * Otherwise, it will contain the ordered areas to explore until we can find #selectedGraph.
     * (i.e. root->getArea(selectedArea[0])->getSubgraph()->getArea(selectedArea[1])->getSubgraph()...)
     */
    list<AreaId> selectedArea;

    /**
     * Selected Curve. Modified in #select() and used in interface functions.
     * If NULL, no curve was selected
     */
    CurvePtr selectedCurve;

    /**
     * Selected segment's index in the selected curve. Modified in Xselect() and used in interface functions.
     */
    int selectedSegment;

    /**
     * Selected node's index in the selected curve. Modified in Xselect() and used in interface functions.
     */
    int selectedPoint;

    /**
     * Returns the ox,oy,l coordinates of the given tile.
     */
    vec3d getTileCoords(int level, int tx, int ty);

    /**
     * Uniform used to convert the coordinates of each points into texture coordinates inside the shader.
     */
    ptr<Uniform3f> tileOffsetU;

    /**
     * The GLSL Program to be used to draw the graphs in this Layer.
     */
    ptr<Program> layerProgram;

    /**
     * True to only call the update() method once the user release the mouse when editing.
     * Usefull for avoiding costly updates when editing.
     */
    bool softEdition;

    /**
     * Minimum amount of time between two updates if softEdition is false.
     */
    double softEditionDelay;

    friend class TweakGraphLayer;
};

}

#endif
