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

#include "proland/ui/twbar/TweakGraphLayer.h"

namespace proland
{

TwBar *TweakGraphLayer::contextBar;

/**
 * Curve Width Callbacks
 */
static TW_CALL void SetCurveWidthCallback(const void *value, void *clientData)
{
    EditGraphOrthoLayer::SelectionData e = *(EditGraphOrthoLayer::SelectionData*)clientData;
    e.c->setWidth(*(float*)value);
    e.editor->updateSelectedCurve();
}

static TW_CALL void GetCurveWidthCallback(void *value, void *clientData)
{
    CurvePtr c = (*(EditGraphOrthoLayer::SelectionData*) clientData).c;
    *static_cast<float*> (value) = c == NULL ? -1.f : c->getWidth();
}

/**
 * Curve Type Callbacks
 */
static TW_CALL void SetCurveTypeCallback(const void *value, void *clientData)
{
    EditGraphOrthoLayer::SelectionData e = *(EditGraphOrthoLayer::SelectionData*)clientData;
    e.c->setType(*(int*)value);
    e.editor->updateSelectedCurve();
}

static TW_CALL void GetCurveTypeCallback(void *value, void *clientData)
{
    CurvePtr c = (*(EditGraphOrthoLayer::SelectionData*) clientData).c;
    *static_cast<int*> (value) = c == NULL ? 0 : c->getType();
}

/**
 * Area Info Callbacks
 */
static TW_CALL void SetAreaInfoCallback(const void *value, void *clientData)
{
    EditGraphOrthoLayer::SelectionData e = *(EditGraphOrthoLayer::SelectionData*)clientData;
    e.c->getArea1()->setInfo(*(int*)value);
    e.editor->updateSelectedCurve();
}

static TW_CALL void GetAreaInfoCallback(void *value, void *clientData)
{
    CurvePtr c = (*(EditGraphOrthoLayer::SelectionData*) clientData).c;
    *static_cast<float*> (value) = c == NULL ? -1.f : c->getArea1()->getInfo();
}

/**
 * Vertex Position Callbacks
 */
static TW_CALL void SetVertexXCallback(const void *value, void *clientData)
{
    EditGraphOrthoLayer::VertexData v = *(EditGraphOrthoLayer::VertexData*)clientData;
    v.movePoint(*(float*) value, v.c->getXY(v.i).y);
}

static TW_CALL void GetVertexXCallback(void *value, void *clientData)
{
    EditGraphOrthoLayer::VertexData c = *(EditGraphOrthoLayer::VertexData*)clientData;

    *static_cast<float*> (value) = c.c == NULL ? 0.f : c.c->getXY(c.i).x;
}

static TW_CALL void SetVertexYCallback(const void *value, void *clientData)
{
    EditGraphOrthoLayer::VertexData v = *(EditGraphOrthoLayer::VertexData*)clientData;
    v.movePoint(v.c->getXY(v.i).x, *(float*) value);
}

static TW_CALL void GetVertexYCallback(void *value, void *clientData)
{
    EditGraphOrthoLayer::VertexData c = *(EditGraphOrthoLayer::VertexData*)clientData;
    *static_cast<float*> (value) = c.c == NULL ? 0.f : c.c->getXY(c.i).y;
}

/**
 * Vertex Attributes Callbacks
 */
static TW_CALL void SetVertexSCallback(const void *value, void *clientData)
{
    (*(EditGraphOrthoLayer::VertexData*)clientData).setS(*(float*) value);
}

static TW_CALL void GetVertexSCallback(void *value, void *clientData)
{
    EditGraphOrthoLayer::VertexData c = *(EditGraphOrthoLayer::VertexData*)clientData;
    *static_cast<float*> (value) = c.c == NULL ? 0.f : c.c->getS(c.i);
}

static TW_CALL void SetVertexBOOLCallback(const void *value, void *clientData)
{
    (*(EditGraphOrthoLayer::VertexData*)clientData).setControlPoint(*(bool*) value);
}

static TW_CALL void GetVertexBOOLCallback(void *value, void *clientData)
{
    EditGraphOrthoLayer::VertexData c = *(EditGraphOrthoLayer::VertexData*)clientData;
    *static_cast<bool*> (value) = c.c == NULL ? false : c.c->getIsControl(c.i);
}

/**
 * Edition Callbacks
 */
static TW_CALL void SetEditedGraphCallback(const void *value, void *clientData)
{
    EditGraphOrthoLayer::getEventHandler().cast<EditGraphOrthoLayer::EditGraphHandlerList>()->setEditedGraph(*(int*)value);
}

static TW_CALL void GetEditedGraphCallback(void *value, void *clientData)
{
    *static_cast<int*> (value) = EditGraphOrthoLayer::getEventHandler().cast<EditGraphOrthoLayer::EditGraphHandlerList>()->getEditedGraph();
}

/**
 * Default Values Callbacks
 */
static TW_CALL void SetDefaultCurveTypeCallback(const void *value, void *clientData)
{
    EditGraphOrthoLayer::getEventHandler().cast<EditGraphOrthoLayer::EditGraphHandlerList>()->setDefaultCurveType(*(int*) value);
}

static TW_CALL void GetDefaultCurveTypeCallback(void *value, void *clientData)
{
    *static_cast<int*> (value) = EditGraphOrthoLayer::getEventHandler().cast<EditGraphOrthoLayer::EditGraphHandlerList>()->getDefaultCurveType();
}

static TW_CALL void SetDefaultCurveWidthCallback(const void *value, void *clientData)
{
    EditGraphOrthoLayer::getEventHandler().cast<EditGraphOrthoLayer::EditGraphHandlerList>()->setDefaultCurveWidth(*(float*) value);
}

static TW_CALL void GetDefaultCurveWidthCallback(void *value, void *clientData)
{
    *static_cast<float*> (value) = EditGraphOrthoLayer::getEventHandler().cast<EditGraphOrthoLayer::EditGraphHandlerList>()->getDefaultCurveWidth();
}

/**
 * Delete selection Callback
 */
static TW_CALL void deleteSelectionCallback(void *clientData)
{
    EditGraphOrthoLayer::SelectionData e = *(EditGraphOrthoLayer::SelectionData*)clientData;
    if (e.editor->remove()) {
        e.editor->update();
    }
    TwSetParam(TweakGraphLayer::contextBar, NULL, "visible", TW_PARAM_CSTRING, 1, "false");
}

/**
 * Inverse selection Callback
 */
static TW_CALL void invertSelectionCallback(void *clientData)
{
    EditGraphOrthoLayer::SelectionData e = *(EditGraphOrthoLayer::SelectionData*)clientData;
    if (e.editor->invert()) {
        e.editor->update();
    }
    TwSetParam(TweakGraphLayer::contextBar, NULL, "visible", TW_PARAM_CSTRING, 1, "false");
}

/**
 * Smoothing Callbacks
 */
static TW_CALL void smoothSelectionCallback(void *clientData)
{
    EditGraphOrthoLayer::SelectionData *e = (EditGraphOrthoLayer::SelectionData*)clientData;
    if (e->editor->transformVertex()) {
        e->editor->update();
    }
    TwSetParam(TweakGraphLayer::contextBar, NULL, "visible", TW_PARAM_CSTRING, 1, "false");
}

static TW_CALL void smoothNodeCallback(void *clientData)
{
    EditGraphOrthoLayer::SelectionData *e = (EditGraphOrthoLayer::SelectionData*)clientData;
    if (e->editor->smoothNode(true)) {
        e->editor->update();
    }
    TwSetParam(TweakGraphLayer::contextBar, NULL, "visible", TW_PARAM_CSTRING, 1, "false");
}

static TW_CALL void unsmoothNodeCallback(void *clientData)
{
    EditGraphOrthoLayer::SelectionData *e = (EditGraphOrthoLayer::SelectionData*)clientData;
    if (e->editor->smoothNode(false)) {
        e->editor->update();
    }
    TwSetParam(TweakGraphLayer::contextBar, NULL, "visible", TW_PARAM_CSTRING, 1, "false");
}

static TW_CALL void smoothCurveCallback(void *clientData)
{
    EditGraphOrthoLayer::SelectionData *e = (EditGraphOrthoLayer::SelectionData*)clientData;
    if (e->editor->smoothCurve(true)) {
        e->editor->update();
    }
    TwSetParam(TweakGraphLayer::contextBar, NULL, "visible", TW_PARAM_CSTRING, 1, "false");
}

static TW_CALL void unsmoothCurveCallback(void *clientData)
{
    EditGraphOrthoLayer::SelectionData *e = (EditGraphOrthoLayer::SelectionData*)clientData;
    if (e->editor->smoothCurve(false)) {
        e->editor->update();
    }
    TwSetParam(TweakGraphLayer::contextBar, NULL, "visible", TW_PARAM_CSTRING, 1, "false");
}

static TW_CALL void smoothAreaCallback(void *clientData)
{
    EditGraphOrthoLayer::SelectionData *e = (EditGraphOrthoLayer::SelectionData*)clientData;
        if (e->editor->smoothArea(true)) {
        e->editor->update();
    }
    TwSetParam(TweakGraphLayer::contextBar, NULL, "visible", TW_PARAM_CSTRING, 1, "false");
}

static TW_CALL void unsmoothAreaCallback(void *clientData)
{
    EditGraphOrthoLayer::SelectionData *e = (EditGraphOrthoLayer::SelectionData*)clientData;
    if (e->editor->smoothArea(false)) {
        e->editor->update();
    }
    TwSetParam(TweakGraphLayer::contextBar, NULL, "visible", TW_PARAM_CSTRING, 1, "false");
}

/**
 * Clip/Merge Callback
 */
static TW_CALL void clipAndMergeCurveCallback(void *clientData) // point2node or node2point
{
    EditGraphOrthoLayer::SelectionData *e = (EditGraphOrthoLayer::SelectionData*)clientData;
    if (e->editor->change()) {
        e->editor->update();
    }
    TwSetParam(TweakGraphLayer::contextBar, NULL, "visible", TW_PARAM_CSTRING, 1, "false");
}

/**
 * Specific delete and add Callbacks
 */
static TW_CALL void deleteCurveCallback(void *clientData)
{
    EditGraphOrthoLayer::SelectionData * c = ((EditGraphOrthoLayer::SelectionData*)clientData);
    c->editor->setSelection(c->editor->getSelectedCurve(), -1, 0);
    if(c->editor->remove()) {
        c->editor->update();
    }
    TwSetParam(TweakGraphLayer::contextBar, NULL, "visible", TW_PARAM_CSTRING, 1, "false");
}

static TW_CALL void addPointCallback(void *clientData)
{
    EditGraphOrthoLayer::SelectionData * c = ((EditGraphOrthoLayer::SelectionData*)clientData);
    if (c->editor->add((float)c->mousePosition.x, (float)c->mousePosition.y, c->editor->getTolerance())) {
        c->editor->update();
    }
    TwSetParam(TweakGraphLayer::contextBar, NULL, "visible", TW_PARAM_CSTRING, 1, "false");
}

static TW_CALL void addControlPointCallback(void *clientData)
{
    EditGraphOrthoLayer::SelectionData * c = ((EditGraphOrthoLayer::SelectionData*)clientData);
    if (c->editor->add((float)c->mousePosition.x, (float)c->mousePosition.y, c->editor->getTolerance())) {
        c->c->setIsControl(c->selectedPoint, true);
        c->editor->update();
    }
    TwSetParam(TweakGraphLayer::contextBar, NULL, "visible", TW_PARAM_CSTRING, 1, "false");
}

static TW_CALL void addNodeCallback(void *clientData)
{
    EditGraphOrthoLayer::SelectionData * c = ((EditGraphOrthoLayer::SelectionData*)clientData);
    if (c->editor->add((float)c->mousePosition.x, (float)c->mousePosition.y, c->editor->getTolerance())) {
        c->editor->addNode();
        c->editor->update();
    }
    TwSetParam(TweakGraphLayer::contextBar, NULL, "visible", TW_PARAM_CSTRING, 1, "false");
}

TweakGraphLayer::TweakGraphLayer(bool active)
{
    init(active);
}

TweakGraphLayer::~TweakGraphLayer()
{
    if (contextBar != NULL) {
        TwDeleteBar(contextBar);
        contextBar = NULL;
    }
}

TweakGraphLayer::TweakGraphLayer() : TweakBarHandler()
{
}

void TweakGraphLayer::init(bool active)
{
    TweakBarHandler::init("Graph Editor", EditGraphOrthoLayer::getEventHandler(), active);
    displayContext = HIDDEN;
    initialized = false;
    lastActiveGraph = -1;
}

void TweakGraphLayer::setActive(bool active)
{
    if (eventHandler != NULL) {
        if (!active) {
            lastActiveGraph = eventHandler.cast<EditGraphOrthoLayer::EditGraphHandlerList>()->getEditedGraph();
        }
        eventHandler.cast<EditGraphOrthoLayer::EditGraphHandlerList>()->setEditedGraph(active ? lastActiveGraph : -1);
    }
    TweakBarHandler::setActive(active);
}

void TweakGraphLayer::createTweakBar()
{
    if (contextBar != NULL) {
        TwDeleteBar(contextBar);
    }
    contextBar = TwNewBar("EditGraphLayerContextBar");

    TwSetParam(contextBar, NULL, "label", TW_PARAM_CSTRING, 1, "Edit");
    TwSetParam(contextBar, NULL, "iconified", TW_PARAM_CSTRING, 1, "false");
    TwSetParam(contextBar, NULL, "resizable", TW_PARAM_CSTRING, 1, "false");
    TwSetParam(contextBar, NULL, "visible", TW_PARAM_CSTRING, 1, "false");
    TwSetParam(contextBar, NULL, "movable", TW_PARAM_CSTRING, 1, "false");
    TwSetParam(contextBar, NULL, "iconifiable", TW_PARAM_CSTRING, 1, "false");

    selectedCurveData = EditGraphOrthoLayer::SelectionData(NULL);

    initialized = true;
}

void TweakGraphLayer::redisplay(double t, double dt, bool &needUpdate)
{
    if (!initialized) {
        createTweakBar();
        initialized = true;
    }
    if (eventHandler == NULL) {
        eventHandler = EditGraphOrthoLayer::getEventHandler();
    }

    TweakBarHandler::redisplay(t, dt, needUpdate);

    if (eventHandler == NULL) {
        if (displayContext >= CLICK) {
            closeMenu();
        }
        return;
    }

    if (eventHandler == NULL) {
        return;
    }

    EditGraphOrthoLayer::SelectionData data = eventHandler.cast<EditGraphOrthoLayer::EditGraphHandlerList>()->selectedCurveData;
    if (data.c == NULL) {
        if (displayContext >= CLICK) {
            closeMenu();
        }
    }

    bool newCurve = ((data.c != selectedCurveData.c) || (data.selectedPoint != selectedCurveData.selectedPoint) || (data.selectedSegment != selectedCurveData.selectedSegment));
    selectedCurveData = data;
    needUpdate |= newCurve;

    if (displayContext == CLICK) {
        int visible = false;
        TwGetParam(contextBar, NULL, "visible", TW_PARAM_INT32, 1, &visible);
        if (visible && data.c == NULL) {
            closeMenu();
        } else {
            displayMenu(menuPos.x, menuPos.y);
        }
    }
}

void TweakGraphLayer::displayCurveInfo(TwBar *bar, EditGraphOrthoLayer::SelectionData* curveData)
{
    // adding callbacks for editing width & type
    char groupName[25];
    sprintf(groupName, " Group='CurveData' ");
    TwAddVarCB(bar, "width", TW_TYPE_FLOAT, SetCurveWidthCallback, GetCurveWidthCallback, curveData, groupName);
    TwAddVarCB(bar, "Type", TW_TYPE_INT32, SetCurveTypeCallback, GetCurveTypeCallback, curveData, groupName);
    if (curveData->c->getArea1() != NULL) {
        TwAddVarCB(bar, "AreaInfo", TW_TYPE_FLOAT, SetAreaInfoCallback, GetAreaInfoCallback, curveData, groupName);
    }
    char pointGroup[20];
    char coord[30];
    char def[100];
    int count = 0;
    bool opened;

    // adding callbacks for editing points
    for(vector<EditGraphOrthoLayer::VertexData>::iterator i = curveData->points.begin(); i != curveData->points.end(); i++) {

        sprintf(pointGroup, "Point%d", count);

        if ((curveData->selectedPoint == count) ||
            (curveData->selectedSegment != -1 && (curveData->selectedSegment == count || curveData->selectedSegment == count - 1)))
        {
            opened = true;
        } else {
            opened = false;
        }
        sprintf(def, " %s/Point%d label='Point %d' opened=%s group='Points' ", TwGetBarName(bar), count, count, opened ? "true" : "false");

        sprintf(coord, "EDITX%d", count);
        TwAddVarCB(bar, coord, TW_TYPE_FLOAT, SetVertexXCallback, GetVertexXCallback, &(*i), NULL);
        TwSetParam(bar, coord, "group", TW_PARAM_CSTRING, 1, pointGroup);
        TwSetParam(bar, coord, "label", TW_PARAM_CSTRING, 1, "X");
        TwSetParam(bar, coord, "readonly", TW_PARAM_CSTRING, 1, "false");

        sprintf(coord, "EDITY%d", count);
        TwAddVarCB(bar, coord, TW_TYPE_FLOAT, SetVertexYCallback, GetVertexYCallback, &(*i), NULL);
        TwSetParam(bar, coord, "group", TW_PARAM_CSTRING, 1, pointGroup);
        TwSetParam(bar, coord, "label", TW_PARAM_CSTRING, 1, "Y");
        TwSetParam(bar, coord, "readonly", TW_PARAM_CSTRING, 1, "false");

        sprintf(coord, "EDITSCOORD%d", count);
        TwAddVarCB(bar, coord, TW_TYPE_FLOAT, SetVertexSCallback, GetVertexSCallback, &(*i), NULL);
        TwSetParam(bar, coord, "group", TW_PARAM_CSTRING, 1, pointGroup);
        TwSetParam(bar, coord, "label", TW_PARAM_CSTRING, 1, "S");
        TwSetParam(bar, coord, "readonly", TW_PARAM_CSTRING, 1, "false");

        sprintf(coord, "EDITISCONTROL%d", count);
        TwAddVarCB(bar, coord, TW_TYPE_BOOL32, SetVertexBOOLCallback, GetVertexBOOLCallback, &(*i), NULL);
        TwSetParam(bar, coord, "group", TW_PARAM_CSTRING, 1, pointGroup);
        TwSetParam(bar, coord, "label", TW_PARAM_CSTRING, 1, "isControl");
        TwSetParam(bar, coord, "readonly", TW_PARAM_CSTRING, 1, "false");

        count++;
        TwDefine(def);
    }

    // Defining groups
    sprintf(def, " %s/Points readonly=false group='CurveData' ", TwGetBarName(bar));
    TwDefine(def);
    sprintf(def, " %s/CurveData readonly=false label='Curve %d Data' group='GraphEdition' ", TwGetBarName(bar), curveData->c->getId().id);
    TwDefine(def);
    sprintf(def, " %s/GraphEdition readonly=false label='Graph Edition' ", TwGetBarName(bar));
    TwDefine(def);
}

void TweakGraphLayer::updateBar(TwBar *bar)
{
    if (eventHandler == NULL) {
        return;
    }
    EditGraphOrthoLayer::EditGraphHandlerList* e = eventHandler.cast<EditGraphOrthoLayer::EditGraphHandlerList>().get();
    if (e == NULL) {
        return;
    }
    // Adding the list of edited graphs
    TwEnumVal *graphNames = new TwEnumVal[e->getGraphCount() + 1];
    int count = 0;

    vector< ptr<GraphProducer> > graphs = e->handlers.begin()->first->getGraphs();
    for (vector<ptr<GraphProducer> >::iterator i = graphs.begin(); i != graphs.end(); i++) {
        graphNames[count].Value = count;
        graphNames[count].Label =(*i)->getName().c_str();
        count++;
    }
    graphNames[count].Value = -1;
    graphNames[count].Label = "No Edition";

    TwType graphNameType = TwDefineEnum("GraphName", graphNames, e->getGraphCount() + 1);
    TwAddVarCB(bar, "EditedGraph", graphNameType, SetEditedGraphCallback, GetEditedGraphCallback, NULL, " label='Edited Graph' group='GraphEdition' key='e'");

    vector<string> typeNamesList;
    e->getTypeNames(typeNamesList);

    TwEnumVal *typeNames = new TwEnumVal[(int) typeNamesList.size()];
    for (int i = 0; i < (int) typeNamesList.size(); i++) {
        typeNames[i].Value = i;
        typeNames[i].Label = typeNamesList[i].c_str();
    }
    TwType typeNamesType = TwDefineEnum("typeName", typeNames, typeNamesList.size());
    TwAddVarCB(bar, "curveDefaultType", typeNamesType, SetDefaultCurveTypeCallback, GetDefaultCurveTypeCallback, NULL, " label='Default Curve Type' group='GraphEdition' ");
    TwAddVarCB(bar, "curveDefaultWidth", TW_TYPE_FLOAT, SetDefaultCurveWidthCallback, GetDefaultCurveWidthCallback, NULL, " label='Default Curve Width' group='GraphEdition' ");

    delete[] typeNames;
    delete[] graphNames;

    // Showing the current selection
    TwAddVarRO(bar, "SCURVE", TW_TYPE_STDSTRING, &selectedCurveData.selectedCurve,
           " label='Selected Curve' help='Currently selected Curve.' group='GraphEdition' ");

    selectedCurveData.points.clear();
    if (selectedCurveData.c == NULL) {
        selectedCurveData.c = NULL;
        selectedCurveData.selectedCurve = "None";
    } else {
        // getting data for the current selected curve
        ostringstream os;
        os << selectedCurveData.c->getId().id;
        selectedCurveData.selectedCurve = os.str();
        for (int i = 0; i < selectedCurveData.c->getSize(); i++) {
            Vertex v = selectedCurveData.c->getVertex(i);
            selectedCurveData.points.push_back(EditGraphOrthoLayer::VertexData(selectedCurveData.c, selectedCurveData.editor, i));
        }

        displayCurveInfo(bar, &selectedCurveData);
    }
}

bool TweakGraphLayer::mouseClick(EventHandler::button b, EventHandler::state s, EventHandler::modifier m, int x, int y, bool &needUpdate)
{
    if ((b == EventHandler::LEFT_BUTTON) && ((m & EventHandler::SHIFT) == 0) && ((m & EventHandler::ALT) == 0) && ((m & EventHandler::CTRL) == 0)) {
        if (displayContext >= CLICK) {
        }
    } else if ((b == EventHandler::RIGHT_BUTTON) && ((m & EventHandler::SHIFT) == 0) && ((m & EventHandler::ALT) == 0)) { // create a new menu
        if (s == EventHandler::DOWN) {
            menuPos = vec2i(x, y);
            displayContext = CLICK;
        }
    }
    return TweakBarHandler::mouseClick(b, s, m, x, y, needUpdate);
}

void TweakGraphLayer::closeMenu()
{
    displayContext = HIDDEN;
    TwSetParam(contextBar, NULL, "visible", TW_PARAM_CSTRING, 1, "false");
}

void TweakGraphLayer::displayMenu(int mousePosX, int mousePosY)
{
    displayContext = DISPLAY_MENU;
    TwRemoveAllVars(contextBar);
    TwSetParam(contextBar, NULL, "visible", TW_PARAM_CSTRING, 1, "true");
    TwSetParam(contextBar, NULL, "iconifiable", TW_PARAM_CSTRING, 1, "false");
    TwSetParam(contextBar, NULL, "movable", TW_PARAM_CSTRING, 1, "false");
    TwSetParam(contextBar, NULL, "resizable", TW_PARAM_CSTRING, 1, "false");
    int position[2] = {mousePosX, mousePosY};
    TwSetParam(contextBar, NULL, "position", TW_PARAM_INT32, 2, position);

    int point = selectedCurveData.selectedPoint;//getSelectedPoint();
    CurvePtr curve = selectedCurveData.c;

    int size[2] = {150, 0};
    TwAddButton(contextBar, "deleteSelectionButton", deleteSelectionCallback, &selectedCurveData, " label='Delete' key=END ");
    TwAddButton(contextBar, "inverseSelectionButton", invertSelectionCallback, &selectedCurveData, " label='Invert' ");

    TwAddSeparator(contextBar, NULL, NULL);
    if (curve->getArea1() != NULL) {
        TwAddButton(contextBar, "smoothAreaButton", smoothAreaCallback, &selectedCurveData, " label='Smooth Area' ");
        TwAddButton(contextBar, "unsmoothAreaButton", unsmoothAreaCallback, &selectedCurveData, " label='Unsmooth Area' ");
        size[1] += 2 * 28;
    }
    TwAddButton(contextBar, "smoothCurveButton", smoothCurveCallback, &selectedCurveData, " label='Smooth Curve' ");
    TwAddButton(contextBar, "unsmoothCurveButton", unsmoothCurveCallback, &selectedCurveData, " label='Unsmooth Curve' ");
    size[1] += 4 * 28;

    if (point != -1) {
        if (point > 0 && point < curve->getSize() - 1) {
            vec2d p = curve->getXY(point - 1);
            vec2d q = curve->getXY(point);
            vec2d r = curve->getXY(point + 1);
            float d = ((p + r) * 0.5 - q).squaredLength();
            if (!curve->getIsControl(point - 1) || !curve->getIsControl(point + 1) || d >= 0.1f) {
                TwAddButton(contextBar, "smoothPointButton", smoothSelectionCallback, &selectedCurveData, "label='Smooth Point' key=HOME ");
            } else {
                TwAddButton(contextBar, "smoothPointButton", smoothSelectionCallback, &selectedCurveData, "label='Unsmooth Point' key=HOME ");
            }
            TwAddSeparator(contextBar, NULL, NULL);
            TwAddButton(contextBar, "clipCurveButton", clipAndMergeCurveCallback, &selectedCurveData,  "label='Clip Curve' ");
            TwAddButton(contextBar, "deleteCurveButton", deleteCurveCallback, &selectedCurveData,  "label='Delete Curve' ");
            //size[1] = 100;
            size[1] += 3 * 28;
        } else {
            NodePtr n = point == 0 ? curve->getStart() : curve->getEnd();
            TwAddButton(contextBar, "smoothPointButton", smoothNodeCallback, &selectedCurveData, "label='Smooth Node' ");
            TwAddButton(contextBar, "unsmoothPointButton", unsmoothNodeCallback, &selectedCurveData, "label='Unsmooth Node' ");
            TwAddSeparator(contextBar, NULL, NULL);
            if (n->getCurveCount() > 1) {
                TwAddButton(contextBar, "mergeCurveButton", clipAndMergeCurveCallback, &selectedCurveData,  "label='Merge Curves' ");
                //size[1] = 85;
                size[1] += 1 * 28;
            }
            TwAddButton(contextBar, "deleteCurveButton", deleteCurveCallback, &selectedCurveData,  "label='Delete Curve' ");
            size[1] += 3 * 28;
        }
    } else {
        TwAddSeparator(contextBar, NULL, NULL);
        TwAddButton(contextBar, "addPointButton", addPointCallback, &selectedCurveData,  "label='Add Point' key=INSERT ");
        TwAddButton(contextBar, "addControlPointButton", addControlPointCallback, &selectedCurveData,  "label='Add ControlPoint' ");
        TwAddButton(contextBar, "addNodeButton", addNodeCallback, &selectedCurveData,  "label='Add Node' ");
        //size[1] = 100;
        size[1] += 3 * 25;
    }

    TwSetParam(contextBar, NULL, "size", TW_PARAM_INT32, 2, size);
}

class TweakGraphLayerResource : public ResourceTemplate<40, TweakGraphLayer>
{
public:
    TweakGraphLayerResource(ptr<ResourceManager> manager, const string &name, ptr<ResourceDescriptor> desc,
            const TiXmlElement *e = NULL) :
        ResourceTemplate<40, TweakGraphLayer> (manager, name, desc)
    {
        e = e == NULL ? desc->descriptor : e;
        checkParameters(desc, e, "name,active,");

        bool active = true;
        if (e->Attribute("active") != NULL) {
            active = strcmp(e->Attribute("active"), "true") == 0;
        }
        init(active);
    }

};

extern const char tweakGraphLayer[] = "tweakGraphLayer";

static ResourceFactory::Type<tweakGraphLayer, TweakGraphLayerResource> TweakGraphLayerType;

}
