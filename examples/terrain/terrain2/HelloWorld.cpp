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

#include <stdlib.h>

#include "ork/core/FileLogger.h"
#include "ork/render/FrameBuffer.h"
#include "ork/resource/XMLResourceLoader.h"
#include "ork/scenegraph/SceneManager.h"
#include "ork/ui/GlutWindow.h"

#include "proland/TerrainPlugin.h"
#include "proland/terrain/TerrainNode.h"
#include "proland/util/PlanetViewController.h"

using namespace ork;
using namespace proland;

class HelloWorld : public GlutWindow
{
public:
    ptr<SceneManager> manager;
    ptr<PlanetViewController> controller;
    int mouseX, mouseY;
    bool rotate;

    HelloWorld() : GlutWindow(Window::Parameters().size(1024, 768))
    {
        FileLogger::File *out = new FileLogger::File("log.html");
        Logger::INFO_LOGGER = new FileLogger("INFO", out, Logger::INFO_LOGGER);
        Logger::WARNING_LOGGER = new FileLogger("WARNING", out, Logger::WARNING_LOGGER);
        Logger::ERROR_LOGGER = new FileLogger("ERROR", out, Logger::ERROR_LOGGER);

        ptr<XMLResourceLoader> resLoader = new XMLResourceLoader();
        resLoader->addPath(".");
        resLoader->addArchive("helloworld.xml");

        ptr<ResourceManager> resManager = new ResourceManager(resLoader, 8);

        manager = new SceneManager();
        manager->setResourceManager(resManager);

        manager->setScheduler(resManager->loadResource("defaultScheduler").cast<Scheduler>());
        manager->setRoot(resManager->loadResource("scene").cast<SceneNode>());
        manager->setCameraNode("camera");
        manager->setCameraMethod("draw");

        controller = new PlanetViewController(manager->getCameraNode(), 6360000.0);
    }

    virtual ~HelloWorld()
    {
    }

    virtual void redisplay(double t, double dt)
    {
        controller->setGroundHeight(TerrainNode::groundHeightAtCamera);
        controller->update();
        controller->setProjection();

        ptr<FrameBuffer> fb = FrameBuffer::getDefault();
        fb->clear(true, false, true);

        manager->update(t, dt);
        manager->draw();

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
        GlutWindow::reshape(x, y);
    }

    virtual void idle(bool damaged)
    {
        GlutWindow::idle(damaged);
        if (damaged) {
            manager->getResourceManager()->updateResources();
        }
    }

    virtual bool mouseClick(button b, state s, modifier m, int x, int y)
    {
        mouseX = x;
        mouseY = y;
        rotate = (m & CTRL) != 0;
        return true;
    }

    virtual bool mouseMotion(int x, int y)
    {
        if (rotate) {
            controller->phi += (mouseX - x) / 500.0;
            controller->theta += (mouseY - y) / 500.0;
        } else {
            vec3d oldp = manager->getWorldCoordinates(mouseX, mouseY);
            vec3d p = manager->getWorldCoordinates(x, y);
            if (valid(oldp) && valid(p)) {
                controller->move(oldp, p);
            }
        }
        mouseX = x;
        mouseY = y;
        return true;
    }

    virtual bool mouseWheel(wheel b, modifier m, int x, int y)
    {
        if (b == WHEEL_DOWN) {
            controller->d *= 1.1;
        }
        if (b == WHEEL_UP) {
            controller->d /= 1.1;
        }
        return true;
    }

    virtual bool keyTyped(unsigned char c, modifier m, int x, int y)
    {
        if (c == 27) {
            ::exit(0);
        }
        return true;
    }

    virtual bool specialKey(key k, modifier m, int x, int y)
    {
        switch (k) {
        case KEY_F5:
            manager->getResourceManager()->updateResources();
            break;
        default:
            break;
        }
        return true;
    }

    bool valid(vec3d p) {
        return p.length() < controller->R * 1.1;
    }

    static static_ptr<Window> app;
};

static_ptr<Window> HelloWorld::app;

int main(int argc, char* argv[])
{
    initTerrainPlugin();
    atexit(Object::exit);
    HelloWorld::app = new HelloWorld();
    HelloWorld::app->start();
    return 0;
}
