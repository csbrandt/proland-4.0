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

#include "proland/plants/LccProducer.h"

#include "ork/math/mat2.h"
#include "ork/resource/ResourceTemplate.h"
#include "ork/scenegraph/SceneManager.h"

#include "proland/producer/GPUTileStorage.h"
#include "proland/terrain/SphericalDeformation.h"

namespace proland
{

const char* copyLccShader = "\
uniform sampler2DArray sourceSampler;\n\
uniform vec4 sourceOSL;\n\
#ifdef _VERTEX_\n\
layout(location=0) in vec4 vertex;\n\
out vec3 stl;\n\
void main() {\n\
    gl_Position = vec4(vertex.xy, 0.0, 1.0);\n\
    stl = vec3((vertex.xy * 0.5 + vec2(0.5)) * sourceOSL.z + sourceOSL.xy, sourceOSL.w);\n\
}\n\
#endif\n\
#ifdef _FRAGMENT_\n\
in vec3 stl;\n\
layout(location=0) out vec4 data;\n\
void main() {\n\
    data = vec4(textureLod(sourceSampler, stl, 0.0).rg, 0.0, 0.0);\n\
}\n\
#endif\n\
";

ptr<FrameBuffer> createLccFramebuffer(ptr<Texture2D> lccTexture)
{
    int tileWidth = lccTexture->getWidth();
    ptr<FrameBuffer> frameBuffer(new FrameBuffer());
    frameBuffer->setReadBuffer(COLOR0);
    frameBuffer->setDrawBuffer(COLOR0);
    frameBuffer->setViewport(vec4<GLint>(0, 0, tileWidth, tileWidth));
    frameBuffer->setTextureBuffer(COLOR0, lccTexture, 0);
    frameBuffer->setPolygonMode(FILL, FILL);
    frameBuffer->setBlend(true, ADD, ONE, ONE, ADD, ONE, ZERO);
    return frameBuffer;
}

static_ptr< Factory< ptr<Texture2D>, ptr<FrameBuffer> > > lccFramebufferFactory(
    new Factory< ptr<Texture2D>, ptr<FrameBuffer> >(createLccFramebuffer));

static_ptr<FrameBuffer> LccProducer::old;

LccProducer::LccProducer(ptr<TileProducer> delegate, ptr<Plants> plants, ptr<Texture2D> lccTexture,
    ptr<Program> copy, ptr<Program> dots, int maxLevel, bool deform) :
        TileProducer("LccProducer", "CreateLcc", delegate->getCache(), true)
{
    init(delegate, plants, lccTexture, copy, dots, maxLevel, deform);
}

LccProducer::LccProducer() : TileProducer("LccProducer", "CreateLcc")
{
}

void LccProducer::init(ptr<TileProducer> delegate, ptr<Plants> plants, ptr<Texture2D> lccTexture,
    ptr<Program> copy, ptr<Program> dots, int maxLevel, bool deform)
{
    TileProducer::init(delegate->getCache(), true);
    this->delegate = delegate;
    this->plants = plants;
    this->lccTexture = lccTexture;
    this->copy = copy;
    this->dots = dots;
    this->maxLevel = maxLevel;
    this->deform = deform;
    this->lastTreeDensity = 0.0;
    this->lastFov = 0.0;
    this->densityU = plants->renderProg->getUniform1f("treeDensity");
    this->sourceSamplerU = copy->getUniformSampler("sourceSampler");
    this->sourceOSLU = copy->getUniform4f("sourceOSL");
    this->tileOffsetU = dots->getUniform4f("tileOffset");
    this->tileDeformU = dots->getUniformMatrix2f("tileDeform");
    this->tileClipU = dots->getUniform4f("tileClip");
    this->densitySamplerU = dots->getUniformSampler("densitySampler");
    this->densityOSLU = dots->getUniform4f("densityOSL");
    this->frameBuffer = lccFramebufferFactory->get(lccTexture);
}

LccProducer::~LccProducer()
{
}

void LccProducer::setRootQuadSize(float size)
{
    TileProducer::setRootQuadSize(size);
    delegate->setRootQuadSize(size);
}

int LccProducer::getBorder()
{
    return delegate->getBorder();
}

bool LccProducer::hasTile(int level, int tx, int ty)
{
    if (maxLevel > 0) {
        return delegate->hasTile(level, tx, ty) || level <= min(plants->getMaxLevel(), maxLevel);
    } else {
        return delegate->hasTile(level, tx, ty) || level <= plants->getMaxLevel();
    }
}

TileCache::Tile* LccProducer::findTile(int level, int tx, int ty, bool includeCache, bool done)
{
    if (delegate->hasTile(level, tx, ty)) {
        return delegate->findTile(level, tx, ty, includeCache, done);
    }
    return TileProducer::findTile(level, tx, ty, includeCache, done);
}

TileCache::Tile* LccProducer::getTile(int level, int tx, int ty, unsigned int deadline)
{
    if (delegate->hasTile(level, tx, ty)) {
        return delegate->getTile(level, tx, ty, deadline);
    }
    return TileProducer::getTile(level, tx, ty, deadline);
}

bool LccProducer::prefetchTile(int level, int tx, int ty)
{
    if (delegate->hasTile(level, tx, ty)) {
        return delegate->prefetchTile(level, tx, ty);
    }
    return TileProducer::prefetchTile(level, tx, ty);
}

void LccProducer::putTile(TileCache::Tile *t)
{
    if (delegate->hasTile(t->level, t->tx, t->ty)) {
        return delegate->putTile(t);
    }
    return TileProducer::putTile(t);
}

void LccProducer::invalidateTiles()
{
    delegate->invalidateTiles();
    TileProducer::invalidateTiles();
}

void LccProducer::invalidateTile(int level, int tx, int ty)
{
    if (delegate->hasTile(level, tx, ty)) {
        delegate->invalidateTile(level, tx, ty);
        return;
    }
    TileProducer::invalidateTile(level, tx, ty);
}

void LccProducer::update(ptr<SceneManager> scene)
{
    if (densityU != NULL) {
        float td = densityU->get();
        if (lastTreeDensity != 0.0 && lastTreeDensity != td) {
            invalidateTiles();
        }
        lastTreeDensity = td;
    }

    vec4d frustum[6];
    SceneManager::getFrustumPlanes(scene->getCameraToScreen(), frustum);
    vec3d left = frustum[0].xyz().normalize();
    vec3d right = frustum[1].xyz().normalize();
    float fov = (float) safe_acos(-left.dotproduct(right));
    if (lastFov != 0.0 && lastFov != fov) {
        plants->setMaxDistance(plants->getMaxDistance() * tan(lastFov / 2.0) / tan(fov / 2.0));
        invalidateTiles();
    }
    lastFov = fov;
}

void LccProducer::getReferencedProducers(vector< ptr<TileProducer> > &producers) const
{
    producers.push_back(delegate);
}

void *LccProducer::getContext() const
{
    return lccTexture.get();
}

ptr<Task> LccProducer::startCreateTile(int level, int tx, int ty, unsigned int deadline, ptr<Task> task, ptr<TaskGraph> owner)
{
    ptr<TaskGraph> result = owner == NULL ? createTaskGraph(task) : owner;

    int l = level;
    int x = tx;
    int y = ty;
    while (!delegate->hasTile(l, x, y)) {
        x /= 2;
        y /= 2;
        l -= 1;
    }
    TileCache::Tile *t = delegate->getTile(l, x, y, deadline);
    result->addTask(t->task);
    result->addDependency(task, t->task);

    TileProducer::startCreateTile(level, tx, ty, deadline, task, result);

    return result;
}

void LccProducer::beginCreateTile()
{
    old = SceneManager::getCurrentFrameBuffer();
    SceneManager::setCurrentFrameBuffer(frameBuffer);
    TileProducer::beginCreateTile();
}

bool LccProducer::doCreateTile(int level, int tx, int ty, TileStorage::Slot *data)
{
    double rootQuadSize = getRootQuadSize();
    int tileBorder = getBorder();
    int tileWidth = data->getOwner()->getTileSize();
    int tileSize = tileWidth - 2 * tileBorder;
    if (deformation == NULL) {
        if (deform) {
            deformation = new SphericalDeformation(rootQuadSize / 2.0);
        } else {
            deformation = new Deformation();
        }
    }

    int m = 1 << plants->getMaxLevel();
    int n = 1 << (plants->getMaxLevel() - level);
    float r = plants->getPoissonRadius();

    TileCache::Tile *t = NULL;
    vec4f coords = delegate->getGpuTileCoords(level, tx, ty, &t);
    assert(t != NULL);
    GPUTileStorage::GPUSlot *parentGpuData = dynamic_cast<GPUTileStorage::GPUSlot*>(t->getData());
    assert(parentGpuData != NULL);
    float b = float(tileBorder) / (1 << (level - t->level));
    float s = tileWidth;
    float S = s / (s - 2 * tileBorder);

    densitySamplerU->set(parentGpuData->t);

    frameBuffer->clear(true, false, false);

    sourceSamplerU->set(parentGpuData->t);
    sourceOSLU->set(vec4f(coords.x - b / parentGpuData->getWidth(), coords.y - b / parentGpuData->getHeight(), coords.w * S, coords.z));
    frameBuffer->drawQuad(copy);

    for (int y = 0; y < n; ++y) {
        int iy = ty * n + y;
        for (int x = 0; x < n; ++x) {
            int ix = tx * n + x;
            double ox = rootQuadSize * (float(ix) / m - 0.5f);
            double oy = rootQuadSize * (float(iy) / m - 0.5f);
            double l = rootQuadSize / m;

            float x0 = float(x) / n;
            float y0 = float(y) / n;
            float ql = 1.0f / n;
            float bx0 = 2.0 * (tileSize * x0 + tileBorder) / tileWidth - 1.0;
            float by0 = 2.0 * (tileSize * y0 + tileBorder) / tileWidth - 1.0;
            float bql = (2.0 * tileSize) / tileWidth * ql;
            tileOffsetU->set(vec4f(bx0, by0, bql, r));

            tileClipU->set(vec4f(x == 0 ? -1 : 0, x == n - 1 ? 2 : 1, y == 0 ? -1 : 0, y == n - 1 ? 2 : 1));

            if (tileDeformU != NULL) {
                mat4d l2d = deformation->localToDeformedDifferential(vec3d(ox + l / 2.0f, oy + l / 2.0f, 0.0f));
                mat4d d2t = deformation->deformedToTangentFrame(l2d * vec3d::ZERO);
                mat4d t2l = l2d.inverse() * d2t.inverse();
                tileDeformU->set(mat2f(t2l[0][0], t2l[0][1], t2l[1][0], t2l[1][1]).coefficients());
            }

            densityOSLU->set(vec4f(coords.x + x0 * coords.w, coords.y + y0 * coords.w, ql * coords.w, coords.z));

            int patternId = int(881 * abs(cos(ox * oy))) % plants->getPatternCount(); // TODO improve this
            ptr<MeshBuffers> pattern = plants->getPattern(patternId);
            int nSeeds = int(pattern->nvertices);

            frameBuffer->draw(dots, *pattern, POINTS, 0, nSeeds);
        }
    }

    GPUTileStorage::GPUSlot *gpuData = dynamic_cast<GPUTileStorage::GPUSlot*>(data);
    assert(gpuData != NULL);
    getCache()->getStorage().cast<GPUTileStorage>()->notifyChange(gpuData);
    gpuData->copyPixels(frameBuffer, 0, 0, tileWidth, tileWidth);

    return true;
}

void LccProducer::endCreateTile()
{
    TileProducer::endCreateTile();
    SceneManager::setCurrentFrameBuffer(old);
    old = NULL;
}

void LccProducer::stopCreateTile(int level, int tx, int ty)
{
    int l = level;
    int x = tx;
    int y = ty;
    while (!delegate->hasTile(l, x, y)) {
        x /= 2;
        y /= 2;
        l -= 1;
    }
    TileCache::Tile *t = delegate->findTile(l, x, y);
    assert(t != NULL);
    delegate->putTile(t);

    TileProducer::stopCreateTile(level, tx, ty);
}

class LccProducerResource : public ResourceTemplate<3, LccProducer>
{
public:
    LccProducerResource(ptr<ResourceManager> manager, const string &name, ptr<ResourceDescriptor> desc, const TiXmlElement *e = NULL) :
        ResourceTemplate<3, LccProducer>(manager, name, desc)
    {
        e = e == NULL ? desc->descriptor : e;
        checkParameters(desc, e, "name,cache,density,plants,maxLevel,deform,");
        ptr<TileProducer> delegate = manager->loadResource(getParameter(desc, e, "density")).cast<TileProducer>();
        ptr<Plants> plants = manager->loadResource(getParameter(desc, e, "plants")).cast<Plants>();

        ptr<TileCache> cache = delegate->getCache();
        int tileWidth = cache->getStorage()->getTileSize();
        int channels = 4;//hasLayers() ? 4 : cache->getStorage().cast<GPUTileStorage>()->getTexture(0)->getComponents();

        ostringstream lccTex;
        lccTex << "renderbuffer-" << tileWidth;
        switch (channels) {
        case 1:
            lccTex << "-R8";
            break;
        case 2:
            lccTex << "-RG8";
            break;
        case 3:
            lccTex << "-RGB8";
            break;
        default:
            lccTex << "-RGBA8";
            break;
        }
        ptr<Texture2D> lcc = manager->loadResource(lccTex.str()).cast<Texture2D>();

        ptr<Program> copy = new Program(new Module(330, copyLccShader));
        ptr<Program> dots = manager->loadResource("globalsShaderGS;dots;").cast<Program>();

        int maxLevel = -1;
        if (e->Attribute("maxLevel") != NULL) {
            getIntParameter(desc, e, "maxLevel", &maxLevel);
        }
        bool deform = e->Attribute("deform") != NULL && strcmp(e->Attribute("deform"), "true") == 0;

        init(delegate, plants, lcc, copy, dots, maxLevel, deform);
    }
};

extern const char lccProducer[] = "lccProducer";

static ResourceFactory::Type<lccProducer, LccProducerResource> LccProducerType;

}
