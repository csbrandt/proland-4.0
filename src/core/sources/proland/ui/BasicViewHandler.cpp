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

#include "proland/ui/BasicViewHandler.h"

#include "ork/resource/ResourceTemplate.h"
#include "ork/render/FrameBuffer.h"
#include "proland/terrain/TerrainNode.h"

using namespace std;
using namespace ork;

namespace proland
{

double mix2(double x, double y, double t)
{
    return abs(x - y) < max(x, y) * 1e-5 ? y : mix(x, y, t);
}

BasicViewHandler::Position::Position() : x0(0.0), y0(0.0), theta(0.0), phi(0.0), d(0.0), sx(0.0), sy(0.0), sz(0.0)
{
}

BasicViewHandler::BasicViewHandler() : EventHandler("BasicViewHandler")
{
}

BasicViewHandler::BasicViewHandler(bool smooth, ViewManager *view, ptr<EventHandler> next) : EventHandler("BasicViewHandler")
{
    init(smooth, view, next);
}

void BasicViewHandler::init(bool smooth, ViewManager *view, ptr<EventHandler> next)
{
    this->viewManager = view;
    this->next = next;
    this->smooth = smooth;
    this->near = false;
    this->far = false;
    this->forward = false;
    this->backward = false;
    this->left = false;
    this->right = false;
    this->initialized = false;
    this->animation = -1;
}

BasicViewHandler::~BasicViewHandler()
{
}

void BasicViewHandler::redisplay(double t, double dt)
{
    if (!initialized) {
        getPosition(target);
        initialized = true;
    }

    ptr<TerrainViewController> controller = getViewManager()->getViewController();

    if (animation >= 0.0) {
        animation = controller->interpolate(start.x0, start.y0, start.theta, start.phi, start.d,
            end.x0, end.y0, end.theta, end.phi, end.d, animation);

        vec3d startl = vec3d(start.sx, start.sy, start.sz);
        vec3d endl = vec3d(end.sx, end.sy, end.sz);
        vec3d l = (startl * (1.0 - animation) + endl * animation).normalize();
        SceneManager::NodeIterator i = getViewManager()->getScene()->getNodes("light");
        if (i.hasNext()) {
            ptr<SceneNode> n = i.next();
            n->setLocalToParent(mat4d::translate(vec3d(l.x, l.y, l.z)));
        }

        if (animation == 1.0) {
            getPosition(target);
            animation = -1.0;
        }
    } else {
        updateView(t, dt);
    }
    controller->update();
    controller->setProjection();

    ptr<FrameBuffer> fb = FrameBuffer::getDefault();
    fb->clear(true, false, true);

    getViewManager()->getScene()->update(t, dt);
    getViewManager()->getScene()->draw();

    double lerp = 1.0 - exp(-dt * 2.301e-6);
    double gh = mix2(controller->getGroundHeight(), TerrainNode::groundHeightAtCamera, lerp);
    controller->setGroundHeight(gh);

    if (next != NULL) {
        next->redisplay(t, dt);
    }
}

void BasicViewHandler::reshape(int x, int y)
{
    if (next != NULL) {
        next->reshape(x, y);
    }
}

void BasicViewHandler::idle(bool damaged)
{
    if (next != NULL) {
        next->idle(damaged);
    }
}

bool BasicViewHandler::mouseClick(button b, state s, modifier m, int x, int y)
{
    oldx = x;
    oldy = y;
    if ((m & CTRL) != 0) {
        mode = rotate;
        return true;
    } else if (m == 0) { // no modifier
        if (b == LEFT_BUTTON) {
            mode = move;
        } else {
            mode = light;
        }
        return true;
    }
    return next != NULL && next->mouseClick(b, s, m, x, y);
}

bool BasicViewHandler::mouseMotion(int x, int y)
{
    if (!initialized) {
        getPosition(target);
        initialized = true;
    }
    if (mode == rotate) {
        target.phi += (oldx - x) / 500.0;
        target.theta += (oldy - y) / 500.0;
        target.theta = max((float) -M_PI, min((float) M_PI, (float) target.theta));
        oldx = x;
        oldy = y;
        return true;
    } else if (mode == move) {
        vec3d oldp = getViewManager()->getWorldCoordinates(oldx, oldy);
        vec3d p = getViewManager()->getWorldCoordinates(x, y);
        if (!(isNaN(oldp.x) || isNaN(oldp.y) || isNaN(oldp.z) || isNaN(p.x) || isNaN(p.y) || isNaN(p.z))) {
            Position current;
            getPosition(current, false);
            setPosition(target, false);
            ptr<TerrainViewController> controller = getViewManager()->getViewController();
            controller->move(oldp, p);
            getPosition(target, false);
            setPosition(current, false);
        }
        oldx = x;
        oldy = y;
        return true;
    } else if (mode == light) {
        float vangle = (float) safe_asin(target.sz);
        float hangle = (float) atan2(target.sy, target.sx);
        vangle += radians((float)(oldy - y)) * 0.25f;
        hangle += radians((float)(oldx - x)) * 0.25f;
        target.sx = cos(vangle) * cos(hangle);
        target.sy = cos(vangle) * sin(hangle);
        target.sz = sin(vangle);

        oldx = x;
        oldy = y;
        return true;
    }
    return next != NULL && next->mouseMotion(x, y);
}

bool BasicViewHandler::mousePassiveMotion(int x, int y)
{
    return next != NULL && next->mousePassiveMotion(x, y);
}

bool BasicViewHandler::mouseWheel(wheel b, modifier m, int x, int y)
{
    if (!initialized) {
        getPosition(target);
        initialized = true;
    }
    ptr<TerrainViewController> controller = getViewManager()->getViewController();
    const float dzFactor = 1.2f;
    if (b == WHEEL_DOWN) {
        target.d = target.d * dzFactor;
        return true;
    }
    if (b == WHEEL_UP) {
        target.d = target.d / dzFactor;
        return true;
    }
    return next != NULL && next->mouseWheel(b, m, x, y);
}

bool BasicViewHandler::keyTyped(unsigned char c, modifier m, int x, int y)
{
    return next != NULL && next->keyTyped(c, m, x, y);
}

bool BasicViewHandler::keyReleased(unsigned char c, modifier m, int x, int y)
{
    return next != NULL && next->keyReleased(c, m, x, y);
}

bool BasicViewHandler::specialKey(key k, modifier m, int x, int y)
{
    switch (k) {
    case KEY_F10:
        smooth = !smooth;
        return true;
    case KEY_PAGE_UP:
        far = true;
        return true;
    case KEY_PAGE_DOWN:
        near = true;
        return true;
    case KEY_UP:
        forward = true;
        return true;
    case KEY_DOWN:
        backward = true;
        return true;
    case KEY_LEFT:
        left = true;
        return true;
    case KEY_RIGHT:
        right = true;
        return true;
    default:
        break;
    }
    return next != NULL && next->specialKey(k, m, x, y);
}

bool BasicViewHandler::specialKeyReleased(key k, modifier m, int x, int y)
{
    switch (k) {
    case KEY_PAGE_UP:
        far = false;
        return true;
    case KEY_PAGE_DOWN:
        near = false;
        return true;
    case KEY_UP:
        forward = false;
        return true;
    case KEY_DOWN:
        backward = false;
        return true;
    case KEY_LEFT:
        left = false;
        return true;
    case KEY_RIGHT:
        right = false;
        return true;
    default:
        break;
    }
    return next != NULL && next->specialKeyReleased(k, m, x, y);
}

void BasicViewHandler::getPosition(Position &p, bool light)
{
    ptr<TerrainViewController> view = getViewManager()->getViewController();
    p.x0 = view->x0;
    p.y0 = view->y0;
    p.theta = view->theta;
    p.phi = view->phi;
    p.d = view->d;
    if (light) {
        SceneManager::NodeIterator i = getViewManager()->getScene()->getNodes("light");
        if (i.hasNext()) {
            ptr<SceneNode> n = i.next();
            vec3d l = n->getLocalToParent() * vec3d::ZERO;
            p.sx = l.x;
            p.sy = l.y;
            p.sz = l.z;
        }
    }
}

void BasicViewHandler::setPosition(const Position &p, bool light)
{
    ptr<TerrainViewController> view = getViewManager()->getViewController();
    view->x0 = p.x0;
    view->y0 = p.y0;
    view->theta = p.theta;
    view->phi = p.phi;
    view->d = p.d;
    if (light) {
        SceneManager::NodeIterator i = getViewManager()->getScene()->getNodes("light");
        if (i.hasNext()) {
            ptr<SceneNode> n = i.next();
            n->setLocalToParent(mat4d::translate(vec3d(p.sx, p.sy, p.sz)));
        }
    }
    animation = -1.0;
}

void BasicViewHandler::goToPosition(const Position &p)
{
    getPosition(start);
    end = p;
    animation = 0.0;
}

void BasicViewHandler::jumpToPosition(const Position &p)
{
    setPosition(p, true);
    target = p;
}

ViewManager *BasicViewHandler::getViewManager()
{
    return viewManager;
}

void BasicViewHandler::updateView(double t, double dt)
{
    ptr<TerrainViewController> controller = getViewManager()->getViewController();
    const float dzFactor = pow(1.02f, std::min(float(50.0e-6 * dt), 1.0f));
    if (near) {
        target.d = target.d / dzFactor;
    } else if (far) {
        target.d = target.d * dzFactor;
    }
    Position p;
    getPosition(p, true);
    setPosition(target, false);
    if (forward) {
        float speed = max(controller->getHeight() - controller->getGroundHeight(), 0.0);
        controller->moveForward(speed * dt * 1e-6);
    } else if (backward) {
        float speed = max(controller->getHeight() - controller->getGroundHeight(), 0.0);
        controller->moveForward(-speed * dt * 1e-6);
    }
    if (left) {
        controller->turn(dt * 5e-7);
    } else if (right) {
        controller->turn(-dt * 5e-7);
    }
    getPosition(target, false);

    if (smooth) {
        double lerp = 1.0 - exp(-dt * 2.301e-6);
        double x0;
        double y0;
        controller->interpolatePos(p.x0, p.y0, target.x0, target.y0, lerp, x0, y0);
        p.x0 = x0;
        p.y0 = y0;
        p.theta = mix2(p.theta, target.theta, lerp);
        p.phi = mix2(p.phi, target.phi, lerp);
        p.d = mix2(p.d, target.d, lerp);
        p.sx = mix2(p.sx, target.sx, lerp);
        p.sy = mix2(p.sy, target.sy, lerp);
        p.sz = mix2(p.sz, target.sz, lerp);
        double l = 1.0 / sqrt(p.sx * p.sx + p.sy * p.sy + p.sz * p.sz);
        p.sx *= l;
        p.sy *= l;
        p.sz *= l;
        setPosition(p);
    } else {
        setPosition(target);
    }
}

void BasicViewHandler::swap(ptr<BasicViewHandler> o)
{
    std::swap(viewManager, o->viewManager);
    std::swap(next, o->next);
    std::swap(mode, o->mode);
    std::swap(oldx, o->oldx);
    std::swap(oldy, o->oldy);
    std::swap(near, o->near);
    std::swap(far, o->far);
}

class BasicViewHandlerResource : public ResourceTemplate<100, BasicViewHandler>
{
public:
    string view;

    BasicViewHandlerResource(ptr<ResourceManager> manager, const string &name, ptr<ResourceDescriptor> desc, const TiXmlElement *e = NULL) :
        ResourceTemplate<100, BasicViewHandler> (manager, name, desc)
    {
        e = e == NULL ? desc->descriptor : e;
        checkParameters(desc, e, "name,viewManager,smooth,next,");

        view = getParameter(desc, e, "viewManager");

        bool smooth = true;
        ptr<EventHandler> next;
        if (e->Attribute("smooth") != NULL) {
            smooth = strcmp(e->Attribute("smooth"), "true") == 0;
        }
        if (e->Attribute("next") != NULL) {
            next = manager->loadResource(getParameter(desc, e, "next")).cast<EventHandler>();
        }

        init(smooth, NULL, next);
    }

    virtual ViewManager *getViewManager()
    {
        if (viewManager == NULL) {
            viewManager = dynamic_cast<ViewManager*>(manager->loadResource(view).get());
        }
        return viewManager;
    }
};

extern const char basicViewHandler[] = "basicViewHandler";

static ResourceFactory::Type<basicViewHandler, BasicViewHandlerResource> BasicViewHandlerType;

}
