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

#include "ork/core/FileLogger.h"
#include "ork/render/FrameBuffer.h"
#include "ork/resource/ResourceTemplate.h"
#include "ork/resource/XMLResourceLoader.h"
#include "ork/scenegraph/AbstractTask.h"
#include "ork/scenegraph/SceneManager.h"
#include "ork/scenegraph/ShowLogTask.h"
#include "ork/ui/GlutWindow.h"

#include "proland/OceanPlugin.h"
#include "proland/preprocess/atmo/PreprocessAtmo.h"
#include "proland/ui/BasicViewHandler.h"
#include "proland/util/PlanetViewController.h"

using namespace ork;
using namespace proland;

class HelloWorld : public GlutWindow, public ViewManager
{
public:
    ptr<SceneManager> scene;
    ptr<TerrainViewController> controller;
    ptr<BasicViewHandler> view;
    ptr<EventHandler> ui;

    HelloWorld() : GlutWindow(Window::Parameters().size(1024, 768))
    {
    }

    virtual ~HelloWorld()
    {
    }

    virtual void redisplay(double t, double dt)
    {
        if (getViewController()->getNode() != scene->getCameraNode()) {
            updateResources();
        }

        ui->redisplay(t, dt);
        GlutWindow::redisplay(t, dt);

        if (Logger::ERROR_LOGGER != NULL) {
            Logger::ERROR_LOGGER->flush();
        }
    }

    virtual void reshape(int x, int y)
    {
        ptr<FrameBuffer> fb = FrameBuffer::getDefault();
        fb->setDepthTest(true, LESS);
		fb->setViewport(vec4<GLint>(0, 0, x, y));
        ui->reshape(x, y);
        GlutWindow::reshape(x, y);
        idle(false);
    }

    virtual void idle(bool damaged)
    {
        GlutWindow::idle(damaged);
        if (damaged) {
            updateResources();
        }
        ui->idle(damaged);
    }

    virtual bool mouseClick(button b, state s, modifier m, int x, int y)
    {
        return ui->mouseClick(b, s, m, x, y);
    }

    virtual bool mouseMotion(int x, int y)
    {
        return ui->mouseMotion(x, y);
    }

    virtual bool mousePassiveMotion(int x, int y)
    {
        return ui->mousePassiveMotion(x, y);
    }

    virtual bool mouseWheel(wheel b, modifier m, int x, int y)
    {
        return ui->mouseWheel(b, m, x, y);
    }

    virtual bool keyTyped(unsigned char c, modifier m, int x, int y)
    {
        if (ui->keyTyped(c, m, x, y)) {
            return true;
        }
        if (c == 27) {
            ::exit(0);
        }
        return false;
    }

    virtual bool keyReleased(unsigned char c, modifier m, int x, int y)
    {
        return ui->keyReleased(c, m, x, y);
    }

    virtual bool specialKey(key k, modifier m, int x, int y)
    {
        if (ui->specialKey(k, m, x, y)) {
            return true;
        }

        switch (k) {
        case KEY_F1:
            ShowLogTask::enabled = !ShowLogTask::enabled;
            return true;
        case KEY_F5:
            updateResources();
            return true;
        default:
            break;
        }
        return false;
    }

    virtual bool specialKeyReleased(key k, modifier m, int x, int y)
    {
        return ui->specialKeyReleased(k, m, x, y);
    }

    virtual ptr<SceneManager> getScene()
    {
        return scene;
    }

    virtual ptr<TerrainViewController> getViewController()
    {
        return controller;
    }

    virtual vec3d getWorldCoordinates(int x, int y)
    {
        vec3d p = scene->getWorldCoordinates(x, y);
        if (controller.cast<PlanetViewController>() != NULL) {
            if (p.length() > controller.cast<PlanetViewController>()->R * 1.1) {
                p = vec3d(NAN, NAN, NAN);
            }
        } else if (abs(p.x) > 100000.0 || abs(p.y) > 100000.0 || abs(p.z) > 100000.0) {
            p = vec3d(NAN, NAN, NAN);
        }
        return p;
    }

    void updateResources()
    {
        BasicViewHandler::Position p;
        view->getPosition(p);
        scene->getResourceManager()->updateResources();
        getViewController()->setNode(scene->getCameraNode());
        view->setPosition(p);
    }

protected:
    void swap(ptr<HelloWorld> o)
    {
        std::swap(scene, o->scene);
        std::swap(ui, o->ui);
        std::swap(view, o->view);
        std::swap(controller, o->controller);
    }
};

class HelloWorldResource : public ResourceTemplate<100, HelloWorld>
{
public:
    HelloWorldResource(ptr<ResourceManager> manager, const std::string &name, ptr<ResourceDescriptor> desc, const TiXmlElement *e = NULL) :
        ResourceTemplate<100, HelloWorld> (manager, name, desc)
    {
        e = e == NULL ? desc->descriptor : e;
        checkParameters(desc, e, "name,ui,view,radius,");

        scene = new SceneManager();
        scene->setResourceManager(manager);
        scene->setScheduler(manager->loadResource("defaultScheduler").cast<Scheduler>());
        scene->setRoot(manager->loadResource("scene").cast<SceneNode>());
        scene->setCameraNode("camera");
        scene->setCameraMethod("draw");
        ui = manager->loadResource(getParameter(desc, e, "ui")).cast<EventHandler>();
        view = manager->loadResource(getParameter(desc, e, "view")).cast<BasicViewHandler>();
        if (e->Attribute("radius") != NULL) {
            float radius;
            getFloatParameter(desc, e, "radius", &radius);
            controller = new PlanetViewController(scene->getCameraNode(), radius);
        } else {
            controller = new TerrainViewController(scene->getCameraNode(), 50000.0);
        }
    }

    virtual void doRelease()
    {
        if (manager != NULL) {
            manager->close();
        }
        delete this;
    }
};

extern const char helloworld[] = "helloworld";

static ResourceFactory::Type<helloworld, HelloWorldResource> HelloWorldType;

static_ptr<Window> app;

void init()
{
    preprocessAtmo(AtmoParameters(), ".");

    FileLogger::File *out = new FileLogger::File("log.html");
    Logger::INFO_LOGGER = new FileLogger("INFO", out, Logger::INFO_LOGGER);
    Logger::WARNING_LOGGER = new FileLogger("WARNING", out, Logger::WARNING_LOGGER);
    Logger::ERROR_LOGGER = new FileLogger("ERROR", out, Logger::ERROR_LOGGER);

    ptr<XMLResourceLoader> resLoader = new XMLResourceLoader();
    resLoader->addPath(".");
    resLoader->addArchive("helloworld.xml");

    ptr<ResourceManager> resManager = new ResourceManager(resLoader, 8);

    app = resManager->loadResource("window").cast<Window>();
}

int main(int argc, char* argv[])
{
    initOceanPlugin();
    atexit(Object::exit);
    init();
    app->start();
    return 0;
}
