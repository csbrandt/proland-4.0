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

#include "proland/particles/screen/ScreenParticleLayer.h"

#include "pmath.h"

#include "ork/render/Program.h"
#include "ork/resource/ResourceTemplate.h"

#include "proland/particles/screen/ParticleGrid.h"
#include "proland/particles/terrain/TerrainParticleLayer.h"

using namespace std;
using namespace ork;

namespace proland
{

const char* packerShader = "\
uniform sampler2D depthTexture; // depth buffer texture\n\
uniform vec3 size; // viewport width and height + particles count\n\
\n\
#ifdef _VERTEX_\n\
layout (location = 0) in vec3 vertex;\n\
out vec2 uv;\n\
void main() {\n\
    vec3 v = vertex.xyz / size;\n\
    uv = v.xy;\n\
    gl_Position = vec4(2.0 * v.z - 1.0, 0.0, 0.0, 1.0);\n\
}\n\
#endif\n\
#ifdef _FRAGMENT_\n\
layout(location = 0) out vec4 data;\n\
in vec2 uv;\n\
void main() {\n\
    data = texture(depthTexture, uv);\n\
}\n\
#endif\n\
";

static_ptr<Program> ScreenParticleLayer::packer(NULL);

static_ptr<Texture2D> ScreenParticleLayer::depthBuffer(NULL);

static const float TWO_PI = (float) (M_PI*2);

#define K_SMALLEST_RANGE 0.000001f

/**
 * Allows fast-computing of the available area around a point, using
 * angles. Acquired from Qizhi Yu's implementation of his thesis, itself
 * acquired from Daniel Dunbar & Gred Humphreys in "A Spatial Data structure
 * for fast poisson disk sample generation".
 */
class RangeList
{
public:
    struct RangeEntry
    {
        float min;

        float max;

        RangeEntry() : min(0.0f), max(0.0f)
        {
        }
    };

    /**
     * Creates a new RangleList.
     */
    RangeList()
    {
        numRanges = 0;
        rangesSize = 8;
        ranges = new RangeEntry[rangesSize];
    }

    /**
     * Deletes this RangeList.
     */
    ~RangeList()
    {
        delete[] ranges;
    }

    /**
     * Returns the number of ranges in this list.
     */
    int getRangeCount() const
    {
        return numRanges;
    }

    /**
     * Returns the range entry at the given index.
     */
    const RangeEntry *getRange(int index) const
    {
        return ranges + index;
    }

    /**
     * Resets the list of range entries.
     *
     * @param min min angle to search from.
     * @param max max angle to search from.
     */
    void reset(float min, float max)
    {
        numRanges = 1;
        ranges[0].min = min;
        ranges[0].max = max;
    }

    /**
     * Removes an area from the available neighboring.
     *
     * @param min begining of the angle to remove.
     * @param max end of the angle to remove.
     */
    void subtract(float min, float max)
    {
        if (min > TWO_PI) {
            subtract(min - TWO_PI, max - TWO_PI);
        } else if (max < 0) {
            subtract(min + TWO_PI, max + TWO_PI);
        } else if (min < 0) {
            subtract(0, max);
            subtract(min + TWO_PI, TWO_PI);
        } else if (max > TWO_PI) {
            subtract(min, TWO_PI);
            subtract(0, max - TWO_PI);
        } else if (numRanges > 0) {
            int pos;
            if (min < ranges[0].min) {
                pos = -1;
            } else {
                int lo = 0;
                int mid = 0;
                int hi = numRanges;
                while (lo < hi - 1) {
                    mid = (lo + hi) >> 1;
                    if (ranges[mid].min < min) {
                        lo = mid;
                    } else {
                        hi = mid;
                    }
                }
                pos = lo;
            }
            if (pos == -1) {
                pos = 0;
            } else if (min < ranges[pos].max) {
                float c = ranges[pos].min;
                float d = ranges[pos].max;
                if (min - c < K_SMALLEST_RANGE) {
                    if (max < d) {
                        ranges[pos].min = max;
                    } else {
                        deleteRange(pos);
                    }
                } else {
                    ranges[pos].max = min;
                    if (max < d) {
                        insertRange(pos + 1, max, d);
                    }
                    pos++;
                }
            } else {
                if (pos < numRanges - 1 && max > ranges[pos + 1].min) {
                    pos++;
                } else {
                    return;
                }
            }
            while (pos < numRanges && max >= (ranges[pos].min)) {
                if (ranges[pos].max - max < K_SMALLEST_RANGE) {
                    deleteRange(pos);
                } else {
                    ranges[pos].min = max;
                    if (ranges[pos].max > max) {
                        break;
                    }
                }
            }
        }
    }

private:
    /**
     * List of entries corresponding to neighboring objects.
     */
    RangeEntry *ranges;

