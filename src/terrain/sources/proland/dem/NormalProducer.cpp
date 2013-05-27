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

#include "proland/dem/NormalProducer.h"

#include <sstream>

#include "ork/core/Logger.h"
#include "ork/resource/ResourceTemplate.h"
#include "ork/render/Program.h"
#include "ork/render/CPUBuffer.h"
#include "ork/scenegraph/SceneManager.h"
#include "ork/scenegraph/SetTargetTask.h"
#include "ork/taskgraph/TaskGraph.h"

#include "proland/producer/CPUTileStorage.h"
#include "proland/producer/GPUTileStorage.h"

using namespace std;
using namespace ork;

namespace proland
{

ptr<FrameBuffer> createNormalFramebuffer(ptr<Texture2D> normalTexture)
{
    int tileWidth = normalTexture->getWidth();
    ptr<FrameBuffer> frameBuffer(new FrameBuffer());
    frameBuffer->setReadBuffer(COLOR0);
    frameBuffer->setDrawBuffer(COLOR0);
    frameBuffer->setViewport(vec4<GLint>(0, 0, tileWidth, tileWidth));
    frameBuffer->setTextureBuffer(COLOR0, normalTexture, 0);
    frameBuffer->setPolygonMode(FILL, FILL);
    frameBuffer->setDepthTest(false);
    frameBuffer->setBlend(false);
    frameBuffer->setColorMask(true, true, true, true);
    frameBuffer->setDepthMask(true);
    frameBuffer->setStencilMask(true, true);
    return frameBuffer;
}

static_ptr< Factory< ptr<Texture2D>, ptr<FrameBuffer> > > normalFramebufferFactory(
    new Factory< ptr<Texture2D>, ptr<FrameBuffer> >(createNormalFramebuffer));

static_ptr<FrameBuffer> NormalProducer::old;

NormalProducer::NormalProducer(ptr<TileCache> cache, ptr<TileProducer> elevationTiles,
    ptr<Texture2D> normalTexture, ptr<Program> normals, int gridSize, bool deform) : TileProducer("NormalProducer", "CreateNormalTile")
{
    init(cache, elevationTiles, normalTexture, normals, gridSize, deform);
}

NormalProducer::NormalProducer() : TileProducer("NormalProducer", "CreateNormalTile")
{
}

void NormalProducer::init(ptr<TileCache> cache, ptr<TileProducer> elevationTiles,
    ptr<Texture2D> normalTexture, ptr<Program> normals, int gridSize, bool deform)
{
    TileProducer::init(cache, true);
    this->elevationTiles = elevationTiles;
    this->normalTexture = normalTexture;
    this->normals = normals;
    this->frameBuffer = normalFramebufferFactory->get(normalTexture);
    this->deform = deform;
    this->gridMeshSize = gridSize;
    this->tileSDFU = normals->getUniform3f("tileSDF");
    this->elevationSamplerU = normals->getUniformSampler("elevationSampler");
    this->elevationOSLU = normals->getUniform4f("elevationOSL");
    this->normalSamplerU = normals->getUniformSampler("normalSampler");
    this->normalOSLU = normals->getUniform4f("normalOSL");
    this->patchCornersU = normals->getUniformMatrix4f("patchCorners");
    this->patchVerticalsU = normals->getUniformMatrix4f("patchVerticals");
    this->patchCornerNormsU = normals->getUniform4f("patchCornerNorms");
    this->worldToTangentFrameU = normals->getUniformMatrix3f("worldToTangentFrame");
    this->parentToTangentFrameU = normals->getUniformMatrix3f("parentToTangentFrame");
    this->deformU = normals->getUniform4f("deform");

    assert(cache->getStorage()->getTileSize() == elevationTiles->getCache()->getStorage()->getTileSize() - 2 * elevationTiles->getBorder());
    assert(normalTexture->getWidth() == cache->getStorage()->getTileSize());
    assert(normalTexture->getHeight() == cache->getStorage()->getTileSize());
    assert((cache->getStorage()->getTileSize() - 1) % gridSize == 0);
}

NormalProducer::~NormalProducer()
{
}

void NormalProducer::getReferencedProducers(vector< ptr<TileProducer> > &producers) const
{
    producers.push_back(elevationTiles);
}

void NormalProducer::setRootQuadSize(float size)
{
    TileProducer::setRootQuadSize(size);
    elevationTiles->setRootQuadSize(size);
}

int NormalProducer::getBorder()
{
    return 0;
}

bool NormalProducer::hasTile(int level, int tx, int ty)
{
    return elevationTiles->hasTile(level, tx, ty);
}

void *NormalProducer::getContext() const
{
    return normalTexture.get();
}

ptr<Task> NormalProducer::startCreateTile(int level, int tx, int ty, unsigned int deadline, ptr<Task> task, ptr<TaskGraph> owner)
{
    ptr<TaskGraph> result = owner == NULL ? createTaskGraph(task) : owner;

    if (level > 0) {
        TileCache::Tile *t = getTile(level - 1, tx / 2, ty / 2, deadline);
        assert(t != NULL);
        result->addTask(t->task);
        result->addDependency(task, t->task);
    }

    TileCache::Tile *t = elevationTiles->getTile(level, tx, ty, deadline);
    assert(t != NULL);
    result->addTask(t->task);
    result->addDependency(task, t->task);

    return result;
}

void NormalProducer::beginCreateTile()
{
    old = SceneManager::getCurrentFrameBuffer();
    SceneManager::setCurrentFrameBuffer(frameBuffer);
}

bool NormalProducer::doCreateTile(int level, int tx, int ty, TileStorage::Slot *data)
{
    if (Logger::DEBUG_LOGGER != NULL) {
        ostringstream oss;
        oss << "Normal tile " << getId() << " " << level << " " << tx << " " << ty;
        Logger::DEBUG_LOGGER->log("DEM", oss.str());
    }

    GPUTileStorage::GPUSlot *gpuData = dynamic_cast<GPUTileStorage::GPUSlot*>(data);
    assert(gpuData != NULL);

    int tileWidth = data->getOwner()->getTileSize();

    ptr<Texture> storage = getCache()->getStorage().cast<GPUTileStorage>()->getTexture(0);
    TextureInternalFormat f = storage->getInternalFormat();
    int components = storage->getComponents();
    bool signedComponents = f != RG8 && f != RGBA8;

    GPUTileStorage::GPUSlot *parentGpuData = NULL;
    if (level > 0) {
        TileCache::Tile *t = findTile(level - 1, tx / 2, ty / 2);
        assert(t != NULL);
        parentGpuData = dynamic_cast<GPUTileStorage::GPUSlot*>(t->getData());
        assert(parentGpuData != NULL);
    }

    TileCache::Tile *t = elevationTiles->findTile(level, tx, ty);
    assert(t != NULL);
    GPUTileStorage::GPUSlot *elevationGpuData = dynamic_cast<GPUTileStorage::GPUSlot*>(t->getData());
    assert(elevationGpuData != NULL);

    float format = components == 4 ? (signedComponents ? 0.0f : 1.0f) : (signedComponents ? 2.0f : 3.0f);
    tileSDFU->set(vec3f((float) tileWidth, float((getCache()->getStorage()->getTileSize() - 1) / gridMeshSize), format));

    if (normalSamplerU != NULL) {
        if (level > 0 && components == 4) {
            ptr<Texture> t = parentGpuData->t;
            float dx = (tx % 2) * (tileWidth / 2.0f);
            float dy = (ty % 2) * (tileWidth / 2.0f);
            normalSamplerU->set(parentGpuData->t);
            normalOSLU->set(vec4f((dx + 0.25f) / parentGpuData->getWidth(), (dy + 0.25f) / parentGpuData->getHeight(), 1.0f / parentGpuData->getWidth(), parentGpuData->l));
        } else {
            normalOSLU->set(vec4f(-1.0f, -1.0f, -1.0f, -1.0f));
        }
    }

    float dx = float(elevationTiles->getBorder());
    float dy = float(elevationTiles->getBorder());
    elevationSamplerU->set(elevationGpuData->t);
    elevationOSLU->set(vec4f((dx + 0.25f) / elevationGpuData->getWidth(), (dy + 0.25f) / elevationGpuData->getHeight(), 1.0f / elevationGpuData->getWidth(), elevationGpuData->l));

    if (deform) {
        double D = getRootQuadSize();
        double R = D / 2.0;
        assert(D > 0.0);
        double x0 = double(tx) / (1 << level) * D - R;
        double x1 = double(tx + 1) / (1 << level) * D - R;
        double y0 = double(ty) / (1 << level) * D - R;
        double y1 = double(ty + 1) / (1 << level) * D - R;
        double l0, l1, l2, l3;
        vec3d p0 = vec3d(x0, y0, R);
        vec3d p1 = vec3d(x1, y0, R);
        vec3d p2 = vec3d(x0, y1, R);
        vec3d p3 = vec3d(x1, y1, R);
        vec3d pc = vec3d((x0 + x1) * 0.5, (y0 + y1) * 0.5, R);
        vec3d v0 = p0.normalize(&l0);
        vec3d v1 = p1.normalize(&l1);
        vec3d v2 = p2.normalize(&l2);
        vec3d v3 = p3.normalize(&l3);
        vec3d vc = (v0 + v1 + v2 + v3) * 0.25;

        mat4d deformedCorners = mat4d(
            v0.x * R - vc.x * R, v1.x * R - vc.x * R, v2.x * R - vc.x * R, v3.x * R - vc.x * R,
            v0.y * R - vc.y * R, v1.y * R - vc.y * R, v2.y * R - vc.y * R, v3.y * R - vc.y * R,
            v0.z * R - vc.z * R, v1.z * R - vc.z * R, v2.z * R - vc.z * R, v3.z * R - vc.z * R,
            1.0, 1.0, 1.0, 1.0);

        mat4d deformedVerticals = mat4d(
            v0.x, v1.x, v2.x, v3.x,
            v0.y, v1.y, v2.y, v3.y,
            v0.z, v1.z, v2.z, v3.z,
            0.0, 0.0, 0.0, 0.0);

        vec3d uz = pc.normalize();
        vec3d ux = vec3d::UNIT_Y.crossProduct(uz).normalize();
        vec3d uy = uz.crossProduct(ux);
        mat3d worldToTangentFrame = mat3d(
            ux.x, ux.y, ux.z,
            uy.x, uy.y, uy.z,
            uz.x, uz.y, uz.z);

        if (level > 0  && parentToTangentFrameU != NULL) {
            double px0 = (tx/2 + 0.5) / (1 << (level - 1)) * D - R;
            double py0 = (ty/2 + 0.5) / (1 << (level - 1)) * D - R;
            pc = vec3d(px0, py0, R);
            uz = pc.normalize();
            ux = vec3d::UNIT_Y.crossProduct(uz).normalize();
            uy = uz.crossProduct(ux);
            mat3d parentToTangentFrame = worldToTangentFrame * mat3d(
                ux.x, uy.x, uz.x,
                ux.y, uy.y, uz.y,
                ux.z, uy.z, uz.z);
            parentToTangentFrameU->setMatrix(parentToTangentFrame.cast<float>());
        }

        patchCornersU->setMatrix(deformedCorners.cast<float>());
        patchVerticalsU->setMatrix(deformedVerticals.cast<float>());
        patchCornerNormsU->set(vec4d(l0, l1, l2, l3).cast<float>());
        worldToTangentFrameU->setMatrix(worldToTangentFrame.cast<float>());
        deformU->set(vec4d(x0, y0, D / (1 << level), R).cast<float>());
    } else {
        double D = getRootQuadSize();
        double R = D / 2.0;
        double x0 = double(tx) / (1 << level) * D - R;
        double y0 = double(ty) / (1 << level) * D - R;
        if (worldToTangentFrameU != NULL) {
            worldToTangentFrameU->setMatrix(mat3f::IDENTITY);
        }
        deformU->set(vec4d(x0, y0, D / (1 << level), 0.0).cast<float>());
    }

    frameBuffer->drawQuad(normals);
    gpuData->copyPixels(frameBuffer, 0, 0, tileWidth, tileWidth);

    return true;
}

void NormalProducer::endCreateTile()
{
    SceneManager::setCurrentFrameBuffer(old);
    old = NULL;
}

void NormalProducer::stopCreateTile(int level, int tx, int ty)
{
    if (level > 0) {
        TileCache::Tile *t = findTile(level - 1, tx / 2, ty / 2);
        assert(t != NULL);
        putTile(t);
    }

    TileCache::Tile *t = elevationTiles->findTile(level, tx, ty);
    assert(t != NULL);
    elevationTiles->putTile(t);
}

void NormalProducer::swap(ptr<NormalProducer> p)
{
    TileProducer::swap(p);
    std::swap(frameBuffer, p->frameBuffer);
    std::swap(old, p->old);
    std::swap(normals, p->normals);
    std::swap(elevationTiles, p->elevationTiles);
    std::swap(normalTexture, p->normalTexture);
    std::swap(deform, p->deform);
    std::swap(gridMeshSize, p->gridMeshSize);
    std::swap(tileSDFU, p->tileSDFU);
    std::swap(elevationSamplerU, p->elevationSamplerU);
    std::swap(elevationOSLU, p->elevationOSLU);
    std::swap(normalSamplerU, p->normalSamplerU);
    std::swap(normalOSLU, p->normalOSLU);
    std::swap(patchCornersU, p->patchCornersU);
    std::swap(patchVerticalsU, p->patchVerticalsU);
    std::swap(patchCornerNormsU, p->patchCornerNormsU);
    std::swap(worldToTangentFrameU, p->worldToTangentFrameU);
    std::swap(parentToTangentFrameU, p->parentToTangentFrameU);
    std::swap(deformU, p->deformU);
}

class NormalProducerResource : public ResourceTemplate<50, NormalProducer>
{
public:
    NormalProducerResource(ptr<ResourceManager> manager, const string &name, ptr<ResourceDescriptor> desc, const TiXmlElement *e = NULL) :
        ResourceTemplate<50, NormalProducer>(manager, name, desc)
    {
        e = e == NULL ? desc->descriptor : e;
        ptr<TileCache> cache;
        ptr<TileProducer> elevations;
        ptr<Texture2D> normalTexture;
        ptr<Program> normalsProg;
        int gridSize = 24;
        bool deform = false;
        checkParameters(desc, e, "name,cache,elevations,normalProg,gridSize,deform,");
        cache = manager->loadResource(getParameter(desc, e, "cache")).cast<TileCache>();
        elevations = manager->loadResource(getParameter(desc, e, "elevations")).cast<TileProducer>();
        string normals = "normalShader;";
        if (e->Attribute("normalProg") != NULL) {
            normals = getParameter(desc, e, "normalProg");
        }
        normalsProg = manager->loadResource(normals).cast<Program>();
        if (e->Attribute("gridSize") != NULL) {
            getIntParameter(desc, e, "gridSize", &gridSize);
        }
        if (e->Attribute("deform") != NULL && strcmp(e->Attribute("deform"), "sphere") == 0) {
            deform = true;
        }

        int tileSize = cache->getStorage()->getTileSize();
        const char* format = cache->getStorage().cast<GPUTileStorage>()->getTexture(0)->getInternalFormatName();
        if (strncmp(format, "RG8", 3) == 0) {
            format = "RGBA8";
        }

        ostringstream normalTex;
        normalTex << "renderbuffer-" << tileSize << "-" << format;
        normalTexture = manager->loadResource(normalTex.str()).cast<Texture2D>();

        init(cache, elevations, normalTexture, normalsProg, gridSize, deform);
    }

    virtual bool prepareUpdate()
    {
        if (dynamic_cast<Resource*>(normals.get())->changed()) {
            invalidateTiles();
        }
        return ResourceTemplate<50, NormalProducer>::prepareUpdate();
    }
};

extern const char normalProducer[] = "normalProducer";

static ResourceFactory::Type<normalProducer, NormalProducerResource> NormalProducerType;

}
