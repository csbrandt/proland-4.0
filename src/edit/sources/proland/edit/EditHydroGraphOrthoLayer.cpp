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

#include "proland/edit/EditHydroGraphOrthoLayer.h"

#include "ork/render/FrameBuffer.h"
#include "proland/producer/ObjectTileStorage.h"
#include "proland/rivers/graph/LazyHydroGraph.h"

using namespace ork;

namespace proland
{

bool EditHydroGraphOrthoLayer::EditHydroGraphHandler::mouseClick(button b, state s, modifier m, int x, int y)
{
    if ((b == LEFT_BUTTON) && ((m & SHIFT) == 0) && ((m & ALT) == 0) && ((m & CTRL) == 0)) {
        if (s == DOWN) {
            // Storing previous selection.
            CurvePtr c;
            int p, s;
            editor->getSelection(c, p, s);

            newPos = getWorldCoordinates(x, y);
            if (editor->select((float) newPos.x, (float) newPos.y, editor->getTolerance())) {
                if (editor->getSelectedSegment() != -1) {
                    editor->displayedPoints.clear();
                    editor->displayedPoints.push_back(vec2i(x, y));
                    editor->displayedPoints.push_back(vec2i(x, y));
                    dynamic_cast<EditHydroGraphOrthoLayer*>(editor)->addingRiver = true;
                    mode = EDIT_MODE;
                    editor->getEditedGraphPtr()->getRoot()->changes.clear();
                    edited = true;
                    update();
                    return true;
                }
            }
            editor->setSelection(c, p, s);
        } else {
            if (dynamic_cast<EditHydroGraphOrthoLayer*>(editor)->addingRiver) {
                newPos = getWorldCoordinates(x, y);
                ptr<Curve> old = editor->getSelectedCurve();
                if (editor->select((float) newPos.x, (float) newPos.y, editor->getTolerance())) {
                    if (! (old->getId() == editor->getSelectedCurve()->getId())) {
                        old->setType(HydroCurve::BANK);
                        old.cast<HydroCurve>()->setRiver(editor->getSelectedCurve()->getId());
                        GraphPtr g = editor->getEditedGraphPtr()->getRoot();
                        g->changes.addedCurves.insert(old->getId());
                        g->changes.removedCurves.insert(old->getId());
                        if (old->getArea1() != NULL) {
                            g->changes.addedAreas.insert(old->getArea1()->getId());
                            g->changes.removedAreas.insert(old->getArea1()->getId());
                            if (old->getArea2() != NULL) {
                                g->changes.addedAreas.insert(old->getArea2()->getId());
                                g->changes.removedAreas.insert(old->getArea2()->getId());
                            }
                        }
                    }
                }
                edited = true;
                dynamic_cast<EditHydroGraphOrthoLayer*>(editor)->addingRiver = false;
                editor->displayedPoints.clear();
                mode = DEFAULT_MODE;
                update();
                return true;
            }
        }
    }

    return EditGraphOrthoLayer::EditGraphHandler::mouseClick(b, s, m, x, y);
}

bool EditHydroGraphOrthoLayer::EditHydroGraphHandler::mouseMotion(int x, int y)
{
    if (mode == EDIT_MODE) {
        newPos = getWorldCoordinates(x, y);
        if (dynamic_cast<EditHydroGraphOrthoLayer*>(editor)->addingRiver) {
            editor->displayedPoints[1] = vec2i(x, y);
            edited = true;
            return true;
        }
    }
    return EditGraphOrthoLayer::EditGraphHandler::mouseMotion(x, y);
}

void EditHydroGraphOrthoLayer::init(const vector< ptr<GraphProducer> > &graphs, ptr<Program> layerProg, int displayLevel, float tolerance, bool softEdition, double softEditionDelay, bool deform, string terrain, ptr<ResourceManager> manager)
{
    TileLayer::init(deform);
    defaultCurveType = 0;
    defaultCurveWidth = 5.0f;
    selectedCurve = NULL;
    selectedGraph = NULL;
    selectedPoint = -1;
    selectedSegment = -1;
    curveStart = vec3f(0.0f, 0.0f, 0.0f);
    this->tolerance = tolerance;
    this->addingRiver = false;

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
    HANDLER->addHandler(this, new EditHydroGraphHandler(this, manager, terrain));
    this->softEdition = softEdition;
    this->softEditionDelay = softEditionDelay;
}

CurvePtr EditHydroGraphOrthoLayer::findRiver(TileCache::Tile *t, double x1, double y1, double x2, double y2, int &orientationC2R, int &orientationR2C)
{
    vec3d q = getTileCoords(t->level, t->tx, t->ty);

    TileCache::Tile *child = NULL;
    int level = t->level + 1;

    int width = 1 << t->level;
    float tileWidth = q.z / (1 << (level - t->level));
    int tx = (int) ((x1 + q.z * width / 2) / tileWidth);
    int ty = (int) ((y1 + q.z * width / 2) / tileWidth);

    vec3d q2 = getTileCoords(level, tx, ty);
    if ((x1 > q2.x) && (x2 > q2.x) && (y1 > q2.y) && (y2 > q2.y) && (x1 < q2.x + q2.z) && (x2 < q2.x + q2.z) && (y1 < q2.y + q2.z) && (y2 < q2.y + q2.z)) {
        child = editGraph->findTile(level, tx, ty);

        if (child != NULL && child->task->isDone()) {
            return findRiver(child, x1, y1, x2, y2, orientationC2R, orientationR2C);
        }
    }
    GraphPtr p = dynamic_cast<ObjectTileStorage::ObjectSlot *>(t->getData())->data.cast<Graph>();

    return findRiver(p, x1, y1, x2, y2, orientationC2R, orientationR2C);
}

CurvePtr EditHydroGraphOrthoLayer::findRiver(GraphPtr p, double x1, double y1, double x2, double y2, int &orientationC2R, int &orientationR2C)
{
    ptr<Graph::CurveIterator> ci = p->getCurves();
    float width;
    CurvePtr closestCurve = NULL;
    float minDist = 1e9;
    int closestPoint = -1;
    float dist;
    int ind1 = -1, ind2 = -1;
    vec2d start = vec2d(x1, y1);
    vec2d end = vec2d (x2, y2);
    seg2d s = seg2d(start, end);
    bool foundStart, foundEnd;

    while (ci->hasNext()) {
        CurvePtr cp = ci->next();
        width = cp->getWidth();
        if (cp->getType() != HydroCurve::AXIS) {
            continue;
        }
        foundStart = false;
        foundEnd = false;
        vec2d cur;
        vec2d next = cp->getXY(0);
        ind1 = -1;
        ind2 = -1;
        for (int i = 0; i < cp->getSize(); i++) {
            cur = next;
            next = cp->getXY(i + 1);
            seg2d ab = seg2d(cur, next);
            if (s.intersects(ab)) {
                orientationC2R = 0;
                orientationR2C = 2;
                return NULL;
            }
            if (!foundStart) {
                dist = ab.segmentDistSq(start);
                if (dist < width*width) {
                    foundStart = true;
                    ind1 = i;
                    if (minDist > dist) {
                        minDist = dist;
                        closestCurve = cp;
                        closestPoint = i;
                    }
                }
            }
            if (!foundEnd) {
                dist = ab.segmentDistSq(end);
                if (dist < width*width) {
                    ind2 = i;
                    foundEnd = true;
                    if (minDist > dist) {
                        minDist = dist;
                        closestCurve = cp;
                        closestPoint = i;
                    }
                }
            }
        }
        if (foundStart && foundEnd) {
            if (ind1 == ind2) {
                ind2 = ind1 + 1;
            }

            orientationC2R = (cross(end - start, cp->getXY(ind1) - start) < 0.0f) && (cross(end - start, cp->getXY(ind2) - start) < 0.0f);
            vec2d a = cp->getXY(min(ind1, ind2));
            vec2d b = cp->getXY(max(ind1, ind2));
            orientationR2C = (cross(b - a, start - a) < 0.0f);

            return cp->getAncestor();
        }
    }
    if (closestCurve != NULL) {
        orientationC2R = (cross(end - start, closestCurve->getXY(closestPoint) - start) < 0.0f);

        vec2d a = closestCurve->getXY(closestPoint);
        vec2d b = closestCurve->getXY(closestPoint + 1);
        orientationR2C = (cross(b - a, start - a) < 0.0f);

        closestCurve = closestCurve->getAncestor();
    }
    return closestCurve;
}

bool EditHydroGraphOrthoLayer::addCurve(double x1, double y1, double x2, double y2, float tolerance, GraphPtr g, CurvePtr &curve, int &segment, int &point, Graph::Changes &changes)
{
    bool res = false;
    if (defaultCurveType != HydroCurve::AXIS) {
        int orientationC2R, orientationR2C;
        TileCache::Tile *t = editGraph->findTile(0, 0, 0);
        CurvePtr river = findRiver(t, x1, y1, x2, y2, orientationC2R, orientationR2C);
        if (orientationC2R) {
            res = EditGraphOrthoLayer::addCurve(x2, y2, x1, y1, tolerance, g, curve, segment, point, changes);
            point = 0;
        } else {
            res = EditGraphOrthoLayer::addCurve(x1, y1, x2, y2, tolerance, g, curve, segment, point, changes);
        }
        if (res) {
            if (orientationR2C == 2) {
                curve->setType(HydroCurve::CLOSING_SEGMENT);
            } else {
                if (river != NULL) {
                    curve.cast<HydroCurve>()->setRiver(river->getId());
                    curve.cast<HydroCurve>()->setPotential(orientationR2C ? 0.f : river->getWidth());
                    curve->setType(HydroCurve::BANK);
                } else {
                    curve->setType(defaultCurveType);
                }
            }
        } else {
            if (Logger::ERROR_LOGGER != NULL) {
                Logger::ERROR_LOGGER->log("RIVERS", "Error while adding curve");
            }
        }
    } else {
        res = EditGraphOrthoLayer::addCurve(x1, y1, x2, y2, tolerance, g, curve, segment, point, changes);
    }

    return res;
}

bool EditHydroGraphOrthoLayer::addCurve(double x, double y, float tolerance, GraphPtr g, CurvePtr &curve, int &point, Graph::Changes &changes)
{
    bool res = false;
    if (g != NULL) {
        if (curve != NULL && point != -1) {
            float width = curve->getWidth();
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
            CurvePtr river = NULL;
            TileCache::Tile *t = editGraph->findTile(0, 0, 0);
            int orientationC2R = 0, orientationR2C = 0;

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

                if (oldCurve->getType() != HydroCurve::AXIS) {
                    river = findRiver(t, n1->getPos().x, n1->getPos().y, n2->getPos().x, n2->getPos().y, orientationC2R, orientationR2C);
                }

//                if (orientationC2R) {
//                    curve = g->addCurve(n2->getId(), n1->getId(), changes);
//                } else {
                    curve = g->addCurve(n1->getId(), n2->getId(), changes);
//                }
            } else { // if there is no existing point at (x,y)
                if (!isExtremity1) {
                    n1 = g->addNode(oldCurve, oldPoint, changes);
                } else {
                    n1 = oldPoint == 0 ? oldCurve->getStart() : oldCurve->getEnd();
                }

                if (oldCurve->getType() != HydroCurve::AXIS) {
                    river = findRiver(t, n1->getPos().x, n1->getPos().y, x, y, orientationC2R, orientationR2C);
                }
                if (orientationC2R) {
//                    curve = g->addCurve(vec2f(x, y), n1->getId(), changes);
                    curve = g->addCurve(n1->getId(), vec2d(x, y), changes);
                    curve->invert();
                    if (orientationR2C == 2) {
                        curve->setType(HydroCurve::CLOSING_SEGMENT);
                    } else {
                        curve->setType(HydroCurve::BANK);
                        curve.cast<HydroCurve>()->setRiver(river->getId());
                        curve.cast<HydroCurve>()->setPotential(orientationR2C ? 0.f : river->getWidth());
                    }
                } else {
                    curve = g->addCurve(n1->getId(), vec2d(x, y), changes);
                }
                n2 = n1 == curve->getStart() ? curve->getEnd() : curve->getStart();
            }

            point = curve->getStart() == n1 ? curve->getSize() -1 : 0;
            if (n1->getCurveCount() == 2) { // Checking if we are just adding new points to an existing Curve and if we can merge the two
                CurvePtr c0 = n1->getCurve(0);
                CurvePtr c1 = n1->getCurve(1);
                bool doMerge = true;

                if (oldCurve->getType() != HydroCurve::AXIS) {
                    if (orientationR2C < 2) {
                        if (river != NULL) {
                            doMerge = (oldCurve->getEnd() == curve->getStart() || oldCurve->getStart() == curve->getEnd()) && oldCurve->getType() == HydroCurve::BANK && oldCurve.cast<HydroCurve>()->getRiver() == river->getId();
                        }
                    } else {
                        doMerge = false;
                    }
                    if (doMerge == false) {
                        if (orientationR2C == 2) {
                            curve->setType(HydroCurve::CLOSING_SEGMENT);
                        } else {
                            if (river != NULL) {
                                curve->setType(HydroCurve::BANK);
                                curve.cast<HydroCurve>()->setRiver(river->getId());
                                curve.cast<HydroCurve>()->setPotential(orientationR2C ? 0.f : river->getWidth());
                            }
                        }
                    }
                }
                if (doMerge && c0->getStart() != c0->getEnd() && c1->getStart() != c1->getEnd()) { // Checking that there are really 2 curves, and that none of them is a loop (start == end).
                    curve = g->removeNode(n1->getCurve(0), n1->getCurve(1), n1->getPos(), changes, point);
                    point = curve->getVertex(n2->getPos());
                }
            } else {
                if (oldCurve->getType() != HydroCurve::AXIS) {

                    if (orientationR2C == 2) {
                        curve->setType(HydroCurve::CLOSING_SEGMENT);
                    } else {
                        if (river != NULL) {
                            curve->setType(HydroCurve::BANK);
                            curve.cast<HydroCurve>()->setRiver(river->getId());
                            curve.cast<HydroCurve>()->setPotential(orientationR2C ? 0.f : river->getWidth());
                        }
                    }
                } else {
                    curve->setType(defaultCurveType);
                }
            }
            if (curve->getType() == HydroCurve::AXIS || river == NULL) {
                curve->setWidth(width);
            }
            res = true;
        }
    }
    return res;
}