    /**
     * Number of entries.
     */
    int numRanges;

    /**
     * Size of #ranges.
     */
    int rangesSize;

    /**
     * Removes an entry.
     *
     * @param pos the index of the entry to remove.
     */
    void deleteRange(int pos)
    {
        if (pos < numRanges - 1) {
            memmove(&ranges[pos], &ranges[pos + 1], sizeof(*ranges) * (numRanges - (pos + 1)));
        }
        numRanges--;
    }

    /**
     * Adds an entry at a given position.
     *
     * @param pos an index.
     * @param min begining of the angle to remove.
     * @param max end of the angle to remove.
     */
    void insertRange(int pos, float min, float max)
    {
        if (numRanges == rangesSize) {
            rangesSize++;
            RangeEntry *tmp = new RangeEntry[rangesSize];
            memcpy(tmp, ranges, numRanges * sizeof(*tmp));
            delete[] ranges;
            ranges = tmp;
        }

        assert(pos < rangesSize);
        if (pos < numRanges) {
            memmove(&ranges[pos + 1], &ranges[pos], sizeof(*ranges) * (numRanges - pos));
        }
        ranges[pos].min = min;
        ranges[pos].max = max;
        numRanges++;
    }
};

ScreenParticleLayer::ScreenParticleLayer(float radius, ptr<Texture2D> offscreenDepthBuffer) :
    ParticleLayer("ScreenParticleLayer", sizeof(ScreenParticle))
{
    init(radius, offscreenDepthBuffer);
}

ScreenParticleLayer::ScreenParticleLayer() :
    ParticleLayer("ScreenParticleLayer", sizeof(ScreenParticle))
{
}

void ScreenParticleLayer::init(float radius, ptr<Texture2D> offscreenDepthBuffer)
{
    this->scene = NULL;
    this->radius = radius;
    this->depthBuffer = offscreenDepthBuffer;
    this->useOffscreenDepthBuffer = depthBuffer != NULL;
    this->bounds = box2f(0.0f, 0.0f, 0.0f, 0.0f);
    this->grid = new ParticleGrid(4.0f * radius, 64);
    this->ranges = new RangeList();
    this->lastWorldToScreen = mat4d::IDENTITY;
    this->lastViewport = vec4i::ZERO;
    this->depthBufferRead = false;
    this->depthArraySize = 0;
    this->depthArray = NULL;
    this->worldLayer = NULL;
    this->lifeCycleLayer = NULL;
    this->frameBuffer = NULL;
}

ScreenParticleLayer::~ScreenParticleLayer()
{
    delete ranges;
    delete grid;
}

float ScreenParticleLayer::getParticleRadius() const
{
    return radius;
}

void ScreenParticleLayer::setParticleRadius(float radius)
{
    this->radius = radius;
    grid->setParticleRadius(4.0f * radius);
    getOwner()->getStorage()->clear();
}

void ScreenParticleLayer::setSceneManager(SceneManager *manager)
{
    this->scene = manager;
}

void ScreenParticleLayer::moveParticles(double dt)
{
    ptr<FrameBuffer> fb = SceneManager::getCurrentFrameBuffer();
    vec4<GLint> v = fb->getViewport();
    assert(v.z >= 0 && v.w >= 0);
    bounds = box2f(v.x, v.x + v.z, v.y, v.y + v.w);
    grid->setViewport(box2i(v.x, v.x + v.z, v.y, v.y + v.w));

    // here we update the screen position of particles, using their world
    // position and the world to screen transformation (this supposes that
    // the world positions have already been updated, by another layer). We
    // then force particles that project outside the frustum to fade out.

    mat4d toScreen = scene->getWorldToScreen();
    float ax = (bounds.xmax - bounds.xmin) / 2.0f;
    float bx = (bounds.xmax + bounds.xmin) / 2.0f;
    float ay = (bounds.ymax - bounds.ymin) / 2.0f;
    float by = (bounds.ymax + bounds.ymin) / 2.0f;

    ptr<ParticleStorage> s = getOwner()->getStorage();
    vector<ParticleStorage::Particle*>::iterator i = s->getParticles();
    vector<ParticleStorage::Particle*>::iterator end = s->end();

    box2f enlargedBounds = bounds.enlarge(radius * 2.0f);
    while (i != end) {
        ParticleStorage::Particle *p = *i;
        ScreenParticle *s = getScreenParticle(p);
        WorldParticleLayer::WorldParticle *w = worldLayer->getWorldParticle(p);
        if (w->worldPos.x != UNINITIALIZED) {
            vec4d q = toScreen * vec4d(w->worldPos, 1.0);
            float x = ax * q.x / q.w + bx;
            float y = ay * q.y / q.w + by;
            s->screenPos = vec2f(x, y);

            if (x < bounds.xmin || x >= bounds.xmax || y < bounds.ymin || y >= bounds.ymax) {
                // warning: we do not use bounds.contains() on purpose! (to exclude
                // equality with max bounds, so that floor(screenPos) is strictly
                // less than viewport width and height)
                if (x < enlargedBounds.xmin || x >= enlargedBounds.xmax || y < enlargedBounds.ymin || y >= enlargedBounds.ymax) {
                    lifeCycleLayer->killParticle(p);
                } else {
                    lifeCycleLayer->setFadingOut(p);
                }
                s->reason = OUTSIDE_VIEWPORT;
            }
        }
        ++i;
    }
}

void ScreenParticleLayer::removeOldParticles()
{
    grid->clear();

    float rangeSqrD = (0.96f * 4.0f) * radius * radius;

    ptr<ParticleStorage> s = getOwner()->getStorage();
    vector<ParticleStorage::Particle*>::iterator i = s->getParticles();
    vector<ParticleStorage::Particle*>::iterator end = s->end();
    while (i != end) {
        ParticleStorage::Particle *p = *i++;
        ScreenParticle *s = getScreenParticle(p);
        if (lifeCycleLayer->isFadingOut(p)) {
         // we do not take fading out particles into account
         // to compute the Poisson-disk distribution
            continue;
        }

        bool remove = false;
        vec2i gridSize = grid->getGridSize();
        vec2i cell = grid->getCell(s->screenPos);

        cell.x = min(gridSize.x - 1, max(cell.x, 0));
        cell.y = min(gridSize.y - 1, max(cell.y, 0));

        int n = grid->getCellSize(cell);
        ScreenParticle **neighbors = grid->getCellContent(cell);
        for (int j = 0; j < n; ++j) {
            ParticleStorage::Particle *np = getParticle(neighbors[j]);//getParticle(neighbors + j);
            ScreenParticle *ns = neighbors[j];
            if (ns == s || lifeCycleLayer->isFadingOut(np)) {
                continue;
            }
            float sqrD = (ns->screenPos - s->screenPos).squaredLength();
            if (sqrD < rangeSqrD) {
                lifeCycleLayer->setFadingOut(p);
                s->reason = POISSON_DISK;
                remove = true;
                break;
            }
        }
        if (!remove) {
            grid->addParticle(s, lifeCycleLayer->getIntensity(p));
        }
    }
}

void ScreenParticleLayer::addNewParticles()
{
    if (bounds.xmax - bounds.xmin == 0 && bounds.ymax - bounds.ymin == 0) {
        return;
    }
    vector<ScreenParticle*> candidates;
    vector<ScreenParticle*> newParticles;

    // --------------------------------------------
    // first, creates new particles in the viewport

    // finds candidates from existing particles
    ptr<ParticleStorage> storage = getOwner()->getStorage();
    vector<ParticleStorage::Particle*>::iterator i = storage->getParticles();
    vector<ParticleStorage::Particle*>::iterator end = storage->end();
    while (i != end) {
        ScreenParticle *p = getScreenParticle(*i);

        if (bounds.contains(p->screenPos) && (!lifeCycleLayer->isFadingOut(*i))) {
            candidates.push_back(p);
            WorldParticleLayer::WorldParticle *w = worldLayer->getWorldParticle(*i);
            if (w->worldPos.x == UNINITIALIZED || w->worldPos.y == UNINITIALIZED || w->worldPos.z == UNINITIALIZED ) {
                newParticles.push_back(p);
            }
        }
        ++i;
    }
    if (candidates.empty()) {
        // if no candidates were found, we want to add the new particles near to the existing ones
        // so, we need to get them, but we can't generate the new particles inside the existing cloud
        // so we must retrieve the whole list and the rest of the algorithm will sort them out.
        i = storage->getParticles();
        end = storage->end();
        while (i != end) {
            ScreenParticle *p = getScreenParticle(*i);
            if (bounds.contains(p->screenPos) && (!lifeCycleLayer->isFadingOut(*i) || p->reason != OUTSIDE_VIEWPORT)) {
                candidates.push_back(p);
                WorldParticleLayer::WorldParticle *w = worldLayer->getWorldParticle(*i);
                if (w->worldPos.x == UNINITIALIZED || w->worldPos.y == UNINITIALIZED || w->worldPos.z == UNINITIALIZED ) {
                    newParticles.push_back(p);
                }
            }
            ++i;
        }
    }
    if (candidates.empty()) {
        // if still no candidates, we need to create a new one
        vec2f p;
        p.x = bounds.xmin + (bounds.xmax - bounds.xmin) * rand() / (RAND_MAX + 1.0f);
        p.y = bounds.ymin + (bounds.ymax - bounds.ymin) * rand() / (RAND_MAX + 1.0f);

        ScreenParticle *s = newScreenParticle(p);
        if (s == NULL) {
            return;
        }

        candidates.push_back(s);
        newParticles.push_back(s);
        assert(s != NULL);
    }

    while (!candidates.empty()) {
        // selects a candidate at random
        int c = rand() % (int) candidates.size();
        ScreenParticle *p = candidates[c];
        vec2f pos = p->screenPos;
        // removes this candidate from the list
        candidates[c] = candidates[int(candidates.size()) - 1];
        candidates.pop_back();

        ranges->reset(0.0f, 2.0f * M_PI);
        findNeighborRanges(p);

        while (ranges->getRangeCount() != 0) {
            // selects a range at random
            const RangeList::RangeEntry *re = ranges->getRange(rand() % ranges->getRangeCount());
            // selects a point at random in this range
            float angle = re->min + (re->max - re->min) * rand() / (float) RAND_MAX;
            ranges->subtract(angle - M_PI / 3.0f, angle + M_PI / 3.0f);

            vec2f pt = pos + vec2f(cos(angle), sin(angle)) * 2.0f * radius;
            if (pt.x >= bounds.xmin && pt.x < bounds.xmax && pt.y >= bounds.ymin && pt.y < bounds.ymax) {
                // warning: we do not use bounds.contains() on purpose! (to exclude
                // equality with max bounds, so that floor(screenPos) is strictly
                // less than viewport width and height)
                ScreenParticle *s = newScreenParticle(pt);
                if (s == NULL) {
                    candidates.clear();
                    break;
                }
                candidates.push_back(s);
                newParticles.push_back(s);
            }
        }
    }
    // --------------------------------------------
    // then, computes the world position of these new particles

    // we first check if the camera has moved or not
    int left = int(bounds.xmin);
    int bottom = int(bounds.ymin);
    int width = int(bounds.xmax - bounds.xmin);
    int height = int(bounds.ymax - bounds.ymin);
    vec4i viewport = vec4i(left, bottom, width, height);
    mat4d toScreen = scene->getWorldToScreen();
    bool sameView = lastViewport == viewport && lastWorldToScreen == toScreen;

    // we then get the particles depths, using one of two methods
    if (sameView) {
        // if the camera has not moved, we read the whole depth buffer,
        // unless it has already been done
        if (!depthBufferRead) {
            if (depthArraySize < width * height) {
                if (depthArray != NULL) {
                    delete depthArray;
                }
                depthArraySize = width * height;
                depthArray = new float[depthArraySize];
            }
            ptr<FrameBuffer> fb = SceneManager::getCurrentFrameBuffer();
            fb->readPixels(0, 0, width, height, DEPTH_COMPONENT, FLOAT, Buffer::Parameters(), CPUBuffer(depthArray));
            depthBufferRead = true;
        }
    } else {
        depthBufferRead = false;
        // if the camera has moved, we only read the depths of the new particles
        getParticleDepths(newParticles);
    }

    lastViewport = viewport;
    lastWorldToScreen = toScreen;

    // finally we use these depths to get the world positions
    mat4d screenToWorld = scene->getWorldToScreen().inverse();
    for (unsigned int i = 0; i < newParticles.size(); ++i) {
        ScreenParticle *s = newParticles[i];
        ParticleStorage::Particle *p = getParticle(s);
        WorldParticleLayer::WorldParticle *w = worldLayer->getWorldParticle(p);

        float winx = 2.0f * (s->screenPos.x - bounds.xmin) / width - 1.0f;
        float winy = 2.0f * (s->screenPos.y - bounds.ymin) / height - 1.0f;
        float winz;
        if (sameView) {
            int x = (int) floor(s->screenPos.x);
            int y = (int) floor(s->screenPos.y);
            assert(x >= 0 && x < width && y >= 0 && y < height);
            winz = 2.0f * depthArray[x + y * width] - 1.0f;
        } else {
            winz = 2.0f * depthArray[i] - 1.0f;
        }
        if (winz != 1.0f) {
            vec4d v = screenToWorld * vec4d(winx, winy, winz, 1.0);
            w->worldPos = v.xyz() / v.w;
        } else {
            w->worldPos = vec3d(UNINITIALIZED, UNINITIALIZED, UNINITIALIZED);
        }
    }
}

void ScreenParticleLayer::initialize()
{
    worldLayer = getOwner()->getLayer<WorldParticleLayer>();
    lifeCycleLayer = getOwner()->getLayer<LifeCycleParticleLayer>();
    assert(worldLayer != NULL);
    assert(lifeCycleLayer != NULL);
}

void ScreenParticleLayer::initParticle(ParticleStorage::Particle *p)
{
    ScreenParticle *s = getScreenParticle(p);
    s->screenPos = vec2f::ZERO;
    s->reason = AGE;
}

ScreenParticleLayer::ScreenParticle** ScreenParticleLayer::getNeighbors(ScreenParticle *s, int &n)
{
    vec2i gridSize = grid->getGridSize();
    vec2i cell = grid->getCell(s->screenPos);
    // here we should have only particles that are in viewport
    cell.x = min(max(0, cell.x), gridSize.x - 1);
    cell.y = min(max(0, cell.y), gridSize.y - 1);
    assert(cell.x >= 0 && cell.x < gridSize.x && cell.y >= 0 && cell.y < gridSize.y);

    n = grid->getCellSize(cell);
    return grid->getCellContent(cell);
}

void ScreenParticleLayer::findNeighborRanges(ScreenParticle *s)
{
    vec2i gridSize = grid->getGridSize();
    vec2i cell = grid->getCell(s->screenPos);
    // here we should have only particles that are in viewport
    assert(cell.x >= 0 && cell.x < gridSize.x && cell.y >= 0 && cell.y < gridSize.y);

    float rangeSqrD = 16.0f * radius * radius;

    int n = grid->getCellSize(cell);
    ScreenParticle **neighbors = grid->getCellContent(cell);
    int count = 0;
    for (int j = 0; j < n; ++j) {
        ScreenParticle *ns = neighbors[j];
        if (ns == s) {
            continue;
        }
        vec2f v = ns->screenPos - s->screenPos;
        float sqrD = v.squaredLength();
        if (sqrD < rangeSqrD) {
            float dist = sqrt(sqrD);
            float angle = atan2(v.y, v.x);
            float theta = safe_acos(0.25f * dist / radius);
            count++;
            ranges->subtract(angle - theta, angle + theta);
        }
    }
}

ScreenParticleLayer::ScreenParticle *ScreenParticleLayer::newScreenParticle(const vec2f &pos)
{
    ParticleStorage::Particle *p = getOwner()->newParticle();
    if (p == NULL) {
        return NULL;
    }

    ScreenParticle *s = getScreenParticle(p);
    s->screenPos = pos;
    grid->addParticle(s, lifeCycleLayer->getIntensity(p));
    return s;
}

void ScreenParticleLayer::getParticleDepths(const vector<ScreenParticle*> &particles)
{
    ptr<FrameBuffer> fb = SceneManager::getCurrentFrameBuffer();
    int width = fb->getViewport().z;
    int height = fb->getViewport().w;

    // we first copy the depth buffer to a texture
    if (!useOffscreenDepthBuffer) {
        if (depthBuffer == NULL || depthBuffer->getWidth() != width || depthBuffer->getHeight() != height) {
            depthBuffer = new Texture2D(width, height, DEPTH_COMPONENT32F, DEPTH_COMPONENT,
                FLOAT, Texture::Parameters().wrapS(CLAMP_TO_EDGE).wrapT(CLAMP_TO_EDGE).min(NEAREST).mag(NEAREST), Buffer::Parameters(), CPUBuffer(NULL));
        }
        fb->copyPixels(0, 0, 0, 0, width, height, *depthBuffer, 0);
    }

    // we set the 'frameBuffer' frame buffer
    if (frameBuffer == NULL) {
        int maxParticles = getOwner()->getStorage()->getCapacity();
        ptr<Texture2D> result = new Texture2D(maxParticles, 1, R32F,
            RED, FLOAT, Texture::Parameters().wrapS(CLAMP_TO_BORDER).wrapT(CLAMP_TO_BORDER).min(NEAREST).mag(NEAREST), Buffer::Parameters(), CPUBuffer(NULL));

        frameBuffer = new FrameBuffer();
        frameBuffer->setReadBuffer(COLOR0);
        frameBuffer->setDrawBuffer(COLOR0);
        frameBuffer->setViewport(vec4<GLint>(0, 0, maxParticles, 1));
        frameBuffer->setTextureBuffer(COLOR0, result, 0);
        frameBuffer->setColorMask(true, false, false, false);
        frameBuffer->setDepthMask(false);
        frameBuffer->setStencilMask(0, 0);

        packer = new Program(new Module(330, packerShader));

        mesh = new Mesh<vec3f, unsigned int>(POINTS, CPU, maxParticles);
        mesh->addAttributeType(0, 3, A32F, false);

        depthTextureU = packer->getUniformSampler("depthTexture");
        sizeU = packer->getUniform3f("size");
    }

    // we then create the mesh, containing one point per particle
    int count = particles.size();
    mesh->clear();
    for (int i = 0; i < count; ++i) {
        ScreenParticle *s = particles[i];
        mesh->addVertex(vec3f(s->screenPos.x, s->screenPos.y, i));
    }

    // we then prepare the uniforms for the packer program
    depthTextureU->set(depthBuffer);
    sizeU->set(vec3f(width, height, getOwner()->getStorage()->getCapacity()));

    // and we draw the mesh with this program
    frameBuffer->draw(packer, *mesh);

    // after that we can read back the framebuffer color attachment
    // to get the depths of the particles. Before that we make sure that
    // depthArray is allocated and of sufficient size
    if (depthArray == NULL || depthArraySize < count) {
        if (depthArray != NULL) {
            delete depthArray;
        }
        depthArraySize = count;
        depthArray = new float[depthArraySize];
    }
    frameBuffer->readPixels(0, 0, count, 1, RED, FLOAT, Buffer::Parameters(), CPUBuffer(depthArray));
}

void ScreenParticleLayer::swap(ptr<ScreenParticleLayer> p)
{
    ParticleLayer::swap(p);
    std::swap(scene, p->scene);
    std::swap(radius, p->radius);
    std::swap(bounds, p->bounds);
    std::swap(grid, p->grid);
    std::swap(ranges, p->ranges);
    std::swap(lastWorldToScreen, p->lastWorldToScreen);
    std::swap(lastViewport, p->lastViewport);
    std::swap(depthBuffer, p->depthBuffer);
    std::swap(depthBufferRead, p->depthBufferRead);
    std::swap(depthArraySize, p->depthArraySize);
    std::swap(depthArray, p->depthArray);
    std::swap(frameBuffer, p->frameBuffer);
    std::swap(depthTextureU, p->depthTextureU);
    std::swap(sizeU, p->sizeU);
    std::swap(mesh, p->mesh);
    std::swap(worldLayer, p->worldLayer);
    std::swap(lifeCycleLayer, p->lifeCycleLayer);
    grid->setParticleRadius(4.0f * radius);
}

class ScreenParticleLayerResource : public ResourceTemplate<50, ScreenParticleLayer>
{
public:
    ScreenParticleLayerResource(ptr<ResourceManager> manager, const string &name, ptr<ResourceDescriptor> desc, const TiXmlElement *e = NULL) :
        ResourceTemplate<50, ScreenParticleLayer>(manager, name, desc)
    {
        e = e == NULL ? desc->descriptor : e;
        checkParameters(desc, e, "name,radius,offscreenDepthBuffer,");

        float radius = 1.0f;

        if (e->Attribute("radius") != NULL) {
            getFloatParameter(desc, e, "radius", &radius);
        }
        ptr<Texture2D> offscreenDepthBuffer = NULL;
        if (e->Attribute("offscreenDepthBuffer") != NULL) {
            offscreenDepthBuffer = manager->loadResource(e->Attribute("offscreenDepthBuffer")).cast<Texture2D>();
        }

        init(radius, offscreenDepthBuffer);
    }

    virtual bool prepareUpdate()
    {
        oldValue = NULL;
        newDesc = NULL;

        return true;
    }
};

extern const char screenParticleLayer[] = "screenParticleLayer";

static ResourceFactory::Type<screenParticleLayer, ScreenParticleLayerResource> ScreenParticleLayerType;

}
