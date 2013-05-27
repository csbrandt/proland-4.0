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
#include "proland/producer/TileProducer.h"
#include "proland/producer/GPUTileStorage.h"

using namespace ork;
using namespace proland;

ptr<FrameBuffer> createContourFramebuffer(ptr<Texture2D> contourTexture)
{
    int tileWidth = contourTexture->getWidth();
    ptr<FrameBuffer> frameBuffer(new FrameBuffer());
    frameBuffer->setReadBuffer(COLOR0);
    frameBuffer->setDrawBuffer(COLOR0);
    frameBuffer->setViewport(vec4<GLint>(0, 0, tileWidth, tileWidth));
    frameBuffer->setTextureBuffer(COLOR0, contourTexture, 0);
    frameBuffer->setPolygonMode(FILL, FILL);
    frameBuffer->setDepthTest(false);
    return frameBuffer;
}

static_ptr< Factory< ptr<Texture2D>, ptr<FrameBuffer> > > contourFramebufferFactory(
    new Factory< ptr<Texture2D>, ptr<FrameBuffer> >(createContourFramebuffer));

class ContourLineProducer : public TileProducer
{
public:
    ContourLineProducer(ptr<TileCache> cache, ptr<TileProducer> elevationTiles,
        ptr<Texture2D> contourTexture, ptr<Program> contourProgram) :
            TileProducer("ContourLineProducer", "CreateContourTile")
    {
        init(cache, elevationTiles, contourTexture, contourProgram);
    }

    virtual ~ContourLineProducer()
    {
    }

    virtual void getReferencedProducers(vector< ptr<TileProducer> > &producers) const
    {
        producers.push_back(elevationTiles);
    }

    virtual int getBorder()
    {
        return 2;
    }

    virtual bool hasTile(int level, int tx, int ty)
    {
        return elevationTiles->hasTile(level, tx, ty);
    }

protected:
    ContourLineProducer() : TileProducer("ContourLineProducer", "CreateContourTile")
    {
    }

    void init(ptr<TileCache> cache, ptr<TileProducer> elevationTiles,
        ptr<Texture2D> contourTexture, ptr<Program> contourProgram)
    {
        TileProducer::init(cache, true);
        this->frameBuffer = contourFramebufferFactory->get(contourTexture);
        this->elevationTiles = elevationTiles;
        this->contourTexture = contourTexture;
        this->contourProgram = contourProgram;
        this->elevationSamplerU = contourProgram->getUniformSampler("elevationSampler");
        this->elevationOSLU = contourProgram->getUniform4f("elevationOSL");
    }

    virtual void *getContext() const
    {
        return contourTexture.get();
    }

    virtual ptr<Task> startCreateTile(int level, int tx, int ty, unsigned int deadline, ptr<Task> task, ptr<TaskGraph> owner)
    {
        ptr<TaskGraph> result = owner == NULL ? createTaskGraph(task) : owner;
        TileCache::Tile *t = elevationTiles->getTile(level, tx, ty, deadline);
        assert(t != NULL);
        result->addTask(t->task);
        result->addDependency(task, t->task);
        return result;
    }

    virtual bool doCreateTile(int level, int tx, int ty, TileStorage::Slot *data)
    {
        if (Logger::DEBUG_LOGGER != NULL) {
            ostringstream oss;
            oss << "Contour tile " << getId() << " " << level << " " << tx << " " << ty;
            Logger::DEBUG_LOGGER->log("ORTHO", oss.str());
        }

        GPUTileStorage::GPUSlot *gpuData = dynamic_cast<GPUTileStorage::GPUSlot*>(data);
        assert(gpuData != NULL);

        // don't forget this!
        getCache()->getStorage().cast<GPUTileStorage> ()->notifyChange(gpuData);

        int tileWidth = data->getOwner()->getTileSize();

        ptr<Texture> storage = getCache()->getStorage().cast<GPUTileStorage>()->getTexture(0);

        TileCache::Tile *t = elevationTiles->findTile(level, tx, ty);
        assert(t != NULL);
        GPUTileStorage::GPUSlot *elevationGpuData = dynamic_cast<GPUTileStorage::GPUSlot*>(t->getData());
        assert(elevationGpuData != NULL);

        int zTileWidth = elevationGpuData->getWidth();
        float scale = ((zTileWidth - 5.0) / zTileWidth) * (tileWidth / (tileWidth - 4.0));
        float offset = (1.0 - scale) / 2.0;

        elevationSamplerU->set(elevationGpuData->t);
        elevationOSLU->set(vec4f(offset, offset, scale, elevationGpuData->l));

        frameBuffer->drawQuad(contourProgram);
        gpuData->copyPixels(frameBuffer, 0, 0, tileWidth, tileWidth);
        return true;
    }

    virtual void stopCreateTile(int level, int tx, int ty)
    {
        TileCache::Tile *t = elevationTiles->findTile(level, tx, ty);
        assert(t != NULL);
        elevationTiles->putTile(t);
    }

    virtual void swap(ptr<ContourLineProducer> p)
    {
        TileProducer::swap(p);
        std::swap(frameBuffer, p->frameBuffer);
        std::swap(elevationTiles, p->elevationTiles);
        std::swap(contourTexture, p->contourTexture);
        std::swap(contourProgram, p->contourProgram);
        std::swap(elevationSamplerU, p->elevationSamplerU);
        std::swap(elevationOSLU, p->elevationOSLU);
    }

    ptr<Program> contourProgram;

private:
    ptr<FrameBuffer> frameBuffer;

    ptr<TileProducer> elevationTiles;

    ptr<Texture2D> contourTexture;

    ptr<UniformSampler> elevationSamplerU;

    ptr<Uniform4f> elevationOSLU;
};

class ContourLineProducerResource : public ResourceTemplate<50, ContourLineProducer>
{
public:
    ContourLineProducerResource(ptr<ResourceManager> manager, const string &name, ptr<ResourceDescriptor> desc, const TiXmlElement *e = NULL) :
        ResourceTemplate<50, ContourLineProducer>(manager, name, desc)
    {
        e = e == NULL ? desc->descriptor : e;
        ptr<TileCache> cache;
        ptr<TileProducer> elevations;
        ptr<Texture2D> contourTexture;
        ptr<Program> contourProgram;
        checkParameters(desc, e, "name,cache,elevations,contourProg,");
        cache = manager->loadResource(getParameter(desc, e, "cache")).cast<TileCache>();
        elevations = manager->loadResource(getParameter(desc, e, "elevations")).cast<TileProducer>();
        string contour = "contourShader;";
        if (e->Attribute("contourProg") != NULL) {
            contour = getParameter(desc, e, "contourProg");
        }
        contourProgram = manager->loadResource(contour).cast<Program>();

        int tileSize = cache->getStorage()->getTileSize();
        const char* format = cache->getStorage().cast<GPUTileStorage>()->getTexture(0)->getInternalFormatName();

        ostringstream contourTex;
        contourTex << "renderbuffer-" << tileSize << "-" << format;
        contourTexture = manager->loadResource(contourTex.str()).cast<Texture2D>();

        init(cache, elevations, contourTexture, contourProgram);
    }

    virtual bool prepareUpdate()
    {
        if (dynamic_cast<Resource*>(contourProgram.get())->changed()) {
            invalidateTiles();
        }
        return ResourceTemplate<50, ContourLineProducer>::prepareUpdate();
    }
};

extern const char contourProducer[] = "contourProducer";

static ResourceFactory::Type<contourProducer, ContourLineProducerResource> ContourLineProducerType;

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