void EditHydroGraphOrthoLayer::swap(ptr<EditHydroGraphOrthoLayer> p)
{
    EditGraphOrthoLayer::swap(p);
    std::swap(addingRiver, p->addingRiver);
}

class EditHydroGraphOrthoLayerResource : public ResourceTemplate<40, EditHydroGraphOrthoLayer>
{
public:
    EditHydroGraphOrthoLayerResource(ptr<ResourceManager> manager, const string &name, ptr<ResourceDescriptor> desc,
            const TiXmlElement *e = NULL) :
        ResourceTemplate<40, EditHydroGraphOrthoLayer> (manager, name, desc)
    {
        e = e == NULL ? desc->descriptor : e;
        vector< ptr<GraphProducer> > graphs;
        int displayLevel = 0;
        float tolerance = 8.0f / 192.0f;
        bool deform = false;
        mat4d transform;
        bool softEdition = true;
        double softEditionDelay = 100000;
        checkParameters(desc, e, "name,graphs,renderProg,level,tolerance,softEdition,softEditionDelay,terrain,deform,");

        string names = getParameter(desc, e, "graphs") + ",";
        string::size_type start = 0;
        string::size_type index;
        while ((index = names.find(',', start)) != string::npos) {
            string name = names.substr(start, index - start);
            ptr<GraphProducer> producer = manager->loadResource(name).cast<GraphProducer>();
            assert("Graph Must be an HydroGraph!\n" && (producer->getRoot().cast<HydroGraph>() != NULL || producer->getRoot().cast<LazyHydroGraph>() != NULL));
            graphs.push_back(producer);
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
                softEditionDelay *= (double)(i * 1000000.0);
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

extern const char editHydroGraphOrthoLayer[] = "editHydroGraphOrthoLayer";

static ResourceFactory::Type<editHydroGraphOrthoLayer, EditHydroGraphOrthoLayerResource> EditHydroGraphOrthoLayerType;

}
