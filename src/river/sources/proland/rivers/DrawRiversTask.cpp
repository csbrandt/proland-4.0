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

#include "proland/rivers/DrawRiversTask.h"

//#include <GL/glew.h>

#include "ork/core/Logger.h"
#include "ork/resource/ResourceTemplate.h"
#include "ork/render/FrameBuffer.h"
#include "ork/render/Texture2D.h"
#include "ork/scenegraph/ShowInfoTask.h"

#include "proland/producer/ObjectTileStorage.h"
#include "proland/rivers/HydroFlowTile.h"
#include "proland/particles/LifeCycleParticleLayer.h"
#include "proland/particles/RandomParticleLayer.h"
#include "proland/particles/screen/ParticleGrid.h"

using namespace ork;

namespace proland
{

DrawRiversTask::vecParticle::vecParticle()
{
}

DrawRiversTask::vecParticle::vecParticle(float x, float y, float tx, float ty, float ox, float oy,
        float i, float r, float id) :
    x(x), y(y), tx(tx), ty(ty), ox(ox), oy(oy), i(i), r(r), id(id)
{
}

DrawRiversTask::vecParticle::vecParticle(ParticleProducer *producer, ParticleStorage::Particle *p)
{
    ScreenParticleLayer::ScreenParticle *s = producer->getLayer<ScreenParticleLayer>()->getScreenParticle(p);
    TerrainParticleLayer::TerrainParticle *t = producer->getLayer<TerrainParticleLayer>()->getTerrainParticle(p);
    RandomParticleLayer::RandomParticle *rp = producer->getLayer<RandomParticleLayer>()->getRandomParticle(p);
    x = s->screenPos.x;
    y = s->screenPos.y;
    tx = t->terrainPos.x;
    ty = t->terrainPos.y;
    ox = rp->randomPos.x;
    oy = rp->randomPos.y;
    i = producer->getLayer<LifeCycleParticleLayer>()->getIntensity(p);
    r = producer->getLayer<ScreenParticleLayer>()->getParticleRadius();
    id = t->terrainId;
}

/**
 * Callback method for retrieving the correct attributes in the sprites parameters' table.
 */
static bool getRiversParticlesParams(ParticleProducer *producer, ParticleStorage::Particle *p, float *params)
{
    ScreenParticleLayer::ScreenParticle *s = producer->getLayer<ScreenParticleLayer>()->getScreenParticle(p);
    TerrainParticleLayer::TerrainParticle *t = producer->getLayer<TerrainParticleLayer>()->getTerrainParticle(p);
    RandomParticleLayer::RandomParticle *r = producer->getLayer<RandomParticleLayer>()->getRandomParticle(p);
    params[0] = s->screenPos.x;
    params[1] = s->screenPos.y;
    params[2] = t->terrainPos.x;
    params[3] = t->terrainPos.y;
    params[4] = r->randomPos.x;
    params[5] = r->randomPos.y;
    params[6] = producer->getLayer<LifeCycleParticleLayer>()->getIntensity(p);
    params[7] = producer->getLayer<ScreenParticleLayer>()->getParticleRadius();
    params[8] = t->terrainId;
    return t->terrainId >= 0 && t->status > FlowTile::UNKNOWN && t->status <= FlowTile::NEAR;
}

DrawRiversTask::DrawRiversTask() : AbstractTask("DrawRiversTask")
{
}

DrawRiversTask::DrawRiversTask(ptr<Program> renderTexProg, ptr<Program> particlesProg, ptr<ParticleProducer> particles, float timeStep, bool drawParticles, ptr<WaveTile> tex, ptr<WaveTile> bedTex, float riverDepth, float waveSlopeFactor, bool useOffscreenDepth) :
    AbstractTask("DrawRiversTask")
{
    init(renderTexProg, particlesProg, particles, timeStep, drawParticles, tex, bedTex, riverDepth, waveSlopeFactor, useOffscreenDepth);
}

void DrawRiversTask::init(ptr<Program> renderTexProg, ptr<Program> particlesProg, ptr<ParticleProducer> particles, float timeStep, bool drawParticles, ptr<WaveTile> tex, ptr<WaveTile> bedTex, float riverDepth, float waveSlopeFactor, bool useOffscreenDepth)
{
    //glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
    this->renderTexProg = renderTexProg;
    this->particlesProg = particlesProg;
    this->particles = particles;
    this->timeStep = timeStep;
    this->drawParticles = drawParticles;
    this->drawVelocities = drawParticles;
    this->drawMesh = ADVECTED;
    this->drawGrid = false;
    this->particleGrid = NULL;
    this->riverTex = tex;
    this->bedTex = bedTex;
    this->sunEffects = true;
    this->riverDepth = riverDepth;
    this->waveSlopeFactor = waveSlopeFactor;

    this->initialized = false;

    this->mesh = new Mesh<vec3f, unsigned int>(POINTS, GPU_DYNAMIC);
    this->mesh->addAttributeType(0, 3, A32F, false);

    if (renderTexProg != NULL) {
        this->particleMesh = new Mesh<vecParticle, unsigned int>(POINTS, GPU_DYNAMIC);
        this->particleMesh->addAttributeType(0, 2, A32F, false); // sPos
        this->particleMesh->addAttributeType(1, 2, A32F, false); // wPos
        this->particleMesh->addAttributeType(2, 2, A32F, false); // oPos
        this->particleMesh->addAttributeType(3, 1, A32F, false); // intensity
        this->particleMesh->addAttributeType(4, 1, A32F, false); // radius
        this->particleMesh->addAttributeType(5, 1, A32F, false); // terrainId
    }
    this->advectedTex = NULL;

    this->spTable = NULL;
    this->usGrid = NULL;
    this->depthBuffer = NULL;
    this->useOffscreenDepth = useOffscreenDepth;

    if (particlesProg != NULL) {
        particleSizeU = particlesProg->getUniform1f("particleSize");
        sizeU = particlesProg->getUniform2f("size");
    }

    if (renderTexProg != NULL) {
        preRenderScreenSizeU = renderTexProg->getUniform2f("river.screenSize");
        depthBufferU = renderTexProg->getUniformSampler("river.depthSampler");
    } else {
        drawModeU = NULL;
    }
}

DrawRiversTask::~DrawRiversTask()
{
    terrainInfos.clear();
    if (particleGrid != NULL) {
        delete particleGrid;
    }
}

void DrawRiversTask::setParticleRadius(float radius)
{
    if (screenLayer != NULL) {
        screenLayer->setParticleRadius(radius);
    }
    if (particleGrid != NULL) {
        particleGrid->setParticleRadius(3.0f * radius);
    }
}

void DrawRiversTask::setSlipParameter(int id, float slip)
{
    if (id != -1) {
        assert((int) terrainInfos.size() >= id + 1);
        terrainInfos[id].flows.cast<HydroFlowProducer>()->setSlipParameter(slip);
    } else {
        for (int i = 0; i < (int) terrainInfos.size(); i++) {
            terrainInfos[i].flows.cast<HydroFlowProducer>()->setSlipParameter(slip);
        }
    }
}

void DrawRiversTask::setPotentialDelta(int id, float potential)
{
    if (id != -1) {
        assert((int) terrainInfos.size() >= id + 1);
        terrainInfos[id].flows.cast<HydroFlowProducer>()->setPotentialDelta(potential);
    } else {
        for (int i = 0; i < (int) terrainInfos.size(); i++) {
            terrainInfos[i].flows.cast<HydroFlowProducer>()->setPotentialDelta(potential);
        }
    }
}

void DrawRiversTask::setTimeStep(float timeStep)
{
    this->timeStep = timeStep;
}

void DrawRiversTask::displayGrid(bool display)
{
    drawGrid = display;
}

void DrawRiversTask::displayParticles(bool display)
{
    drawParticles = display;
}

void DrawRiversTask::displayVelocities(bool display)
{
    drawVelocities = display;
}

void DrawRiversTask::displaySunEffects(bool display)
{
    sunEffects = display;
}

void DrawRiversTask::setRiverDepth(float depth)
{
    riverDepth = depth;
}

void DrawRiversTask::setWaveSlopeFactor(float slopeFactor)
{
    waveSlopeFactor = slopeFactor;
}

void DrawRiversTask::setWaveLength(float length)
{
    riverTex->setWaveLength(length);
}

void DrawRiversTask::setBedLength(float length)
{
    if (bedTex != NULL) {
        bedTex->setWaveLength(length);
    }
}

void DrawRiversTask::setMeshDisplayType(MeshDisplayType type)
{
    drawMesh = type;
}

ptr<ParticleProducer> DrawRiversTask::getParticles() const
{
    return particles;
}

float DrawRiversTask::getParticleRadius() const
{
    return screenLayer->getParticleRadius();
}

float DrawRiversTask::getSlipParameter(int id) const
{
    if (id == -1) {
        id = 0;
    }
    assert((int) terrainInfos.size() >= id + 1);
    return terrainInfos[id].flows.cast<HydroFlowProducer>()->getSlipParameter();
}

float DrawRiversTask::getPotentialDelta(int id) const
{
    if (id == -1) {
        id = 0;
    }
    assert((int) terrainInfos.size() >= id + 1);
    return terrainInfos[id].flows.cast<HydroFlowProducer>()->getPotentialDelta();
}

float DrawRiversTask::getTimeStep() const
{
    return timeStep;
}

bool DrawRiversTask::displayGrid() const
{
    return drawGrid;
}

bool DrawRiversTask::displayParticles() const
{
    return drawParticles;
}

bool DrawRiversTask::displayVelocities() const
{
    return drawVelocities;
}

bool DrawRiversTask::displaySunEffects() const
{
    return sunEffects;
}

float DrawRiversTask::getRiverDepth() const
{
    return riverDepth;
}

float DrawRiversTask::getWaveSlopeFactor() const
{
    return waveSlopeFactor;
}

float DrawRiversTask::getWaveLength() const
{
    return riverTex->getWaveLength();
}

float DrawRiversTask::getBedLength() const
{
    if (bedTex != NULL) {
        return bedTex->getWaveLength();
    }
    return 0.f;
}

DrawRiversTask::MeshDisplayType DrawRiversTask::getMeshDisplayType() const
{
    return drawMesh;
}

ptr<Task> DrawRiversTask::getTask(ptr<Object> context)
{
    ptr<SceneNode> n = context.cast<Method>()->getOwner();

    ptr<FrameBuffer> old = SceneManager::getCurrentFrameBuffer();

    vec4<GLint> vp = old->getViewport();
    if (advectedTex == NULL || advectedTex->getWidth() != vp.z || advectedTex->getHeight() != vp.w) {
        advectedTex = new Texture2D(vp.z, vp.w, RGBA32F, RGBA, FLOAT,
            Texture::Parameters().wrapT(REPEAT).wrapS(REPEAT).min(LINEAR).mag(LINEAR),
            Buffer::Parameters(), CPUBuffer(NULL));
        if (frameBuffer == NULL) {
            frameBuffer = new FrameBuffer();
        }

        frameBuffer->setReadBuffer(COLOR0);
        frameBuffer->setDrawBuffer(COLOR0);
        frameBuffer->setViewport(vec4<GLint>(0, 0, vp.z, vp.w));
        frameBuffer->setTextureBuffer(COLOR0, advectedTex, 0);
        frameBuffer->setStencilMask(0, 0);
        frameBuffer->setBlend(true, ADD, ONE, ONE, ADD, ONE, ONE);
        frameBuffer->clear(true, false, false);
    }

    if (! initialized) {
        screenLayer = particles->getLayer<ScreenParticleLayer>();
        lifeCycleLayer = particles->getLayer<LifeCycleParticleLayer>();
        terrainLayer = particles->getLayer<TerrainParticleLayer>();

        if (particleGrid != NULL) {
            delete particleGrid;
        }
        particleGrid = new ParticleGrid(screenLayer->getParticleRadius() * 3.0f, 16, 8.f * 3.f / 4.f ); //rfactor = 8 times the size of the neighborhood grid

        scene = context.cast<Method>()->getOwner()->getOwner().get();
        screenLayer->setSceneManager(scene);

        map<ptr<TileProducer>, TerrainParticleLayer::TerrainInfo*> infos = terrainLayer->getTerrainInfos();
        for (map<ptr<TileProducer>, TerrainParticleLayer::TerrainInfo*>::iterator i = infos.begin(); i != infos.end(); i++) {
            ptr<SceneNode> tn = i->second->node;

            TerrainInfo ti;
            ti.id = terrainInfos.size();
            ti.tn = tn.get();
            ti.t = i->second->terrain.get();
            ti.flows = i->first;
            ostringstream n;
            n << "terrainInfos[" << ti.id << "]";
            ti.name = n.str();
            if (renderTexProg != NULL) {
                ti.screenToLocalU = renderTexProg->getUniformMatrix4f(ti.name + ".screenToLocal");
            } else {
                ti.screenToLocalU = NULL;
            }

            terrainInfos.push_back(ti);
        }

        initialized = true;
    }

    ptr<Impl> result = new Impl(this);

    return result;
}

void DrawRiversTask::doDrawParticles(ptr<ParticleProducer> pp)
{
    ptr<FrameBuffer> fb = SceneManager::getCurrentFrameBuffer();
    fb->setBlend(true, ADD, ONE, ONE, ADD, ONE, ONE);
    fb->setColorMask(true, true, true, false);
    fb->setDepthMask(false);

    mesh->clear();
//    bool disablePS = false;
//    if (! glIsEnabled(GL_POINT_SMOOTH)) {
//        disablePS = true;
//        glEnable(GL_POINT_SMOOTH);
//    }

    if (particlesProg != NULL) {
        particleSizeU->set(screenLayer->getParticleRadius());
        sizeU->set(vec2f(fb->getViewport().z, fb->getViewport().w));

        ptr<ParticleStorage> storage = pp->getStorage();
        vector<ParticleStorage::Particle*>::iterator i = storage->getParticles();
        vector<ParticleStorage::Particle*>::iterator end = storage->end();
        while (i != end) {
            ParticleStorage::Particle* p = *i;
            ScreenParticleLayer::ScreenParticle *s = screenLayer->getScreenParticle(p);
            TerrainParticleLayer::TerrainParticle *t = terrainLayer->getTerrainParticle(p);
            float intensity = t->status + lifeCycleLayer->getIntensity(p) * 0.99f;
            if (t->status == FlowTile::NEAR) {
                intensity = (float)FlowTile::NEAR + 0.99f;
            }
            mesh->addVertex(vec3f(s->screenPos.x, s->screenPos.y, intensity));
            ++i;
        }
        fb->draw(particlesProg, *mesh);
    }
//    if (disablePS) {
//        glDisable(GL_POINT_SMOOTH);
//    }

    fb->setBlend(false);
    fb->setColorMask(true, true, true, true);
    fb->setDepthMask(true);
}

void DrawRiversTask::drawRivers()
{
    if (Logger::DEBUG_LOGGER != NULL) {
        Logger::DEBUG_LOGGER->log("RIVERS", "Drawing Rivers");
    }
    if ((int) terrainInfos.size() == 0) {
        return;
    }

    ptr<Program> prog = SceneManager::getCurrentProgram();
    ptr<FrameBuffer> fb = SceneManager::getCurrentFrameBuffer();

    if (drawModeU == NULL) {
        drawModeU = prog->getUniform1f("river.drawMode");
        displayGridU = prog->getUniform1f("river.displayGrid");
        sunEffectsU = prog->getUniform1f("river.enableSunEffects");
        waveSlopeFactorU = prog->getUniform1f("river.waveSlopeFactor");
        riverDepthU = prog->getUniform1f("river.depth");
        useBedTexU = prog->getUniform1f("river.useBedTex");
        screenSizeU = prog->getUniform2f("river.screenSize");
        gridSizeU = prog->getUniform2f("river.gridSize");
        uniformSpriteGridU = prog->getUniformSampler("river.spriteTable");
        spriteParamTableU = prog->getUniformSampler("river.spriteParamTable");
        riverTexU = prog->getUniformSampler("river.riverTex");
    }

    vec4<GLint> vp = fb->getViewport();
    if (vp.z == 0 || vp.w == 0) {
        return;
    }

    if ((drawMesh > NONE || drawParticles)) {
        if (drawParticles || drawMesh < NON_ADVECTED) { // updating particles and particle Grid.

            particles->updateParticles(timeStep * scene->getElapsedTime());
            if (renderTexProg == NULL || drawMesh != PRE_ADVECTED) {
                particleGrid->setViewport(box2i(vp.x, vp.x + vp.z, vp.y, vp.y + vp.w));
                particleGrid->clear();

                ptr<ParticleStorage> storage = particles->getStorage();
                vector<ParticleStorage::Particle*>::iterator i = storage->getParticles();
                vector<ParticleStorage::Particle*>::iterator end = storage->end();
                while (i != end) {
                    ParticleStorage::Particle* p = *i;
                    ScreenParticleLayer::ScreenParticle *s = screenLayer->getScreenParticle(p);
                    TerrainParticleLayer::TerrainParticle *t = terrainLayer->getTerrainParticle(p);
                    if (t->status == FlowTile::INSIDE || t->status == FlowTile::LEAVING) {
                        particleGrid->addParticle(s, lifeCycleLayer->getIntensity(p));
                    }
                    ++i;
                }
            }
        }
        riverTex->timeStep(timeStep);
        if(bedTex != NULL) {
            bedTex->timeStep(timeStep);
        }
        char counter[30];
        sprintf(counter, "%d / %d particles", particles->getStorage()->getParticlesCount(), particles->getStorage()->getCapacity());
        ShowInfoTask::setInfo("PARTICLES", counter);
    }
    if (drawMesh > NONE) {
        if (renderTexProg != NULL && drawMesh == PRE_ADVECTED) {
            if (!useOffscreenDepth) {
                if (depthBuffer == NULL || depthBuffer->getWidth() != vp.z - vp.x || depthBuffer->getHeight() != vp.w - vp.y) {
                    depthBuffer = new Texture2D(vp.z - vp.x, vp.w - vp.y, DEPTH_COMPONENT32F, DEPTH_COMPONENT, FLOAT,
                        Texture::Parameters().wrapS(CLAMP_TO_EDGE).wrapT(CLAMP_TO_EDGE).min(NEAREST).mag(NEAREST),
                        Buffer::Parameters(), CPUBuffer(NULL));
                }
                fb->copyPixels(0, 0, 0, 0, vp.z - vp.x, vp.w - vp.y, *depthBuffer, 0);
            }

            if (!useOffscreenDepth) {
                depthBufferU->set(depthBuffer);
            }

            preRenderScreenSizeU->set(vec2f(vp.z - vp.x, vp.w - vp.y));
            riverTex->updateUniform(renderTexProg);

            for (unsigned int i = 0; i < terrainInfos.size(); ++i) {
                terrainInfos[i].screenToLocalU->setMatrix(terrainInfos[i].tn->getLocalToScreen().inverse().cast<float>());
            }
            frameBuffer->clear(true, false, false);
            particleMesh->clear();
            ptr<ParticleStorage> storage = particles->getStorage();
            vector<ParticleStorage::Particle*>::iterator i = storage->getParticles();
            vector<ParticleStorage::Particle*>::iterator end = storage->end();
            while (i != end) {
                ParticleStorage::Particle* p = *i;
                TerrainParticleLayer::TerrainParticle *t = terrainLayer->getTerrainParticle(p);
                if (t->status == FlowTile::INSIDE || t->status == FlowTile::LEAVING) {
                    particleMesh->addVertex(vecParticle(particles.get(), p));
                }
                ++i;
            }
            frameBuffer->draw(renderTexProg, *particleMesh);
        } else {
            riverTex->updateUniform(prog);
            spTable = particles->copyToTexture(spTable, 9, getRiversParticlesParams);
            int nLayers;
            usGrid = particleGrid->copyToTexture(screenLayer, usGrid, nLayers);
            assert(spTable != NULL);
            assert(usGrid != NULL);
            uniformSpriteGridU->set(usGrid);
            spriteParamTableU->set(spTable);
        }
        if (drawModeU != NULL) {
            drawModeU->set(drawMesh);
        }
        if (displayGridU != NULL) {
            displayGridU->set(drawGrid);
        }
        gridSizeU->set(particleGrid->getGridSize().cast<float>());
        screenSizeU->set(vec2f(vp.z, vp.w));
        if (sunEffectsU != NULL) {
            sunEffectsU->set(sunEffects);
        }
        if (waveSlopeFactorU != NULL) {
            waveSlopeFactorU->set(waveSlopeFactor);
        }
        if (riverDepthU != NULL) {
            riverDepthU->set(riverDepth);
        }
        if (riverTexU != NULL) {
            riverTexU->set(advectedTex);
        }
        if (useBedTexU != NULL) {
            if (bedTex != NULL) {
                bedTex->updateUniform(prog);
                useBedTexU->set(1.0f);
            } else {
                useBedTexU->set(0.0f);
            }
        }
    }

    if (drawParticles) {
        doDrawParticles(particles);
    }
}

DrawRiversTask::Impl::Impl(DrawRiversTask *owner) :
    Task("DrawRivers", true, 0), owner(owner)
{
}

DrawRiversTask::Impl::~Impl()
{
}

bool DrawRiversTask::Impl::run()
{
    owner->drawRivers();
    return true;
}

void DrawRiversTask::swap(ptr<DrawRiversTask> t)
{
    std::swap(timeStep, t->timeStep);
    std::swap(drawParticles, t->drawParticles);
    std::swap(sunEffects, t->sunEffects);
    std::swap(drawMesh, t->drawMesh);
    std::swap(riverTex, t->riverTex);
    std::swap(bedTex, t->bedTex);
    std::swap(mesh, t->mesh);
    std::swap(terrainInfos, t->terrainInfos);
    std::swap(initialized, t->initialized);
    std::swap(waveSlopeFactor, t->waveSlopeFactor);
    std::swap(terrainLayer, t->terrainLayer);
    std::swap(screenLayer, t->screenLayer);
    std::swap(lifeCycleLayer, t->lifeCycleLayer);
    std::swap(spTable, t->spTable);
    std::swap(renderTexProg, t->renderTexProg);
    std::swap(particlesProg, t->particlesProg);
    std::swap(usGrid, t->usGrid);
    std::swap(drawModeU, t->drawModeU);
    std::swap(displayGridU, t->displayGridU);
    std::swap(sunEffectsU, t->sunEffectsU);
    std::swap(waveSlopeFactorU, t->waveSlopeFactorU);
    std::swap(riverDepthU, t->riverDepthU);
    std::swap(useBedTexU, t->useBedTexU);
    std::swap(screenSizeU, t->screenSizeU);
    std::swap(gridSizeU, t->gridSizeU);
    std::swap(uniformSpriteGridU, t->uniformSpriteGridU);
    std::swap(spriteParamTableU, t->spriteParamTableU);
}

class DrawRiversTaskResource : public ResourceTemplate<50, DrawRiversTask>
{
public:
    DrawRiversTaskResource(ptr<ResourceManager> manager, const string &name, ptr<ResourceDescriptor> desc, const TiXmlElement *e = NULL) :
        ResourceTemplate<50, DrawRiversTask>(manager, name, desc)
    {
        e = e == NULL ? desc->descriptor : e;

        ptr<ParticleProducer> particles;
        bool drawParticles = false;
        float timeStep = 1.0f;
        ptr<WaveTile> tex = NULL;
        ptr<WaveTile> bedTex = NULL;
        float waveSlopeFactor = 1.0f;
        float waveLength = 1.0f;
        float riverDepth = 1.0;
        checkParameters(desc, e, "name,renderTexProg,particlesProg,particles,drawParticles,timeStep,texture,bedTexture,waveSlopeFactor,waveLength,riverDepth,useOffscreenDepthBuffer,");

        ptr<Program> particlesProg = NULL;
        if (e->Attribute("particlesProg") != NULL) {
            particlesProg = manager->loadResource(getParameter(desc, e, "particlesProg")).cast<Program>();
        }

        ptr<Program> renderTexProg = NULL;
        if (e->Attribute("renderTexProg") != NULL) {
            renderTexProg = manager->loadResource(getParameter(desc, e, "renderTexProg")).cast<Program>();
        }

        if (e->Attribute("drawParticles") != NULL) {
            drawParticles = strcmp(e->Attribute("drawParticles"), "true") == 0;
        }

        if (e->Attribute("timeStep") != NULL) {
            getFloatParameter(desc, e, "timeStep", &timeStep);
        }
        if (e->Attribute("waveSlopeFactor") != NULL) {
            getFloatParameter(desc, e, "waveSlopeFactor", &waveSlopeFactor);
        }

        tex = manager->loadResource(getParameter(desc, e, "texture")).cast<WaveTile>();
        assert(tex != NULL);
        if (e->Attribute("bedTexture") != NULL) {
            bedTex = manager->loadResource(getParameter(desc, e, "bedTexture")).cast<WaveTile>();
            if (e->Attribute("riverDepth") != NULL) {
                getFloatParameter(desc, e, "riverDepth", &riverDepth);
            }
        }

        if (e->Attribute("waveLength") != NULL) {
            getFloatParameter(desc, e, "waveLength", &waveLength);
            tex->setWaveLength(waveLength);
        }

        bool useOffscreenDepth = false;
        if (e->Attribute("useOffscreenDepthBuffer") != NULL) {
            useOffscreenDepth = strcmp(e->Attribute("useOffscreenDepthBuffer"), "true") == 0;
        }

        particles = manager->loadResource(getParameter(desc, e, "particles")).cast<ParticleProducer>();
        assert(particles != NULL);
        init(renderTexProg, particlesProg, particles, timeStep, drawParticles,
             tex, bedTex, riverDepth, waveSlopeFactor, useOffscreenDepth);
    }
};


extern const char drawRivers[] = "drawRivers";

static ResourceFactory::Type<drawRivers, DrawRiversTaskResource> DrawRiversTaskType;

}
