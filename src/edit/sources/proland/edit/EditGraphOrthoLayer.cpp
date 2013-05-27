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

#include "proland/edit/EditGraphOrthoLayer.h"

#include "ork/resource/ResourceTemplate.h"
#include "ork/render/FrameBuffer.h"
#include "ork/scenegraph/SceneManager.h"

#include "proland/producer/ObjectTileStorage.h"
#include "proland/graph/Area.h"
#include "proland/graph/GraphListener.h"

#define INF 1e9

using namespace ork;

namespace proland
{

///////////////////////////////////////////////
//////////// TWEAKBAR related stuff ///////////
///////////////////////////////////////////////

//EditGraphOrthoLayer::SelectionData EditGraphOrthoLayer::EditGraphHandler::selectedCurveData;

const char * displayPointsShader = "\
uniform vec2 winSize;\n\
#ifdef _VERTEX_\n\
layout (location = 0) in vec2 vertex;\n\
void main() {\n\
    gl_Position = vec4((vertex / winSize) * 2.0 - 1.0, 0.0, 1.0);\n\
    gl_Position.y = -gl_Position.y;\n\
}\n\
#endif\n\
#ifdef _FRAGMENT_\n\
layout (location = 0) out vec4 data;\n\
void main() {\n\
    data = vec4(0.0, 1.0, 1.0, 1.0);\n\
}\n\
#endif\n\
";

static_ptr<Program> EditGraphOrthoLayer::EditGraphHandler::displayPointsProgram(NULL);
static_ptr<Uniform2f> EditGraphOrthoLayer::EditGraphHandler::windowSizeU(NULL);

static_ptr<Mesh<vec3f, unsigned int> > EditGraphOrthoLayer::mesh(NULL);

EditGraphOrthoLayer::EditGraphHandler::EditGraphHandler() : EventHandler("EditGraphOrthoLayer")
{
    mode = DEFAULT_MODE;
    edited = false;
    newPos = vec3d(0.0, 0.0, 0.0);
    prevPos = vec3d(0., 0., 0.);
    manager = NULL;
    t = "";
    terrain = NULL;
    terrainNode = NULL;
    editor = NULL;
    initialized = false;
    if (displayPointsProgram == NULL) {
        displayPointsProgram = new Program(new Module(330, displayPointsShader));
        windowSizeU = displayPointsProgram->getUniform2f("winSize");
    }
    this->lastUpdate = 0;
}

EditGraphOrthoLayer::EditGraphHandler::EditGraphHandler(EditGraphOrthoLayer *e, ptr<ResourceManager> r, string t) : EventHandler("EditGraphOrthoLayer")
{
    mode = DEFAULT_MODE;
    edited = false;
    newPos = vec3d(0.0, 0.0, 0.0);
    prevPos = vec3d(0., 0. ,0.);
    manager = r.get();
    editor = e;
    terrain = NULL;
    terrainNode = NULL;
    this->t = t;
    initialized = false;
    if (displayPointsProgram == NULL) {
        displayPointsProgram = new Program(new Module(330, displayPointsShader));
        windowSizeU = displayPointsProgram->getUniform2f("winSize");
    }
    this->lastUpdate = 0;
}

EditGraphOrthoLayer::EditGraphHandler::~EditGraphHandler()
{
}

//TODO : Choix appliquer a tous graphs ou juste graph edition
bool EditGraphOrthoLayer::EditGraphHandler::undo()
{
    if (prevPos.z == 1.f) {
        editor->movePoint(prevPos.x, prevPos.y);
        update();
        return true;
    }
    return false;
}

void EditGraphOrthoLayer::EditGraphHandler::redisplay(double t, double dt)
{
    if (initialized == false) {
        terrain = manager->loadResource(this->t).cast<SceneNode>().get();
        terrainNode = terrain->getField("terrain").cast<TerrainNode>().get();
        initialized = true;
    }

    if ((int) editor->displayedPoints.size() > 1) {
        ptr<FrameBuffer> fb = SceneManager::getCurrentFrameBuffer();
        fb->setDepthTest(false);

        vec4<GLint> vp = fb->getViewport();
        windowSizeU->set(vec2f(vp.z, vp.w));
        EditGraphOrthoLayer::mesh->clear();
        EditGraphOrthoLayer::mesh->setMode(LINES);
        for (int i = 0; i < (int) editor->displayedPoints.size(); i++) {
            vec2i v = editor->displayedPoints[i];
            EditGraphOrthoLayer::mesh->addVertex(vec3f(v.x, v.y, 0.f));
        }
        fb->draw(displayPointsProgram, *(EditGraphOrthoLayer::mesh));
        fb->setDepthTest(true);
    }

    if (editor->softEdition == false && lastUpdate < t - editor->softEditionDelay) {
        bool res = true;
        switch(mode) {
            case CREATING_SMOOTH_CURVE: {
                editor->fitCurve();
                editor->displayedPoints.clear();
                break;
            }
            case EDIT_MODE: {
                newPos = getWorldCoordinates(lastScreenPos.x, lastScreenPos.y);
                if (newPos.x != INF) {
                    editor->movePoint((float) newPos.x, (float) newPos.y);
                }
                editor->displayedPoints.clear();
                break;
            }
            case SMOOTHING_POINT: {
                vec2d pos1 = getWorldCoordinates(editor->displayedPoints[0].x, editor->displayedPoints[0].y).xy();
//                    vec2f pos2 = getWorldCoordinates(editor->displayedPoints[3].x, editor->displayedPoints[3].y).xy().cast<float>();
                vec2d pos2 = vec2d(2., 2.) * editor->selectedCurve->getXY(editor->selectedPoint) - pos1;
                vec2d a, b;
                editor->editGraph->getRoot()->changes.clear();
                if (!editor->selectedCurve->getIsSmooth(editor->selectedPoint, a, b) && editor->selectedCurve->getIsControl(editor->selectedPoint) == false) {
                    if ((pos1 - a).length() < (pos1 - b).length()) {
                        editor->selectedCurve->addVertex(pos1, editor->selectedPoint - 1, true);
                        editor->selectedCurve->addVertex(pos2, editor->selectedPoint + 1, true);
                    } else {
                        editor->selectedCurve->addVertex(pos2, editor->selectedPoint - 1, true);
                        editor->selectedCurve->addVertex(pos1, editor->selectedPoint + 1, true);
                    }
                    editor->selectedCurve->computeCurvilinearCoordinates();
                    editor->editGraph->getRoot()->changes.addedCurves.insert(editor->selectedCurve->getId());
                    editor->editGraph->getRoot()->changes.removedCurves.insert(editor->selectedCurve->getId());
                    editor->editGraph->getRoot()->getAreasFromCurves(editor->editGraph->getRoot()->changes.addedCurves, editor->editGraph->getRoot()->changes.addedAreas);
                    editor->editGraph->getRoot()->getAreasFromCurves(editor->editGraph->getRoot()->changes.removedCurves, editor->editGraph->getRoot()->changes.removedAreas);
                }
                editor->displayedPoints.clear();
                mode = EDIT_MODE;
                break;
            }
            default: {
                res = false;
                break;
            }
        }
        if (res) {
            update();
        }
        edited = true;
        lastUpdate = t;
    }
}

bool EditGraphOrthoLayer::EditGraphHandler::mouseClick(button b, state s, modifier m, int x, int y)
{
    if (editor->getEditedGraph() == -1) { // if no editor, no edition.
        mode = DEFAULT_MODE;
        return false;
    }

    if ((b == LEFT_BUTTON) && ((m & ALT) == 0)) { // left click
        if (s == DOWN) { // clicking
            newPos = getWorldCoordinates(x, y);
            bool oldSelection = editor->selection();
            if ((m & SHIFT) != 0) { // if click + upper case -> create a smooth curve.
                lastScreenPos = vec2i(x, y);
                mode = CREATING_SMOOTH_CURVE;
                editor->displayedPoints.clear();
                editor->displayedPoints.push_back(vec2i(x, y));
                edited = true;
            } else {
                if (editor->select((float) newPos.x, (float) newPos.y, editor->tolerance)) {
                    if (editor->selectedPoint != -1)  {
                        vec2d v = editor->selectedCurve->getXY(editor->selectedPoint);
                        if ((!oldSelection || ((vec2d(prevPos.x, prevPos.y) - v).length() > editor->tolerance))) {
                            newPos = vec3d(v.x, v.y, 1.0f);
                        } else {
                            newPos.x = INF;
                        }
                        prevPos = vec3d(v.x, v.y, 1.0f);
                    }
                    if ((m & CTRL) == 0) {
                        mode = EDIT_MODE;
                    } else {
                        int point = editor->selectedPoint;
                        CurvePtr c = editor->selectedCurve;
                        if (point != -1 && point != 0 && point != c->getSize() - 1) { // adding new control points
                            mode = SMOOTHING_POINT;
                            lastScreenPos = getScreenCoordinates(c->getXY(point).x, c->getXY(point).y, newPos.z).xy();//vec2i(x, y);
                        }
                    }
                    editor->displayedPoints.clear();
                    editor->editGraph->getRoot()->changes.clear();
                    edited = true;
                    update();
                    return true;
                } else if (oldSelection) {
                    mode = DEFAULT_MODE;
                    prevPos.z = 0.f;
                    editor->editGraph->getRoot()->changes.clear();
                    edited = false;
                    update();
                    return false;
                } else {
                    edited = false;
                    mode = DEFAULT_MODE;
                }
            }
        } else { // releasing -> this is where we really apply changes
            bool res = true;
            switch(mode) {
                case CREATING_SMOOTH_CURVE: {
                    editor->fitCurve();
                    editor->displayedPoints.clear();
                    break;
                }
                case EDIT_MODE: {
                    newPos = getWorldCoordinates(x, y);
                    if (newPos.x != INF) {
                        editor->movePoint((float) newPos.x, (float) newPos.y);
                    }
                    editor->displayedPoints.clear();
                    break;
                }
                case SMOOTHING_POINT: {
                    vec2d pos1 = getWorldCoordinates(editor->displayedPoints[0].x, editor->displayedPoints[0].y).xy();
//                    vec2f pos2 = getWorldCoordinates(editor->displayedPoints[3].x, editor->displayedPoints[3].y).xy().cast<float>();
                    vec2d pos2 = vec2d(2., 2.) * editor->selectedCurve->getXY(editor->selectedPoint) - pos1;
                    vec2d a, b;
                    editor->editGraph->getRoot()->changes.clear();
                    if (!editor->selectedCurve->getIsSmooth(editor->selectedPoint, a, b) && editor->selectedCurve->getIsControl(editor->selectedPoint) == false) {
                        if ((pos1 - a).length() < (pos1 - b).length()) {
                            editor->selectedCurve->addVertex(pos1, editor->selectedPoint - 1, true);
                            editor->selectedCurve->addVertex(pos2, editor->selectedPoint + 1, true);
                        } else {
                            editor->selectedCurve->addVertex(pos2, editor->selectedPoint - 1, true);
                            editor->selectedCurve->addVertex(pos1, editor->selectedPoint + 1, true);
                        }
                        editor->selectedCurve->computeCurvilinearCoordinates();
                        editor->editGraph->getRoot()->changes.addedCurves.insert(editor->selectedCurve->getId());
                        editor->editGraph->getRoot()->changes.removedCurves.insert(editor->selectedCurve->getId());
                        editor->editGraph->getRoot()->getAreasFromCurves(editor->editGraph->getRoot()->changes.addedCurves, editor->editGraph->getRoot()->changes.addedAreas);
                        editor->editGraph->getRoot()->getAreasFromCurves(editor->editGraph->getRoot()->changes.removedCurves, editor->editGraph->getRoot()->changes.removedAreas);
                    }
                    editor->displayedPoints.clear();
                    break;
                }
                default: {
                    res = false;
                    break;
                }
            }
            if (res) {
                update();
            }
            edited = false;
            mode = DEFAULT_MODE;
            return res;
        }
    } else if ((b == RIGHT_BUTTON) && ((m & SHIFT) == 0) && ((m & ALT) == 0)) {
        if (s == DOWN && editor->editedGraph != -1) {
            newPos = getWorldCoordinates(x, y);
            if ((m & CTRL) == 0) {
                editor->select((float) newPos.x, (float) newPos.y, editor->tolerance);
                editor->editGraph->getRoot()->changes.clear();
                edited = false;
                update();
                mode = DEFAULT_MODE;
                return false;
            }
        }
    }

    return false;
}

bool EditGraphOrthoLayer::EditGraphHandler::mouseMotion(int x, int y)
{ // TODO UPDATE DISPLAYEDPOINTS
    switch(mode) {
        case EDIT_MODE: {
            lastScreenPos = vec2i(x, y);
            newPos = getWorldCoordinates(x, y);
            editor->displayedPoints.clear();
            int point = editor->selectedPoint;
            CurvePtr curve = editor->selectedCurve;
            if (point != -1) {
                if (point !=0 && point != curve->getSize() - 1) { // point
                    if (curve->getIsControl(point)) { // control point
                        vec2d a, b;
                        int p = 0;
                        int index = -1;
                        vec2d q;
                        CurveId id;
                        id.id = NULL_ID;
                        if (curve->getIsSmooth(point - 1, a, b)) {
                            p = -1;
                        } else if (curve->getIsSmooth(point + 1, a, b)) {
                            p = 1;
                        } else { // if the two neighboring points are not "smoothed", one of them MUST be a smoothed node.
                            if (point == 1) {
                                if (Graph::hasOppositeControlPoint(curve, 0, - 1, q, id, index)) {
                                    p = -1;
                                }
                            }
                            if (p == 0 && point == curve->getSize() - 2) {
                                if (Graph::hasOppositeControlPoint(curve, curve->getSize() - 1, 1, q, id, index)) {
                                    p = 1;
                                }
                            }
                        }
                        bool smoothed = p != 0;
                        if (p == 0) {
                            p = 1;
                        }
                        vec2i v1 = getScreenCoordinates(curve->getXY(point - p).x, curve->getXY(point - p).y, newPos.z).xy();// non-moving point
                        vec2i v2 = getScreenCoordinates(curve->getXY(point + p).x, curve->getXY(point + p).y, newPos.z).xy();//pivot point
                        vec2i v3 = v2 + v2 - lastScreenPos; // opposite control point
                        vec2i v4;
                        if (smoothed) {
                            if (id.id != NULL_ID) { // if the next point is a Node ->get the following one.
                                CurvePtr c = editor->selectedGraph->getCurve(id);
                                NodePtr n = p == -1 ? curve->getStart() : curve->getEnd();
                                if (index == 1) {
                                    if (n == c->getStart()) { // extra checks for loops...
                                        index++;
                                    } else {
                                        index--;
                                    }
                                } else {
                                    index--;
                                }
                                v4 = getScreenCoordinates(c->getXY(index).x, c->getXY(index).y, newPos.z).xy();
                            } else { // just get the opposite point
                                v4 = getScreenCoordinates(curve->getXY(point + 3 * p).x, curve->getXY(point + 3 * p).y, newPos.z).xy();//last point
                            }
                        }
                        editor->displayedPoints.push_back(v1);
                        editor->displayedPoints.push_back(lastScreenPos);
                        editor->displayedPoints.push_back(lastScreenPos);
                        editor->displayedPoints.push_back(v2);
                        if (smoothed) {
                            editor->displayedPoints.push_back(v2);
                            editor->displayedPoints.push_back(v3);
                            editor->displayedPoints.push_back(v3);
                            editor->displayedPoints.push_back(v4);
                        }
                    } else { // point
                        //vec2f v1 = curve->getXY(curve->getIsControl(point - 1) ? point - 2 : point - 1);
                        //vec2f v2 = curve->getXY(curve->getIsControl(point + 1) ? point + 2 : point + 1);
                        vec2d a, b;
                        bool isSmooth = curve->getIsSmooth(point, a, b);
                        vec2d v1 = curve->getXY(isSmooth ? point - 2 : point - 1);
                        vec2d v2 = curve->getXY(isSmooth ? point + 2 : point + 1);
                        editor->displayedPoints.push_back(getScreenCoordinates(v1.x, v1.y, newPos.z).xy());
                        if (isSmooth) {
                            vec3i a = getScreenCoordinates(curve->getXY(point - 1).x, curve->getXY(point - 1).y, newPos.z);
                            vec3i b = getScreenCoordinates(curve->getXY(point + 1).x, curve->getXY(point + 1).y, newPos.z);
                            vec3i c = getScreenCoordinates(curve->getXY(point).x, curve->getXY(point).y, newPos.z);
                            editor->displayedPoints.push_back(lastScreenPos + (a - c).xy());
                            editor->displayedPoints.push_back(lastScreenPos + (a - c).xy());
                            editor->displayedPoints.push_back(lastScreenPos + (b - c).xy());
                            editor->displayedPoints.push_back(lastScreenPos + (b - c).xy());
                        } else {
                            editor->displayedPoints.push_back(lastScreenPos);
                            editor->displayedPoints.push_back(lastScreenPos);
                        }
                        editor->displayedPoints.push_back(getScreenCoordinates(v2.x, v2.y, newPos.z).xy());
                    }
                } else { // node
                    NodePtr n = point == 0 ? curve->getStart() : curve->getEnd();
                    CurveId id;
                    int index;
                    vec2d q;
                    for (int i = 0; i < n->getCurveCount(); i++) {
                        CurvePtr c = n->getCurve(i);
                        if (c->getStart() == n) {
                            if (Graph::hasOppositeControlPoint(c, 0, - 1, q, id, index)) {
                                index = 2;
                                editor->displayedPoints.push_back(getScreenCoordinates(c->getXY(index).x, c->getXY(index).y, newPos.z).xy()); // previous

                                index = 1;
                                vec3i a = getScreenCoordinates(c->getXY(index).x, c->getXY(index).y, newPos.z);
                                vec3i b = getScreenCoordinates(c->getXY(0).x, c->getXY(0).y, newPos.z);
                                editor->displayedPoints.push_back(lastScreenPos + (a - b).xy()); // to current
                                editor->displayedPoints.push_back(lastScreenPos + (a - b).xy()); // current
                            } else {
                                index = 1;
                                editor->displayedPoints.push_back(getScreenCoordinates(c->getXY(index).x, c->getXY(index).y, newPos.z).xy()); // current
                            }
                            editor->displayedPoints.push_back(lastScreenPos); // to node
                        }
                        if (c->getEnd() == n) {
                            if (Graph::hasOppositeControlPoint(c, c->getSize() - 1, 1, q, id, index)) {
                                index = c->getSize() - 3;
                                editor->displayedPoints.push_back(getScreenCoordinates(c->getXY(index).x, c->getXY(index).y, newPos.z).xy()); // previous

                                index = c->getSize() - 2;
                                vec3i a = getScreenCoordinates(c->getXY(index).x, c->getXY(index).y, newPos.z);
                                vec3i b = getScreenCoordinates(c->getXY(c->getSize() - 1).x, c->getXY(c->getSize() - 1).y, newPos.z);
                                editor->displayedPoints.push_back(lastScreenPos + (a - b).xy()); // to current
                                editor->displayedPoints.push_back(lastScreenPos + (a - b).xy()); // current
                            } else {
                                index = c->getSize() - 2;
                                editor->displayedPoints.push_back(getScreenCoordinates(c->getXY(index).x, c->getXY(index).y, newPos.z).xy()); // current
                            }
                            editor->displayedPoints.push_back(lastScreenPos); // to node
                        }
                    }
                }
                return true;
            }
            return false;
        }
        case CREATING_SMOOTH_CURVE: {
            if ((vec2i(x, y) - lastScreenPos).length() > 5) {
                editor->displayedPoints.push_back(vec2i(x, y));
                lastScreenPos = vec2i(x, y);
            }
            return true;
        }
        case SMOOTHING_POINT: {
            editor->displayedPoints.clear();
            newPos = getWorldCoordinates(x, y);
            int point = editor->selectedPoint;
            CurvePtr curve = editor->selectedCurve;

            vec2i v1 = vec2i(x, y);
            vec2i v2 = lastScreenPos + lastScreenPos - vec2i(x, y);
            editor->displayedPoints.push_back(v1);
            editor->displayedPoints.push_back(lastScreenPos);
            editor->displayedPoints.push_back(lastScreenPos);
            editor->displayedPoints.push_back(v2);

            vec2i p1 = getScreenCoordinates(curve->getXY(point - 1).x, curve->getXY(point - 1).y, newPos.z).xy();
            vec2i p2 = getScreenCoordinates(curve->getXY(point + 1).x, curve->getXY(point + 1).y, newPos.z).xy();
            if ((v1 - p1).length() < (v2 - p1).length()) {
                editor->displayedPoints.push_back(v1);
                editor->displayedPoints.push_back(p1);
                editor->displayedPoints.push_back(v2);
                editor->displayedPoints.push_back(p2);
            } else {
                editor->displayedPoints.push_back(v1);
                editor->displayedPoints.push_back(p2);
                editor->displayedPoints.push_back(v2);
                editor->displayedPoints.push_back(p1);
            }

            return true;
        }
        default: {
            break;
        }
    }

    return false;
}

bool EditGraphOrthoLayer::EditGraphHandler::keyTyped(unsigned char c, modifier m, int x, int y)
{
    if (c == 'e') {
        editor->setEditedGraph((editor->getEditedGraph() + 2) % (editor->getGraphCount() + 1) - 1);
        update();
        return true;
    } else if (c == 's' && m == ALT) {
        if (editor->editedGraph != -1) {
            printf("Saving Graph to TMPGRAPH.graph file\n");
            editor->editGraph->getRoot()->save("TMPGRAPH.graph", true, true, true);
        }
        return true;
    } else if (c == 's') {
        editor->transformVertex();
        update();
        return true;
    } else if (c == 'z') {
        return undo();
    } else if (c == 'j') {
        vec3d v = getWorldCoordinates(x, y);
        if (Logger::INFO_LOGGER != NULL) {
            ostringstream oss;
            oss << x << ":" << y << " -> " << v.x << ":" << v.y << ":" << v.z << endl;
            Logger::INFO_LOGGER->log("INFO", oss.str());
        }
        return true;
    }
    return false;
}

bool EditGraphOrthoLayer::EditGraphHandler::specialKey(key k, modifier m, int x, int y)
{
    switch (k) {
    case KEY_HOME:
        if (editor->transformVertex()) {
            update();
        }
        return true;
    case KEY_END:
        if (editor->remove()) {
            update();
        }
        return true;
    case KEY_INSERT: {
        vec3d v = getWorldCoordinates(x, y);
        if (editor->add((float) v.x, (float)v.y, editor->tolerance)) {
            update();
        }
        return true;
    }
    default:
        return false;
    }
}

vec3i EditGraphOrthoLayer::EditGraphHandler::getScreenCoordinates(double x, double y, double z)
{
    ptr<FrameBuffer> fb = SceneManager::getCurrentFrameBuffer();
    vec4<GLint> vp = fb->getViewport();
    float width = (float) vp.z;
    float height = (float) vp.w;

    vec3d v = terrainNode->deform->localToDeformed(vec3d(x, y, z));
    vec4d p = terrain->getLocalToScreen() * vec4d(v, 1.0f);
    vec3d v2 = p.xyz() / p.w;



    vec3i i = vec3f((v2.x + 1.0f) * width / 2.0f, height - (v2.y + 1.0f) * height / 2.0f, (v2.z + 1.0f) / 2.0f ).cast<int>();//(p.xyz() / p.w).cast<int>();
    return i;
}

vec3d EditGraphOrthoLayer::EditGraphHandler::getWorldCoordinates(int x, int y)
{
    float winx, winy, winz;
    ptr<FrameBuffer> fb = SceneManager::getCurrentFrameBuffer();
    vec4<GLint> vp = fb->getViewport();
    float width = (float) vp.z;
    float height = (float) vp.w;
    fb->readPixels(x, vp.w - y, 1, 1, DEPTH_COMPONENT, FLOAT, Buffer::Parameters(), CPUBuffer(&winz));

    winx = (x * 2.0f) / width - 1.0f;
    winy = 1.0f - (y * 2.0f) / height;
    winz = 2.0f * winz - 1.0f;
    mat4d screenToLocal = terrain->getLocalToScreen().inverse();//terrain->getLocalToWorld()->get(0).inverse() * terrain->getOwner()->getWorldToScreen().inverse() ;//terrain->getLocalToScreen().inverse();
    vec4d p = screenToLocal * vec4d(winx, winy, winz, 1);


    box3d b = terrain->getLocalBounds();

    double px = p.x / p.w, py = p.y /p.w, pz = p.z / p.w;
    vec3d v = vec3d(px, py, pz);

    if (b.xmin > px || b.xmax < px || b.ymin > py || b.ymax < py || b.zmin > pz || b.zmax < pz) {
        px = INF;
        py = INF;
        pz = INF;
        return vec3d(px, py, pz);
    } else {
        vec3d v2 = terrainNode->deform->deformedToLocal(v);
        return v2;
    }
}

void EditGraphOrthoLayer::EditGraphHandler::update()
{
    if (edited == false) {
        prevPos.z = 0.f;
    }
    editor->invalidateTiles();
    if (editor->editedGraph != -1) {
        editor->HANDLER->selectedCurveData.editor = editor;
        editor->HANDLER->selectedCurveData.mousePosition = newPos;
        editor->HANDLER->selectedCurveData.c = editor->selectedCurve.get();
        editor->HANDLER->selectedCurveData.selectedSegment = editor->selectedSegment;
        editor->HANDLER->selectedCurveData.selectedPoint = editor->selectedPoint;
        editor->editGraph->getRoot()->notifyListeners();
    }
}

void EditGraphOrthoLayer::getTypeNames(vector<string> &typeNames)
{
    for (int i = 0; i < 5; i++) {
        ostringstream s;
        s << "Type " << i;
        typeNames.push_back(s.str());
    }
}

static_ptr<EditGraphOrthoLayer::EditGraphHandlerList> EditGraphOrthoLayer::HANDLER(NULL);

EditGraphOrthoLayer::EditGraphOrthoLayer() :
    TileLayer("EditGraphOrthoLayer")
{
}

EditGraphOrthoLayer::EditGraphOrthoLayer(const vector< ptr<GraphProducer> > &graphs, ptr<Program> layerProg, int displayLevel, float tolerance, bool softEdition, double softEditionDelay, bool deform, string terrain, ptr<ResourceManager> manager) :
    TileLayer("EditGraphOrthoLayer")
{
    init(graphs, layerProg, displayLevel, tolerance, softEdition, softEditionDelay, deform, terrain, manager);
}

ptr<EventHandler> EditGraphOrthoLayer::getEventHandler()
{
    return HANDLER;
}

void EditGraphOrthoLayer::init(const vector< ptr<GraphProducer> > &graphs, ptr<Program> layerProg, int displayLevel, float tolerance, bool softEdition, double softEditionDelay, bool deform, string terrain, ptr<ResourceManager> manager)
{
    TileLayer::init(deform);
    defaultCurveType = 0;
    defaultCurveWidth = 7.0f;
    selectedCurve = NULL;
    selectedGraph = NULL;
    selectedPoint = -1;
    selectedSegment = -1;
    curveStart = vec3f(0.0f, 0.0f, 0.0f);
    this->tolerance = tolerance;

    this->graphs = graphs;
    this->layerProgram = layerProg;
    this->tileOffsetU = layerProgram->getUniform3f("tileOffset");
    this->editedGraph = -1;
    this->displayLevel = displayLevel;
    if (mesh == NULL) {
        this->mesh = new Mesh<vec3f, unsigned int>(LINE_STRIP, GPU_STREAM, 4, 4);
        this->mesh->addAttributeType(0, 3, A32F, false);
    }

    if (HANDLER == NULL) {
        HANDLER = new EditGraphHandlerList();
    }
    HANDLER->addHandler(this, new EditGraphHandler(this, manager, terrain));
    this->softEdition = softEdition;
    this->softEditionDelay = softEditionDelay;
}

EditGraphOrthoLayer::~EditGraphOrthoLayer()
{
    HANDLER->removeHandler(this);
    if (HANDLER->handlers.size() == 0) {
        HANDLER = NULL;
    }
    selectedCurve = NULL;
}

void EditGraphOrthoLayer::setTileSize(int tileSize, int tileBorder, float rootQuadSize)
{
    TileLayer::setTileSize(tileSize, tileBorder, rootQuadSize);
    for (unsigned int i = 0; i < graphs.size(); ++i) {
        graphs[i]->setTileSize(tileSize);
        graphs[i]->setRootQuadSize(rootQuadSize);
    }
}

void EditGraphOrthoLayer::useTile(int level, int tx, int ty, unsigned int deadline)
{
    usedTiles.insert(make_pair(level, make_pair(tx, ty)));
    if (editedGraph != -1 && level >= displayLevel) {
        if (editGraph->hasTile(level, tx, ty)) {
            TileCache::Tile *t = editGraph->getTile(level, tx, ty, deadline);
            assert(t != NULL);
        }
    }
}

void EditGraphOrthoLayer::unuseTile(int level, int tx, int ty)
{
    usedTiles.erase(make_pair(level, make_pair(tx, ty)));
    if (editedGraph != -1 && level>= displayLevel) {
        if (editGraph->hasTile(level, tx, ty)) {
            TileCache::Tile *t = editGraph->findTile(level, tx, ty);
            assert(t != NULL);
            editGraph->putTile(t);
        }
    }
}

void EditGraphOrthoLayer::startCreateTile(int level, int tx, int ty, unsigned int deadline, ptr<Task> task, ptr<TaskGraph> result)
{
    if (editedGraph != -1 && level >= displayLevel) {
        if (editGraph->hasTile(level, tx, ty)) {
            TileCache::Tile *t = editGraph->getTile(level, tx, ty, deadline);
            assert(t != NULL);
            if (result != NULL) {
                result->addTask(t->task);
                result->addDependency(task, t->task);
            }
        }
    }
}

void EditGraphOrthoLayer::beginCreateTile()
{
}

void EditGraphOrthoLayer::drawGraph(const vec3d &tileCoords, GraphPtr g, float pointSize, ptr<Mesh<vec3f, unsigned int> > mesh, vec2d &nx, vec2d &ny, vec2d &lx, vec2d &ly)
{
     bool checkGraph = false;
     if (selectedGraph != NULL) {
         checkGraph = (g->getAncestor() == selectedGraph.get());
     }
     ptr<FrameBuffer> fb = SceneManager::getCurrentFrameBuffer();

     AreaPtr a;
     ptr<Graph::AreaIterator> ai = g->getAreas();
     while (ai->hasNext()) {
         a = ai->next();
         ptr<Graph> sg = a->getSubgraph();
         if (sg != NULL) {
             drawGraph(tileCoords, sg, pointSize, mesh, nx, ny, lx, ly);
         }

         a = a->getAncestor();
         a->check();
         mesh->setMode(LINE_STRIP);
         mesh->clear();
         int n = a->getCurveCount();
         for (int i = 0; i < n; ++i) {
             int o;
             CurvePtr c = a->getCurve(i, o);
             int size = c->getSize();
             for (int j = 0; j < size; ++j) {

                 vec2d p, q, r, co;

                 q = c->getXY(o == 0 ? j : size - 1 - j);

                 if (j > 0) {
                     p = c->getXY(o == 0 ? j - 1 : size - 1 - (j - 1));
                 } else {
                     int oo;
                     CurvePtr d = a->getCurve(i > 0 ? i - 1 : n - 1, oo);
                     p = d->getXY(oo == 0 ? d->getSize() - 2 : 1);
                 }

                 if (j < size - 1) {
                     r = c->getXY(o == 0 ? j + 1 : size - 1 - (j + 1));
                 } else {
                     int oo;
                     CurvePtr d = a->getCurve(i < n - 1 ? i + 1 : 0, oo);
                     r = d->getXY(oo == 0 ? 1 : d->getSize() - 2);
                 }

                 float w = cross(r - q, p - q) > 0.0f ? -3.0f * pointSize : 3.0f * pointSize;

                 vec2d pq = q - p;
                 vec2d qr = r - q;
                 float l = pq.length();
                 if (l != 0.0f) {
                    pq = pq / l;
                 }
                 l = qr.length();
                 if (l != 0.0f) {
                     qr = qr / l;
                 }
                 co = corner(q, p, r, w / (lx * pq.y - ly * pq.x).length(), w / (lx * qr.y - ly * qr.x).length());

                 vec2d v = (vec2d(co.x, co.y) - tileCoords.xy()) * tileCoords.z;
                 mesh->addVertex(vec3f(v.x, v.y, 3.0f));
             }
         }
         fb->draw(layerProgram, *mesh);
     }


    ptr<Graph::CurveIterator> ci = g->getCurves();
    CurvePtr p;
    int size;
    mesh->setMode(LINE_STRIP);
    while (ci->hasNext()) {
        p = ci->next();
        if (p->getAncestor() == selectedCurve && selectedSegment != -1) {
            continue;
        }
        size = p->getSize();
        mesh->clear();
        int c = p->getAncestor() == selectedCurve ? 1 : 0;
        for (int i = 0; i < size; ++i) {
            vec2d xy = (p->getXY(i) - tileCoords.xy()) * tileCoords.z;
            mesh->addVertex(vec3f(xy.x, xy.y, (float) c));
        }
        fb->draw(layerProgram, *mesh);
    }
    if (checkGraph && selectedCurve != NULL && selectedSegment != -1) {
        mesh->clear();
        for (int i = 0; i < selectedCurve->getSize(); ++i) {
            //prev = cur;
            vec2d cur = (selectedCurve->getXY(i) - tileCoords.xy()) * tileCoords.z;
            if (selectedSegment == i) {
                mesh->addVertex(vec3f(cur.x, cur.y, 0.0f));
            }
            if ((selectedSegment + 1) == i) {
                mesh->addVertex(vec3f(cur.x, cur.y, 1.0f));
            }
            int c = selectedSegment == i ? 1 : 0;
            mesh->addVertex(vec3f(cur.x, cur.y, (float) c));
        }
        fb->draw(layerProgram, *mesh);
    }

    ci = g->getCurves();
    mesh->clear();
    mesh->setMode(TRIANGLES);

    vec2d bx = vec2d(pointSize, 0.0f);
    vec2d by = nx * pointSize;
    vec2d s = (by - bx) * tileCoords.z;
    vec2d t = (bx + by) * tileCoords.z;

    while (ci->hasNext()) {
        p = ci->next();
        size = p->getSize();
        for (int i = 0; i < size; ++i) {
            vec2d v = (p->getXY(i) - tileCoords.xy()) * tileCoords.z;
            int c;
            if (p->getAncestor() == selectedCurve && selectedPoint != -1 && p->getAncestor()->getVertex(v) == selectedPoint) {
                continue;
            } else {
                if (i == 0 || i == size - 1) {
                    c = 0;
                } else if (p->getIsControl(i)) {
                    c = 2;
                } else {
                    c = 3;
                }
            }
            mesh->addVertex(vec3f(v.x - t.x, v.y - t.y, (float) c));
            mesh->addVertex(vec3f(v.x - s.x, v.y - s.y, (float) c));
            mesh->addVertex(vec3f(v.x + s.x, v.y + s.y, (float) c));
            mesh->addVertex(vec3f(v.x + t.x, v.y + t.y, (float) c));
            mesh->addVertex(vec3f(v.x + s.x, v.y + s.y, (float) c));
            mesh->addVertex(vec3f(v.x - s.x, v.y - s.y, (float) c));
        }
    }

    if (checkGraph && selectedCurve != NULL && selectedPoint != -1) {
        vec2d v = (selectedCurve->getXY(selectedPoint) - tileCoords.xy()) * tileCoords.z;
        mesh->addVertex(vec3f(v.x - t.x, v.y - t.y, 1.0f));
        mesh->addVertex(vec3f(v.x - s.x, v.y - s.y, 1.0f));
        mesh->addVertex(vec3f(v.x + s.x, v.y + s.y, 1.0f));
        mesh->addVertex(vec3f(v.x + t.x, v.y + t.y, 1.0f));
        mesh->addVertex(vec3f(v.x + s.x, v.y + s.y, 1.0f));
        mesh->addVertex(vec3f(v.x - s.x, v.y - s.y, 1.0f));
    }

    if (checkGraph && curveStart.z == 1.0f) {
        vec2d v = (vec2d(curveStart.x, curveStart.y) - tileCoords.xy()) * tileCoords.z;
        mesh->addVertex(vec3f(v.x - t.x, v.y - t.y, 1.0f));
        mesh->addVertex(vec3f(v.x - s.x, v.y - s.y, 1.0f));
        mesh->addVertex(vec3f(v.x + s.x, v.y + s.y, 1.0f));
        mesh->addVertex(vec3f(v.x + t.x, v.y + t.y, 1.0f));
        mesh->addVertex(vec3f(v.x + s.x, v.y + s.y, 1.0f));
        mesh->addVertex(vec3f(v.x - s.x, v.y - s.y, 1.0f));
    }
    if (mesh->getVertexCount() != 0) {
        fb->draw(layerProgram, *mesh);
    }
}

bool EditGraphOrthoLayer::doCreateTile(int level, int tx, int ty, TileStorage::Slot *data)
{
    if (Logger::DEBUG_LOGGER != NULL) {
        ostringstream oss;
        oss << "EditGraph tile " << getProducerId() << " " << level << " " << tx << " " << ty;
        Logger::DEBUG_LOGGER->log("GRAPH", oss.str());
    }
    if (editedGraph != -1 && level >= displayLevel) {
        if (editGraph->hasTile(level, tx, ty)) {


            TileCache::Tile * t = editGraph->findTile(level, tx, ty);
            assert(t != NULL);
            ObjectTileStorage::ObjectSlot *graphData = dynamic_cast<ObjectTileStorage::ObjectSlot*>(t->getData());
            GraphPtr g = graphData->data.cast<Graph>();
            assert(g != NULL);

            vec3d q = getTileCoords(level, tx, ty);
            float scale = 2.0f * (1.0f - getTileBorder() * 2.0f / getTileSize()) / q.z;
            vec3d tileOffset = vec3d(q.x + q.z / 2.0f, q.y + q.z / 2.0f, scale);
            //tileOffsetU->set(vec3f(q.x + q.z / 2.0f, q.y + q.z / 2.0f, scale));
            tileOffsetU->set(vec3f(0.0, 0.0, 1.0));

            vec2d nx = vec2d(0.0f, 1.0f);
            vec2d ny = vec2d(-1.0f, 0.0f);
            vec2d lx = vec2d(1.0f, 0.0f);
            vec2d ly = vec2d(0.0f, 1.0f);
            getDeformParameters(q, nx, ny, lx, ly);

            float screenPointSize = 6.0f; // size in pixels on screen
            float pointSize = screenPointSize / (getTileSize() - 2.0f * getTileBorder()) / scale;
            drawGraph(tileOffset, g, pointSize, mesh, nx, ny, lx, ly);
        }
    }
    return true;
}

void EditGraphOrthoLayer::endCreateTile()
{
}

void EditGraphOrthoLayer::stopCreateTile(int level, int tx, int ty)
{
    if (editedGraph != -1 && level >= displayLevel) {
        if (editGraph->hasTile(level, tx, ty)) {
            TileCache::Tile *t = editGraph->findTile(level, tx, ty);
            assert(t != NULL);
            editGraph->putTile(t);
        }
    }
}

int EditGraphOrthoLayer::getGraphCount()
{
    return int(graphs.size());
}

int EditGraphOrthoLayer::getEditedGraph()
{
    return editedGraph;
}

vector<ptr<GraphProducer> > EditGraphOrthoLayer::getGraphs()
{
    return graphs;
}

void EditGraphOrthoLayer::setEditedGraph(int index)
{
    if (index != editedGraph) {
        editedGraph = index;
        if (editGraph != NULL) {
            set<TileCache::Tile::Id>::iterator i = usedTiles.begin();
            while (i != usedTiles.end()) {
                TileCache::Tile *t = editGraph->findTile(i->first, i->second.first, i->second.second);
                assert(t != NULL);
                editGraph->putTile(t);
                i++;
            }
        }
        if (index == -1) {
            editGraph = NULL;
        } else {
            editGraph = new GraphProducer(graphs[index]->getName(), graphs[index]->getCache(), graphs[index]->getPrecomputedGraphs(), false, 0, true, 20);
            editGraph->setTileSize(getTileSize());
            editGraph->setRootQuadSize(getRootQuadSize());
            set<TileCache::Tile::Id>::iterator i = usedTiles.begin();
            while (i != usedTiles.end()) {
                TileCache::Tile *t = editGraph->getTile(i->first, i->second.first, i->second.second, 0);
                assert(t != NULL);
                i++;
            }
            selectedGraph = NULL;
            selectedCurve = NULL;
            selectedSegment = -1;
            selectedPoint = -1;
            curveStart.z = 0.0f;
        }
        if (Logger::INFO_LOGGER != NULL) {
            ostringstream msg;
            msg << "Changing edited graph " << index;
            Logger::INFO_LOGGER->log("GRAPHEDITOR", msg.str());
        }
    }
}

bool EditGraphOrthoLayer::select(double x, double y, float tolerance)
{
    if (x == INF) {
        return false;
    }
    return select(x, y, tolerance, selectedGraph, selectedArea, selectedCurve, selectedSegment, selectedPoint);
}

bool EditGraphOrthoLayer::selection()
{
    return editedGraph != -1 && selectedCurve != NULL && (selectedPoint != -1 || selectedSegment != -1);
}

void EditGraphOrthoLayer::getSelection(CurvePtr &curve, int &point, int &segment)
{
    curve = selectedCurve;
    point = selectedPoint;
    segment = selectedSegment;
}

void EditGraphOrthoLayer::setSelection(CurvePtr curve, int point, int segment)
{
    selectedCurve = curve;
    selectedPoint = point;
    selectedSegment = segment;
}

bool EditGraphOrthoLayer::select(double x, double y, float tolerance, GraphPtr &graph, list<AreaId> &areas, CurvePtr &curve, int &segment, int &point)
{
    bool res = false;
    if (editedGraph != -1) {
        TileCache::Tile *t = editGraph->findTile(0, 0, 0);
        if (t != NULL && t->task->isDone()) {
            res = select(t, x, y, tolerance, graph, areas, curve == NULL ? selectedCurve : curve, segment, point);
        }
    }
    //printf("Selected : %d %d %d\n", selectedCurve == NULL ? -1 : selectedCurve->getId().id, selectedSegment, selectedPoint);
    return res;
}

bool EditGraphOrthoLayer::findCurve(GraphPtr p, double x, double y, float tolerance, GraphPtr &graph, list<AreaId> &areas, CurvePtr &curve, int &segment, int &point)
{
    ptr<Graph::CurveIterator> ci = p->getCurves();
    while (ci->hasNext()) {
        CurvePtr cp = ci->next();
        for (int i = 0; i < cp->getSize(); i++) {
            vec2d pt = cp->getXY(i);
            if (abs(x - pt.x) < tolerance && abs(y - pt.y) < tolerance) {
                graph = p->getAncestor();
                curve = cp->getAncestor();
                point = curve->getVertex(pt);
                if (point == -1) {
                    return false;
                }
                assert(point != -1);

                //if (Logger::INFO_LOGGER != NULL) {
                //    ostringstream msg;
                //    msg << "Selected curve : " << selectedCurve->getId().id << " - Node " << selectedPoint;
                //    Logger::INFO_LOGGER->log("GRAPHEDITOR", msg.str());
                //}
                //printf("Selected Curve : %d - Node %d\n", selectedCurve->getId().id, selectedPoint);
                return true;
            }
        }
    }
    ci = p->getCurves();
    while (ci->hasNext()) {
        CurvePtr cp = ci->next();
        vec2d cur;
        vec2d next = cp->getXY(0);
        for (int i = 0; i < cp->getSize(); i++) {
            cur = next;
            next = cp->getXY(i + 1);
            seg2d ab = seg2d(cur, next);
            if (ab.segmentDistSq(vec2d(x, y)) < tolerance*tolerance) {
                graph = p->getAncestor();
                curve = cp->getAncestor();
                segment = curve->getVertex(cp->getXY(i));
                int p1 = curve->getVertex(cur);
                int p2 = curve->getVertex(next);
                segment = min(p1, p2);
                if (curve->getStart() == curve->getEnd()) {
                    if (min(p1, p2) == 0 && max(p1, p2) != 1) {
                        segment = max(p1, p2);
                    }
                }
                if (segment == -1) {
                    return false;
                }
                assert(segment != -1);

                //if (Logger::INFO_LOGGER != NULL) {
                //    ostringstream msg;
                //    msg << "Selected curve : " << selectedCurve->getId().id << " - Segment " << selectedSegment;
                //    Logger::INFO_LOGGER->log("GRAPHEDITOR", msg.str());
                //}
                //printf("Selected Curve : %d - Segment %d\n", selectedCurve->getId().id, selectedSegment);
                return true;
            }
        }
    }
    ptr<Graph::AreaIterator> ca = p->getAreas();
    while (ca->hasNext()) {
        AreaPtr a = ca->next();
        GraphPtr g = a->getSubgraph();
        if (g != NULL) {
            if (findCurve(g, x, y, tolerance, graph, areas, curve, segment, point)) {
                areas.push_front(a->getAncestor()->getId());
                return true;
            }
        }
    }

    //if (Logger::INFO_LOGGER != NULL) {
    //    Logger::INFO_LOGGER->log("GRAPHEDITOR", "No node selected !");
    //}
    return false;
}

bool EditGraphOrthoLayer::select(TileCache::Tile *t, double x, double y, float tolerance, GraphPtr &graph, list<AreaId> &areas, CurvePtr &curve, int &segment, int &point)
{
    vec3d q = getTileCoords(t->level, t->tx, t->ty);

    TileCache::Tile *child = NULL;
    int level = t->level + 1;

    int width = 1 << t->level;
    float tileWidth = q.z / (1 << (level - t->level));
    int tx = (int) ((x + q.z * width / 2) / tileWidth);
    int ty = (int) ((y + q.z * width / 2) / tileWidth);

    child = editGraph->findTile(level, tx, ty);

    if (child != NULL && child->task->isDone()) {
        return select(child, x, y, tolerance, graph, areas, curve, segment, point);
    }

    float d = tolerance * q.z;

    GraphPtr p = dynamic_cast<ObjectTileStorage::ObjectSlot *>(t->getData())->data.cast<Graph>();

    areas.clear();
    graph = NULL;
    curve = NULL;
    point = -1;
    segment = -1;
    curveStart.z = 0.0f;

    return findCurve(p, x, y, d, graph, areas, curve, segment, point);
}

bool EditGraphOrthoLayer::updateSelectedCurve()
{
    bool res = false;
    if (editedGraph != -1) {
        if (selectedCurve != NULL) {
            GraphPtr g = editGraph->getRoot();
            g->changes.clear();
            g->changes.changedArea = selectedArea;
            g->changes.addedCurves.insert(selectedCurve->getId());
            g->changes.removedCurves.insert(selectedCurve->getId());
            selectedGraph->getAreasFromCurves(g->changes.addedCurves, g->changes.addedAreas);
            selectedGraph->getAreasFromCurves(g->changes.removedCurves, g->changes.removedAreas);

            res = true;
            update();
        }
    }
    return res;
}

bool EditGraphOrthoLayer::movePoint(double x, double y, int i)
{
    bool res = false;
    if (editedGraph != -1) {

        if (selectedCurve != NULL && (selectedPoint != -1 || i != -1)) {
            GraphPtr g = editGraph->getRoot();//dynamic_cast<ObjectTileStorage::ObjectSlot *>(editGraph->findTile(0, 0, 0)->getData())->data.cast<Graph>();
            g->changes.clear();
            set<CurveId> curves;
            selectedGraph->movePoint(selectedCurve, i == -1 ? selectedPoint : i, vec2d(x, y), curves);
            g->changes.changedArea = selectedArea;
            selectedGraph->getAreasFromCurves(curves, g->changes.removedAreas);
            selectedGraph->getAreasFromCurves(curves, g->changes.addedAreas);
            g->changes.addedCurves.insert(curves.begin(), curves.end());
            g->changes.removedCurves.insert(curves.begin(), curves.end());

            res = true;
        } else if (curveStart.z != 0.0f) {
            curveStart = vec3f(x, y, 1.0f);
        }
    }
    return res;

}

bool EditGraphOrthoLayer::add(double x, double y, float tolerance)
{
    bool res = false;

    if (editedGraph != -1) {
        ostringstream msg;
        GraphPtr root = editGraph->getRoot();
        root->changes.clear();
        root->changes.changedArea = selectedArea;
        if (selectedGraph == NULL) {
            selectedGraph = editGraph->getRoot();
        }

        if (selectedCurve == NULL) { // Case : 2 new Nodes -> Create a new Curve
            if (curveStart.z == 0.0f) {
                curveStart = vec3f(x, y, 1.0f);
                res = true;
                if (Logger::INFO_LOGGER != NULL) {
                    msg << "Adding a new Node.";
                }
            } else {
                res = addCurve(curveStart.x, curveStart.y, x, y, tolerance, selectedGraph, selectedCurve, selectedSegment, selectedPoint, root->changes);
                curveStart.z = 0.0f;
                if (Logger::INFO_LOGGER != NULL) {
                    msg << "Adding a new curve.";
                }
            }
        } else if (selectedSegment != -1 && selectedPoint == -1) { // Case -> Create a new Vertex on a given Curve
            res = addVertex(x, y, selectedGraph, selectedCurve, selectedSegment, selectedPoint, root->changes);
            if (Logger::INFO_LOGGER != NULL) {
                msg << "Adding a new Control Point.";
            }
        } else if (selectedPoint != -1 && selectedSegment == -1) { // Case -> Find a node @x:y, create the node if necessary, and create a curve
            res = addCurve(x, y, tolerance, selectedGraph, selectedCurve, selectedPoint, root->changes);
            if (Logger::INFO_LOGGER != NULL) {
                msg << "Adding a new curve.";
            }
        }
        if (Logger::INFO_LOGGER != NULL && msg.str().length() != 0) {
            Logger::INFO_LOGGER->log("GRAPHEDITOR", msg.str());
        }
    }
    return res;
}

bool EditGraphOrthoLayer::change()
{
    bool res = false;
    if (editedGraph != -1) {
        ostringstream msg;
        GraphPtr root = editGraph->getRoot();
        root->changes.clear();
        root->changes.changedArea = selectedArea;
        if (selectedCurve != NULL && selectedPoint != -1) {
            if (selectedPoint == 0 || selectedPoint == selectedCurve->getSize() - 1) {
                NodePtr n = selectedPoint == 0 ? selectedCurve->getStart() : selectedCurve->getEnd();
                if ( n->getCurveCount() != 2) {
                    res = false;
                } else {
                    res = removeNode(selectedGraph, selectedCurve, selectedSegment, selectedPoint, root->changes);
                    if (Logger::INFO_LOGGER != NULL) {
                        msg << "Transforming Node to Control Point.";
                    }
                }
            } else {
                res = addNode(selectedGraph, selectedCurve, selectedPoint, root->changes);
                if (Logger::INFO_LOGGER != NULL) {
                    msg << "Transforming Control Point to Node.";
                }
            }
        }
        if (Logger::INFO_LOGGER != NULL && msg.str().length() != 0) {
            Logger::INFO_LOGGER->log("GRAPHEDITOR", msg.str());
        }

    }
    return res;
}

bool EditGraphOrthoLayer::remove()
{
    bool res = false;
    if (editedGraph != -1) {
        ostringstream msg;
        GraphPtr root = editGraph->getRoot();
        root->changes.clear();
        root->changes.changedArea = selectedArea;
        if (selectedCurve != NULL) {
            if (selectedPoint == -1) { //if we have selected a segment -> delete the curve
                res = removeCurve(selectedGraph, selectedCurve, root->changes);
                selectedSegment = -1;
                if (Logger::INFO_LOGGER != NULL) {
                    msg << "Removing Curve.";
                }
            } else {
                if (selectedCurve->getSize() == 3) { // checking that the new curve won't be a clone of another one.
                    NodePtr start = selectedCurve->getStart();
                    CurvePtr c;
                    for (int i = 0; i < start->getCurveCount(); i++) { // for each linked curve
                        c = start->getCurve(i);
                        if (c->getSize() == 2 && c->getOpposite(start) == selectedCurve->getEnd()) { // if the curve is exactly the same, we delete it
                            res = removeCurve(selectedGraph, selectedCurve, root->changes);
                            selectedPoint = -1;
                            if (Logger::INFO_LOGGER != NULL) {
                                msg << "Removing Curve.";
                            }
                            break;
                        }
                    }
                }
                if (!res) { // if we didn't delete anything so far
                    bool removeCP = true;
                    if  (selectedPoint == 0 || selectedPoint == selectedCurve->getSize() - 1) { // if we are deleting a NODE
                        NodePtr n = selectedPoint == 0 ? selectedCurve->getStart() : selectedCurve->getEnd();
                        if (n->getCurveCount() != 2) {
                            removeCP = false;
                        }
                        res = removeNode(selectedGraph, selectedCurve, selectedSegment, selectedPoint, root->changes);
                        if (Logger::INFO_LOGGER != NULL) {
                            msg << "Removing Node.";
                        }
                    } else {
                        if (Logger::INFO_LOGGER != NULL) {
                            msg << "Removing Vertex.";
                        }
                    }
                    if (removeCP && selectedCurve != NULL) { // removing a vertex (can be an ex NODE)
                        selectedGraph->removeVertex(selectedCurve, selectedSegment, selectedPoint, root->changes);
                        res = true;
                    }
                }
            }
        }
        if (Logger::INFO_LOGGER != NULL) {
            Logger::INFO_LOGGER->log("GRAPHEDITOR", msg.str());
        }
    }
    return res;
}

bool EditGraphOrthoLayer::invert()
{
    bool res = false;
    if (editedGraph != -1) {
        ostringstream msg;
        GraphPtr root = editGraph->getRoot();
        root->changes.clear();
        root->changes.changedArea = selectedArea;
        if (selectedCurve != NULL) {
            res = true;
            selectedCurve->invert();
            if (selectedPoint != -1) {
                selectedPoint = selectedCurve->getSize() - selectedPoint - 1;
            } else {
                selectedSegment = selectedCurve->getSize() - selectedSegment - 2;
            }
            if (Logger::INFO_LOGGER != NULL) {
                msg << "Inverting Curve.";
            }
            root->changes.addedCurves.insert(selectedCurve->getId());
            root->changes.removedCurves.insert(selectedCurve->getId());
            if (selectedCurve->getArea1() != NULL) {
                root->changes.addedAreas.insert(selectedCurve->getArea1()->getId());
                root->changes.removedAreas.insert(selectedCurve->getArea1()->getId());
                if (selectedCurve->getArea2() != NULL) {
                    root->changes.addedAreas.insert(selectedCurve->getArea2()->getId());
                    root->changes.removedAreas.insert(selectedCurve->getArea2()->getId());
                }
            }
        }
        if (Logger::INFO_LOGGER != NULL) {
            Logger::INFO_LOGGER->log("GRAPHEDITOR", msg.str());
        }
    }
    return res;
}

bool EditGraphOrthoLayer::addVertex(double x, double y, GraphPtr g, CurvePtr &curve, int &segment, int &point, Graph::Changes &changes)
{
    bool res = false;
    if (g != NULL) {
        if (curve != NULL && segment != -1) {
            vec2d v(x, y);
            curve->addVertex(v, segment, false);
            curve->computeCurvilinearCoordinates();

            changes.addedCurves.insert(curve->getId());
            changes.removedCurves.insert(curve->getId());
            if (curve->getArea1() != NULL) {
                changes.addedAreas.insert(curve->getArea1()->getId());
                changes.removedAreas.insert(curve->getArea1()->getId());
                if (curve->getArea2() != NULL) {
                    changes.addedAreas.insert(curve->getArea2()->getId());
                    changes.removedAreas.insert(curve->getArea2()->getId());
                }
            }
            point = segment + 1;
            segment = -1;

            res = true;
        }
    }
    return res;
}

bool EditGraphOrthoLayer::transformVertex()
{
    if (editedGraph != -1) {
        GraphPtr root = editGraph->getRoot();
        root->changes.clear();
        root->changes.changedArea = selectedArea;
        return transformVertex(selectedGraph, selectedCurve, selectedPoint, root->changes);
    }
    return false;
}

bool EditGraphOrthoLayer::smoothNode(bool smooth)
{
    if (editedGraph != -1) {
        GraphPtr root = editGraph->getRoot();
        root->changes.clear();
        root->changes.changedArea = selectedArea;
        return smoothNode(selectedGraph, selectedCurve, selectedPoint, root->changes, smooth);
    }
    return false;
}

bool EditGraphOrthoLayer::smoothCurve(bool smooth)
{
    if (editedGraph != -1) {
        GraphPtr root = editGraph->getRoot();
        root->changes.clear();
        root->changes.changedArea = selectedArea;
        return smoothCurve(selectedGraph, selectedCurve, selectedPoint, selectedSegment, root->changes, smooth);
    }
    return false;
}

bool EditGraphOrthoLayer::smoothArea(bool smooth)
{
    if (editedGraph != -1) {
        GraphPtr root = editGraph->getRoot();
        root->changes.clear();
        root->changes.changedArea = selectedArea;
        return smoothArea(selectedGraph, selectedCurve, selectedPoint, selectedSegment, root->changes, smooth);

    }
    return false;
}

bool EditGraphOrthoLayer::smoothArea(GraphPtr g, CurvePtr &curve, int &point, int &segment, Graph::Changes &changes, bool smooth)
{
    bool res = false;
    if (g != NULL) {
        if (curve != NULL) {
            AreaPtr a = curve->getArea1();
            if (a != NULL) {
                int p, s;

                for (int i = 0; i < a->getCurveCount(); i++) {
                    CurvePtr c = a->getCurve(i);
                    if (c->getId() == curve->getId()) {
                        res |= smoothCurve(selectedGraph, c, point, segment, changes, smooth);
                    } else {
                        res |= smoothCurve(selectedGraph, c, p, s, changes, smooth);
                    }
                }
            }
        }
    }
    return res;
}

bool EditGraphOrthoLayer::smoothNode(GraphPtr g, CurvePtr &curve, int &point, Graph::Changes &changes, bool smooth)
{
    bool res = false;
    if (g != NULL && curve != NULL && (point == 0 || point == curve->getSize() - 1)) {
        NodePtr n = point == 0 ? curve->getStart() : curve->getEnd();
        vector<pair<CurvePtr, int> > curves;
        set<CurveId> found;
        set<CurveId> excluded;
        CurvePtr c = curve;
        vec2d q;
        vec2d previous;
        CurveId id;
        id.id = NULL_ID;
        int index;
        int orientation;
        int step;

        orientation = point == 0 ? 0 : 1;

        while (c != NULL) {
            bool isStart = n == c->getStart();
            bool isEnd = n == c->getEnd();
            bool inside = found.find(c->getId()) != found.end();
            if (Graph::hasOppositeControlPoint(c, orientation ? c->getSize() - 1 : 0, orientation ? 1 : - 1, q, id, index)) {
                excluded.insert(id);
                if (smooth) {
                    // nothing
                } else {
                    curves.push_back(make_pair(g->getCurve(id), index != 1));
                    curves.push_back(make_pair(c, orientation));
                }
            } else {
                if (smooth) {
                    curves.push_back(make_pair(c, orientation));
                } else {
                    // nothing
                }
            }

            if (inside || !isStart || !isEnd) {
                excluded.insert(c->getId());
            }
            found.insert(c->getId());
            previous = orientation ? c->getXY(c->getSize() - 2) : c->getXY(1);
            c = curve->getNext(n, excluded, false);

            if (c != NULL) {
                if (c->getEnd() == c->getStart()) {
                    float ai = angle(previous - n->getPos(), c->getXY(1) - n->getPos());
                    float aj = angle(previous - n->getPos(), c->getXY(c->getSize() - 2) - n->getPos());
                    orientation = aj < ai;
                } else {
                    orientation = n == c->getStart() ? 0 : 1;
                }
            }
        }

        if (smooth) {
            vec2d a, b;
            for (int i = 0; i < (int) curves.size(); i++) {
                c = curves[i].first;
                orientation = curves[i].second;
                index = orientation ? c->getSize() - 1 : 0;
                step = orientation ? -1 : 1;
                if (c->getIsControl(index + step)) {
                    if (c->getIsControl(index + step * 2)) {
                        //nothing
                    } else {
                        if (c->getIsSmooth(index + step * 2, a, b)) {
                            c->addVertex(n->getPos(), orientation ? c->getSize() - 2 : 0, true);
                            res = true;
                        }
                    }
                } else {
                    c->addVertex(n->getPos(), orientation ? c->getSize() - 2 : 0, true);
                    res = true;
                }
            }
            vec2d q = n->getPos();
            vec2d p, r;

            for (int i = 0; i < (int) curves.size() / 2; i++) {
                CurvePtr c0 = curves[i].first;
                CurvePtr c1 = curves[i + (int) curves.size() / 2].first;
                int orientation0 = curves[i].second;
                int orientation1 = curves[i + (int) curves.size() / 2].second;

                p = c0->getXY(orientation0 ? c0->getSize() - 3 : 2);
                r = c1->getXY(orientation1 ? c1->getSize() - 3 : 2);

                a = q - (r - p) * 0.10f;
                b = q + (r - p) * 0.10f;
                c0->setXY(orientation0 ? c0->getSize() - 2 : 1, a);
                c1->setXY(orientation1 ? c1->getSize() - 2 : 1, b);
                changes.addedCurves.insert(c0->getId());
                changes.removedCurves.insert(c0->getId());
                changes.addedCurves.insert(c1->getId());
                changes.removedCurves.insert(c1->getId());
            }
            if  ((int) curves.size() % 2 != 0) {
                c = curves[(int) curves.size() - 1].first;
                orientation = curves[(int) curves.size() - 1].second;
                c->setXY(orientation ? c->getSize() - 2 : 1, (n->getPos() + c->getXY(orientation ? c->getSize() - 3 : 2)) / 2.0f );
                changes.addedCurves.insert(c->getId());
                changes.removedCurves.insert(c->getId());
            }
        } else {
            for (int i = 0; i < (int) curves.size(); i++) {
                c = curves[i].first;
                orientation = curves[i].second;
                c->removeVertex(orientation ? c->getSize() - 2 : 1);
                changes.addedCurves.insert(c->getId());
                changes.removedCurves.insert(c->getId());
            }
        }
        for (int i = 0; i < (int) curves.size(); i++) {
            curves[i].first->computeCurvilinearCoordinates();
        }

        selectedGraph->getAreasFromCurves(g->changes.addedCurves, g->changes.addedAreas);
        selectedGraph->getAreasFromCurves(g->changes.removedCurves, g->changes.removedAreas);
        res = (int) g->changes.addedCurves.size() != 0;
        if (point != 0) {
            point = curve->getSize() - 1;
        }

    }
    return res;
}

bool EditGraphOrthoLayer::smoothCurve(GraphPtr g, CurvePtr &curve, int &point, int &segment, Graph::Changes &changes, bool smooth)
{
    bool res = false;
    if (g != NULL) {
        if (curve != NULL) {

            int i = 1;
            while (true) {
                if (i >= curve->getSize() - 1) {
                    break;
                }
                if (curve->getIsControl(i)) {
                    i++;
                    continue;
                }
                vec2d a, b;
                bool isSmooth = curve->getIsSmooth(i, a, b);
                if (smooth) {
                    if (!isSmooth) {
                        curve->addVertex(a, i - 1, true);
                        curve->addVertex(b, i + 1, true);
                        if (point != -1) {
                            if (point > i) {
                                point += 2;
                            } else if (point == i) {
                                point += 1;
                            }
                        } else {
                            if (segment >= i) {
                                segment += 2;
                            }
                        }
                        i += 2;
                        res = true;
                    }
                    i++;
                } else {
                    if (isSmooth) {
                        curve->removeVertex(i + 1);
                        curve->removeVertex(i - 1);

                        if (point != -1) {
                            if (point == i) {
                                point -= 1;
                            } else if (point > i) {
                                point -= 2;
                            }
                        } else {
                            if (segment > i) {
                                segment -= 2;
                            } else if (segment == i) {
                                segment -= 1;
                            }
                        }
                        res = true;
                    } else {
                        i++;
                    }
                }
            }

            if (res) {
                curve->computeCurvilinearCoordinates();
                changes.addedCurves.insert(curve->getId());
                changes.removedCurves.insert(curve->getId());
                if (curve->getArea1() != NULL) {
                    changes.addedAreas.insert(curve->getArea1()->getId());
                    changes.removedAreas.insert(curve->getArea1()->getId());
                    if (curve->getArea2() != NULL) {
                        changes.addedAreas.insert(curve->getArea2()->getId());
                        changes.removedAreas.insert(curve->getArea2()->getId());
                    }
                }
            }
        }
    }
    return res;
}

bool EditGraphOrthoLayer::transformVertex(GraphPtr g, CurvePtr &curve, int &point, Graph::Changes &changes)
{
    bool res = false;
    if (g != NULL) {
        if (curve != NULL && point != -1) {
            if (point != 0 && point != curve->getSize() - 1 && !curve->getIsControl(point)) {
                vec2d a, b;
                if (curve->getIsSmooth(point, a, b)) {
                    curve->removeVertex(point + 1);
                    curve->removeVertex(point - 1);
                    point -= 1;
                } else {
                    curve->addVertex(a, point - 1, true);
                    curve->addVertex(b, point + 1, true);
                    point += 1;
                }

                changes.addedCurves.insert(curve->getId());
                changes.removedCurves.insert(curve->getId());
                if (curve->getArea1() != NULL) {
                    changes.addedAreas.insert(curve->getArea1()->getId());
                    changes.removedAreas.insert(curve->getArea1()->getId());
                    if (curve->getArea2() != NULL) {
                        changes.addedAreas.insert(curve->getArea2()->getId());
                        changes.removedAreas.insert(curve->getArea2()->getId());
                    }
                }
            } else if (point == 0 || point == curve->getSize() - 1) {
                smoothNode(g, curve, point, changes, true);
            }
            res = true;
        }
    }

    return res;
}

bool EditGraphOrthoLayer::addNode()
{
    return addNode(selectedGraph, selectedCurve, selectedPoint, editGraph->getRoot()->changes);
}

bool EditGraphOrthoLayer::addNode(GraphPtr g, CurvePtr &curve, int &point, Graph::Changes &changes)
{
    bool res = false;
    if (g != NULL) {
        if (curve != NULL && point != -1 && point != 0 && point != curve->getSize() - 1) {
            g->addNode(curve, point, changes);
            res = true;
        }
    }
    return res;
}

bool EditGraphOrthoLayer::removeNode(GraphPtr g, CurvePtr &curve, int &segment, int &point, Graph::Changes &changes)
{
    bool res = false;
    if (g != NULL) {
        if (curve != NULL && point != -1) {
            if (point == 0 || point == ((int)curve->getSize() - 1) ) { // Point is a node
                NodePtr n = point == 0 ? curve->getStart() : curve->getEnd();
                if (n->getCurveCount() > 2) {
                    while (n->getCurveCount()) {
                        curve = n->getCurve(0);
                        point = curve->getVertex(n->getPos());
                        removeCurve(g, curve, changes);
                    }

                    curve = NULL;
                    point = -1;
                    res = true;
                } else if (n->getCurveCount() == 2) {
                    CurvePtr c = n->getCurve(0) == curve ? n->getCurve(1) : n->getCurve(0);
                    if (c->getStart() == c->getEnd() || curve->getStart() == curve->getEnd()) {
                        removeCurve(g, c, changes);
                        removeCurve(g, curve, changes);
                        changes.print();
                        curve = NULL;
                        point = -1;
                    } else {
                        curve = g->removeNode(curve, c, n->getPos(), changes, point);
                    }

                    res = true;
                } else if (n->getCurveCount() == 1) {
                    g->removeVertex(curve, segment, point, changes);
                    res = true;
                }
            } else { // Point is a Vertex
                g->removeVertex(curve, segment, point, changes);
                res = true;
            }
        }

    }
    return res;
}

bool EditGraphOrthoLayer::addCurve(double x1, double y1, double x2, double y2, float tolerance, GraphPtr g, CurvePtr &curve, int &segment, int &point, Graph::Changes &changes)
{
    bool res = false;
    if (g != NULL) {
        list<AreaId> areas;
        GraphPtr graph = NULL;
        if (select(x1, y1, tolerance, graph, areas, curve, segment, point)) {
            if (segment != -1) {
                addVertex(x1, y1, graph, curve, segment, point, changes);
            }
            res = addCurve(x2, y2, tolerance, graph, curve, point, changes);

        } else {
            select(x2, y2, tolerance, graph, areas, curve, segment, point);

            if (curve == NULL || graph != g) {
                curve = g->addCurve(vec2d(x1, y1), vec2d(x2, y2), changes);
                point = curve->getSize() - 1;
            } else {
                if (segment != -1) {
                    addVertex(x2, y2, graph, curve, segment, point, changes);
                }
                NodePtr n = NULL;
                if (point != 0 && point != curve->getSize() - 1) {
                    n = g->addNode(curve, point, changes);
                } else {
                    n = point == 0 ? curve->getStart() : curve->getEnd();
                }
                curve = g->addCurve(n->getId(), vec2d(x1, y1), changes);
                point = curve->getStart() == n ? 0 : curve->getSize() -1;
            }
            curve->setWidth(defaultCurveWidth);
            curve->setType(defaultCurveType);
            res = true;
        }
    }
    return res;
}

bool EditGraphOrthoLayer::addCurve(double x, double y, float tolerance, GraphPtr g, CurvePtr &curve, int &point, Graph::Changes &changes)
{
    bool res = false;
    if (g != NULL) {
        if (curve != NULL && point != -1) {
            list<AreaId> areas;

            // Storing info about previously selected curve
            CurvePtr oldCurve = curve;
            int oldPoint = point;
            int segment = -1;
            GraphPtr graph = NULL;
            // Determining if the new point is on an existing curve
            select(x, y, tolerance, graph, areas, curve, segment, point);

            NodePtr n1, n2;
            bool isExtremity1 = (oldPoint == 0 || oldPoint == oldCurve->getSize() - 1);
            bool isExtremity2 = curve == NULL ? false : (point == 0 || point == curve->getSize() - 1);

            bool invPoints = false;

            // If that's the case
            if (curve != NULL && graph == g) { // This whole part is used to determine whether we should create a new curve or not.
                bool testExtremities = true;
                if (oldCurve == curve) { //if we are linking a curve to itself
                    if (point == oldPoint || (point != -1 && (curve->getSize() == 2 || (point == oldPoint - 1 || point == oldPoint + 1)))) {
                        return true; // and if we are linking a node to itself or already linked nodes, return
                    }
                    else if (segment != -1) { // if inside a segment, we need to add a point
                        addVertex(x, y, graph, curve, segment, point, changes);
                        testExtremities = false;
                        if (oldPoint > point) {
                            oldPoint++;
                        }
                        if (point == oldPoint - 1 || point == oldPoint + 1) { // don't add anything if the segment was previous segment
                            return true;
                        }
                        if (!isExtremity1 && point > oldPoint) {
                            invPoints = true;
                        }
                    }
                }
                if ((isExtremity1 || isExtremity2) && testExtremities) {
                    NodePtr n;
                    CurvePtr c1, c2;
                    int p, s;
                    if (isExtremity1) {
                        n = oldPoint == 0 ? oldCurve->getStart() : oldCurve->getEnd();
                        c1 = curve;
                        c2 = oldCurve;
                        p = point;
                        s = segment;
                    } else {
                        n = point == 0 ? curve->getStart() : curve->getEnd();
                        c1 = oldCurve;
                        c2 = curve;
                        p = oldPoint;
                        s = -1;
                    }
                    if ((n == c1->getStart() && (p == 0 || p == 1)) || (n == c1->getEnd() && (p == c1->getSize() - 2 || p == c1->getSize() - 1))) {
                        return true;
                    }
                    if (c2->getSize() == 2 && ((c2->getOpposite(n) == c1->getStart() && p == 0) || (c2->getOpposite(n) == c1->getEnd() && p == c1->getSize() - 1))) {
                        return true;
                    }
                    if ((n == c1->getStart() && (s == 0)) || (n == c1->getEnd() && s == c1->getSize() - 2)) {
                        addVertex(x, y, graph, curve, segment, point, changes);
                        return true;
                    }
                }
                // This is to avoid bad curve transformations: we need to be sure that n1 is on Curve & n2 on a newly selected one,
                // especially if the 2 points are on the same curve (#addNode would mess all up)
                if (invPoints) {
                    n2 = isExtremity2 ? point == 0 ? curve->getStart() : curve->getEnd() : graph->addNode(curve, point, changes);
                    n1 = isExtremity1 ? oldPoint == 0 ? oldCurve->getStart() : oldCurve->getEnd() : g->addNode(oldCurve, oldPoint, changes);
                } else {
                    n1 = isExtremity1 ? oldPoint == 0 ? oldCurve->getStart() : oldCurve->getEnd() : g->addNode(oldCurve, oldPoint, changes);
                    if (segment != -1) {
                        addVertex(x, y, g, curve, segment, point, changes);
                    }
                    n2 = isExtremity2 ? point == 0 ? curve->getStart() : curve->getEnd() : graph->addNode(curve, point, changes);
                }

                curve = g->addCurve(n1->getId(), n2->getId(), changes);
            } else { // if there is no existing point at (x,y)
                if (!isExtremity1) {
                    n1 = g->addNode(oldCurve, oldPoint, changes);
                } else {
                    n1 = oldPoint == 0 ? oldCurve->getStart() : oldCurve->getEnd();
                }

                curve = g->addCurve(n1->getId(), vec2d(x, y), changes);
                n2 = n1 == curve->getStart() ? curve->getEnd() : curve->getStart();
            }
            curve->setWidth(defaultCurveWidth);
            curve->setType(defaultCurveType);

            point = curve->getStart() == n1 ? curve->getSize() - 1 : 0;
            if (n1->getCurveCount() == 2) { // Checking if we are just adding new points to a new Curve
                CurvePtr c0 = n1->getCurve(0);
                CurvePtr c1 = n1->getCurve(1);
                if (c0->getStart() != c0->getEnd() && c1->getStart() != c1->getEnd()) { // Checking that there are realy 2 curves, and that none of them is a loop (start == end).
                    bool sameStart = c0->getStart() == c1->getStart();
                    curve = g->removeNode(n1->getCurve(0), n1->getCurve(1), n1->getPos(), changes, point);
                    if (curve->getStart() == curve->getEnd()) {
                        if (sameStart) {
                            point = 0;
                        } else {
                            point = curve->getSize() - 1;
                        }
                    } else {
                        point = curve->getVertex(n2->getPos());
                    }
                }
            }

            res = true;
        }
    }
    return res;
}

/*bool EditGraphOrthoLayer::fitCurve()
{
    bool res = false;
    if (editedGraph != -1) {
        GraphPtr root = editGraph->getRoot();
        root->changes.clear();
        vector<vec2d> output;
        vector<vec2d> input;
        for (int i = 0; i < (int) displayedPoints.size(); i++) {
            vec3d v = HANDLER->getWorldCoordinates(this, displayedPoints[i].x, displayedPoints[i].y);
            input.push_back(vec2d(v.x, v.y));
        }
        root->fitCubicCurve(input, output, 50.0f);
        if ((int) output.size() != 0) {
            selectedGraph = root;

            vec2d start = output[0];
            vec2d end;

            for (int i = 1; i < (int) output.size(); i ++) {
                end = output[i];
                addCurve(start.x, start.y, end.x, end.y, tolerance, selectedGraph, selectedCurve, selectedSegment, selectedPoint, root->changes);
                start = end;
            }

            if (selectedCurve->getSize() > 4) {
                for (int i = 1; i < selectedCurve->getSize() - 1; i++) {
                    if (i % 3 != 2) {
                        selectedCurve->setIsControl(i, true);
                    } else {
                        if (i < selectedCurve->getSize() - 2) {
                            selectedCurve->setXY(i, (selectedCurve->getXY(i - 1) + selectedCurve->getXY(i + 1)) / 2.0f);
                        } else {
                            selectedCurve->addVertex((selectedCurve->getXY(i - 1) + selectedCurve->getXY(i)) / 2.0f, i - 1, false);
                        }
                    }
                }
            } else {
                smoothCurve(selectedGraph, selectedCurve, selectedPoint, selectedSegment, root->changes, true);
            }
            selectedPoint = selectedPoint == 0 ? 0 : selectedCurve->getSize() - 1;
        }
        output.clear();
        res = true;
    }
    return res;
}*/
bool EditGraphOrthoLayer::fitCurve()
{
    bool res = false;
    if (editedGraph != -1) {
        GraphPtr root = editGraph->getRoot();
        root->changes.clear();
        vector<vec2d> output;
        vector<vec2d> input;
        for (int i = 0; i < (int) displayedPoints.size(); i++) {
            vec3d v = HANDLER->getWorldCoordinates(this, displayedPoints[i].x, displayedPoints[i].y);
            input.push_back(vec2d(v.x, v.y));
        }
        root->fitCubicCurve(input, output, 50.0f);
        if ((int) output.size() != 0) {
            vec2d start = output[0], end = output[(int) output.size() - 1];
            selectedGraph = root;
            addCurve(start.x, start.y, end.x, end.y, tolerance, selectedGraph, selectedCurve, selectedSegment, selectedPoint, root->changes);
            int size = selectedCurve->getSize() - 1;

            if (selectedPoint == 0) {
                for (int i = (int) output.size() - 2; i > 0; i--) {
                    selectedCurve->addVertex(output[i], (int) output.size() - i, false);
                }
            } else {
                for (int i = 1; i < (int) output.size() - 1; i++) {
                    selectedCurve->addVertex(output[i], size + i, false);
                }
            }

            if (selectedCurve->getSize() > 4) {
                for (int i = 1; i < selectedCurve->getSize() - 1; i++) {
                    if (i % 3 != 2) {
                        selectedCurve->setIsControl(i, true);
                    } else {
                        if (i < selectedCurve->getSize() - 2) {
                            selectedCurve->setXY(i, (selectedCurve->getXY(i - 1) + selectedCurve->getXY(i + 1)) / 2.0f);
                        } else {
                            selectedCurve->addVertex((selectedCurve->getXY(i - 1) + selectedCurve->getXY(i)) / 2.0f, i - 1, false);
                        }
                    }
                }

            } else {
                smoothCurve(selectedGraph, selectedCurve, selectedPoint, selectedSegment, root->changes, true);
            }
            selectedCurve->computeCurvilinearCoordinates();
            selectedPoint = selectedPoint == 0 ? 0 : selectedCurve->getSize() - 1;
        }
        output.clear();
        res = true;
    }
    return res;
}

bool EditGraphOrthoLayer::removeCurve(GraphPtr g, CurvePtr &curve, Graph::Changes &changes)
{
    bool res = false;
    if (g != NULL) {
        if (curve != NULL) {
            g->removeCurve(curve->getId(), changes);
            res = true;
            curve = NULL;
        }
    }
    return res;
}

void EditGraphOrthoLayer::update()
{
    invalidateTiles();
    if (editedGraph != -1) {
        editGraph->getRoot()->notifyListeners();
        HANDLER->selectedCurveData.editor = this;
        HANDLER->selectedCurveData.c = selectedCurve.get();
        HANDLER->selectedCurveData.selectedSegment = selectedSegment;
        HANDLER->selectedCurveData.selectedPoint = selectedPoint;
    }
}

vec3d EditGraphOrthoLayer::getTileCoords(int level, int tx, int ty)
{
    float rootQuadSize = getRootQuadSize();
    double ox = rootQuadSize * (double(tx) / (1 << level) - 0.5f);
    double oy = rootQuadSize * (double(ty) / (1 << level) - 0.5f);
    double l = rootQuadSize / (1 << level);
    return vec3d(ox, oy, l);
}

void EditGraphOrthoLayer::swap(ptr<EditGraphOrthoLayer> p)
{
    std::swap(graphs, p->graphs);
    std::swap(editedGraph, p->editedGraph);
    std::swap(editGraph, p->editGraph);
    std::swap(usedTiles, p->usedTiles);
    std::swap(selectedCurve, p->selectedCurve);
    std::swap(selectedPoint, p->selectedPoint);
    std::swap(selectedSegment, p->selectedSegment);
    std::swap(displayLevel, p->displayLevel);
    std::swap(mesh, p->mesh);
    std::swap(tileOffsetU, p->tileOffsetU);
    std::swap(layerProgram, p->layerProgram);
    std::swap(displayedPoints, p->displayedPoints);
}

class EditGraphOrthoLayerResource : public ResourceTemplate<40, EditGraphOrthoLayer>
{
public:
    EditGraphOrthoLayerResource(ptr<ResourceManager> manager, const string &name, ptr<ResourceDescriptor> desc,
            const TiXmlElement *e = NULL) :
        ResourceTemplate<40, EditGraphOrthoLayer> (manager, name, desc)
    {
        e = e == NULL ? desc->descriptor : e;
        vector< ptr<GraphProducer> > graphs;
        int displayLevel = 0;
        float tolerance = 8.0f / 192.0f;
        bool deform = false;
        mat4d transform;
        bool softEdition = true;
        double softEditionDelay = 100000.0;
        checkParameters(desc, e, "name,graphs,renderProg,level,tolerance,terrain,deform,softEdition,softEditionDelay,");

        string names = getParameter(desc, e, "graphs") + ",";
        string::size_type start = 0;
        string::size_type index;
        while ((index = names.find(',', start)) != string::npos) {
            string name = names.substr(start, index - start);
            graphs.push_back(manager->loadResource(name).cast<GraphProducer>());
            start = index + 1;
        }

        string renderProg = "editLayerShader;";
        if (e->Attribute("renderProg") != NULL) {
            renderProg = getParameter(desc, e, "renderProg");
        }
        ptr<Program> layerProgram = manager->loadResource(renderProg).cast<Program>();
        assert(layerProgram != NULL);

        if (e->Attribute("level") != NULL) {
            getIntParameter(desc, e, "level", &displayLevel);
        }
        if (e->Attribute("tolerance") != NULL) {
            getFloatParameter(desc, e, "tolerance", &tolerance);
        }
        if (e->Attribute("deform") != NULL) {
            deform = strcmp(e->Attribute("deform"), "true") == 0;
        }
        if (e->Attribute("softEdition") != NULL) {
            softEdition = strcmp(e->Attribute("softEdition"), "true") == 0;
            if (e->Attribute("softEditionDelay") != NULL) {
                float i;
                getFloatParameter(desc, e, "softEditionDelay", &i); // should be in seconds
                softEditionDelay *= (double) (i * 1000000.0);
            }
        }
        string terrain = "";
        terrain = getParameter(desc, e, "terrain");

        init(graphs, layerProgram, displayLevel, tolerance, softEdition, softEditionDelay, deform, terrain, manager);
    }

    //To avoid call to Resource::prepareUpdate, which won't find the resource editGraphOrthoLayer since
    //it's not an actual Resource, and thus would cause an Error.
    virtual bool prepareUpdate()
    {
        return true;
    }
};

extern const char editGraphOrthoLayer[] = "editGraphOrthoLayer";

static ResourceFactory::Type<editGraphOrthoLayer, EditGraphOrthoLayerResource> EditGraphOrthoLayerType;

}
