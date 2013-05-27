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
#include "ork/resource/ResourceTemplate.h"
#include "ork/resource/XMLResourceLoader.h"
#include "ork/scenegraph/SceneManager.h"
#include "ork/scenegraph/ShowLogTask.h"
#include "ork/ui/GlutWindow.h"

#include "proland/ui/BasicViewHandler.h"
#include "proland/ui/twbar/TweakBarManager.h"
#include "proland/util/TerrainViewController.h"

#include "proland/TerrainPlugin.h"
#include "proland/producer/TileLayer.h"
#include "proland/producer/GPUTileStorage.h"

using namespace ork;
using namespace proland;

class DebugOrthoLayer : public TileLayer
{
public:
    DebugOrthoLayer(ptr<Font> f, ptr<Program> p, float fontHeight) : TileLayer("DebugOrthoLayer")
    {
        TileLayer::init(false);
        init(f, p, fontHeight);
    }

    virtual ~DebugOrthoLayer()
    {
    }

    virtual bool doCreateTile(int level, int tx, int ty, TileStorage::Slot *data)
    {
        if (Logger::DEBUG_LOGGER != NULL) {
            ostringstream oss;
            oss << "Debug tile " << getProducerId() << " " << level << " " << tx << " " << ty;
            Logger::DEBUG_LOGGER->log("ORTHO", oss.str());
        }

        ostringstream os;
        os << dynamic_cast<GPUTileStorage::GPUSlot*>(data)->l;

        ptr<FrameBuffer> fb = SceneManager::getCurrentFrameBuffer();
        vec4f vp = fb->getViewport().cast<float>();
        fb->setBlend(true, ADD, SRC_ALPHA, ONE_MINUS_SRC_ALPHA, ADD, ZERO, ONE);
        fontMesh->clear();
        font->addLine(vp, 2.0f, 2.0f, os.str(), fontHeight, 0xFF0000FF, fontMesh);
        fontU->set(font->getImage());
        fb->draw(fontProgram, *fontMesh);
        fb->setBlend(false);
        return true;
    }

protected:
    DebugOrthoLayer() : TileLayer("DebugOrthoLayer")
    {
    }

    void init(ptr<Font> f, ptr<Program> p, float fontHeight)
    {
        this->font = f;
        this->fontProgram = p;
        this->fontHeight = fontHeight;
        this->fontU = p->getUniformSampler("font");
        if (fontMesh == NULL) {
            fontMesh = new Mesh<Font::Vertex, unsigned int>(TRIANGLES, CPU);
            fontMesh->addAttributeType(0, 4, A16F, false);
            fontMesh->addAttributeType(1, 4, A8UI, true);
        }
    }

    virtual void swap(ptr<DebugOrthoLayer> p)
    {
        TileLayer::swap(p);
        std::swap(font, p->font);
        std::swap(fontProgram, p->fontProgram);
        std::swap(fontHeight, p->fontHeight);
    }

private:
    ptr<Font> font;

    ptr<Program> fontProgram;

    float fontHeight;

    ptr<UniformSampler> fontU;

    static static_ptr< Mesh<Font::Vertex, unsigned int> > fontMesh;
};

static_ptr< Mesh<Font::Vertex, unsigned int> > DebugOrthoLayer::fontMesh;

class DebugOrthoLayerResource : public ResourceTemplate<40, DebugOrthoLayer>
{
public:
    DebugOrthoLayerResource(ptr<ResourceManager> manager, const string &name, ptr<ResourceDescriptor> desc,
            const TiXmlElement *e = NULL) :
        ResourceTemplate<40, DebugOrthoLayer> (manager, name, desc)
    {
        e = e == NULL ? desc->descriptor : e;
        checkParameters(desc, e, "name,font,fontSize,fontProgram,");
        string fontName = "defaultFont";
        if (e->Attribute("font") != NULL) {
            fontName = Resource::getParameter(desc, e, "font");
        }
        ptr<Font> f = manager->loadResource(fontName).cast<Font>();

        float size = f->getTileHeight();
        if (e->Attribute("fontSize") != NULL) {
            getFloatParameter(desc, e, "fontSize", &size);
        }

        string fontProgram = "text;";
        if (e->Attribute("fontProgram") != NULL) {
            fontProgram = string(e->Attribute("fontProgram"));
        }
        ptr<Program> p = manager->loadResource(fontProgram).cast<Program>();

        init(f, p, size);
    }

    virtual bool prepareUpdate()
    {
        oldValue = NULL;
        newDesc = NULL;
        return true;
    }
};

extern const char debugOrthoLayer[] = "debugOrthoLayer";

static ResourceFactory::Type<debugOrthoLayer, DebugOrthoLayerResource> DebugOrthoLayerType;

class HelloWorld : public GlutWindow, public ViewManager
{
public:
    ptr<SceneManager> manager;
    ptr<TerrainViewController> controller;
    ptr<BasicViewHandler> view;
    ptr<EventHandler> ui;

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

        controller = new TerrainViewController(manager->getCameraNode(), 50000.0);
        view = new BasicViewHandler(true, this, NULL);

        ptr<TweakBarManager> tb = resManager->loadResource("ui").cast<TweakBarManager>();
        tb->setNext(view);
        ui = tb;
    }

    virtual ~HelloWorld()
    {
    }

    virtual void redisplay(double t, double dt)
    {
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
        return manager;
    }

    virtual ptr<TerrainViewController> getViewController()
    {
        return controller;
    }

    virtual vec3d getWorldCoordinates(int x, int y)
    {
        vec3d p = manager->getWorldCoordinates(x, y);
        if (abs(p.x) > 100000.0 || abs(p.y) > 100000.0 || abs(p.z) > 100000.0) {
            p = vec3d(NAN, NAN, NAN);
        }
        return p;
    }

    void updateResources()
    {
        BasicViewHandler::Position p;
        view->getPosition(p);
        manager->getResourceManager()->updateResources();
        controller->setNode(manager->getCameraNode());
        view->setPosition(p);
    }

    static void exit() {
        app.cast<HelloWorld>()->manager->getResourceManager()->close();
        Object::exit();
    }

    static static_ptr<Window> app;
};

static_ptr<Window> HelloWorld::app;

int main(int argc, char* argv[])
{
    initTerrainPlugin();
    atexit(HelloWorld::exit);
    HelloWorld::app = new HelloWorld();
    HelloWorld::app->start();
    return 0;
}
