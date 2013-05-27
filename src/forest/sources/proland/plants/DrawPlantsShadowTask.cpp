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

#include "proland/plants/DrawPlantsShadowTask.h"

#include <sstream>

#include "ork/math/mat2.h"
#include "ork/resource/ResourceTemplate.h"
#include "ork/render/FrameBuffer.h"
#include "ork/scenegraph/ShowInfoTask.h"

#include "proland/plants/PlantsProducer.h"

namespace proland
{

ptr<FrameBuffer> createShadowFramebuffer(ptr<Texture2DArray> texture)
{
    int width = texture->getWidth();
    int height = texture->getHeight();
    ptr<FrameBuffer> frameBuffer(new FrameBuffer());
    frameBuffer->setViewport(vec4<GLint>(0, 0, width, height));
    frameBuffer->setReadBuffer(BufferId(0));
    if (texture->getFormat() == DEPTH_COMPONENT) {
        frameBuffer->setDrawBuffer(BufferId(0));
        frameBuffer->setTextureBuffer(DEPTH, texture, 0, -1);
        frameBuffer->setDepthTest(true, LEQUAL);
    } else {
        frameBuffer->setDrawBuffer(COLOR0);
        frameBuffer->setTextureBuffer(COLOR0, texture, 0, -1);
        frameBuffer->setBlend(true, MIN, ONE, ONE);
        frameBuffer->setClearColor(vec4f(1.0f, 1.0f, 1.0f, 1.0f));
    }
    frameBuffer->setPolygonMode(FILL, FILL);
    return frameBuffer;
}

static_ptr< Factory< ptr<Texture2DArray>, ptr<FrameBuffer> > > shadowFramebufferFactory(
    new Factory< ptr<Texture2DArray>, ptr<FrameBuffer> >(createShadowFramebuffer));

DrawPlantsShadowTask::DrawPlantsShadowTask() : AbstractTask("DrawPlantsTask")
{
}

DrawPlantsShadowTask::DrawPlantsShadowTask(const string &terrain, ptr<Plants> plants) :
    AbstractTask("DrawPlantsShadowTask")
{
    init(terrain, plants);
}

void DrawPlantsShadowTask::init(const string &terrain, ptr<Plants> plants)
{
    this->terrain = terrain;
    this->plants = plants;
    this->initialized = false;
}

DrawPlantsShadowTask::~DrawPlantsShadowTask()
{
}

ptr<Task> DrawPlantsShadowTask::getTask(ptr<Object> context)
{
    ptr<SceneNode> n = context.cast<Method>()->getOwner();
    if (producers.empty()) {
        SceneNode::FieldIterator fields = n->getFields();
        while (fields.hasNext()) {
            string name;
            ptr<Object> value = fields.next(name);
            if (name.compare(0, terrain.length(), terrain) == 0) {
                ptr<SceneNode> tn = value.cast<SceneNode>();
                ptr<PlantsProducer> p = PlantsProducer::getPlantsProducer(tn, plants);
                producers.push_back(p);
            }
        }
    }

    return new Impl(this, n);
}

void DrawPlantsShadowTask::swap(ptr<DrawPlantsShadowTask> t)
{
    std::swap(terrain, t->terrain);
    std::swap(plants, t->plants);
    std::swap(producers, t->producers);
    initialized = false;
    t->initialized = false;
}

void DrawPlantsShadowTask::drawPlantsShadow(ptr<SceneNode> context)
{
    if (Logger::DEBUG_LOGGER != NULL) {
        Logger::DEBUG_LOGGER->log("PLANTS", "DrawPlantsShadow");
    }

    vec4d worldSunDir;
    ptr<SceneManager> scene = context.cast<SceneNode>()->getOwner();
    SceneManager::NodeIterator i = scene->getNodes("light");
    if (i.hasNext()) {
        worldSunDir = vec4d(i.next()->getWorldPos(), 0.0);
    }

    box3f b;
    ptr<Program> old = SceneManager::getCurrentProgram();
    SceneManager::setCurrentProgram(plants->selectProg);
    for (unsigned int i = 0; i < producers.size(); ++i) {
        ptr<PlantsProducer> p = producers[i].cast<PlantsProducer>();
        p->produceTiles();
        p->tangentSunDir = (p->tangentFrameToWorld.inverse() * worldSunDir).xyz();
    }
    SceneManager::setCurrentProgram(old);

    if (plants->shadowProg == NULL) {
        return;
    }

    if (!initialized) {
        initialized = true;
        ptr<Texture2DArray> shadowTexture;
        ptr<UniformSampler> shadowSampler = plants->renderProg->getUniformSampler("treeShadowMap");
        if (shadowSampler != NULL) {
            shadowTexture = shadowSampler->get().cast<Texture2DArray>();
        }
        if (shadowTexture == NULL) {
            SceneNode::FieldIterator fields = context->getFields();
            while (fields.hasNext() && shadowTexture == NULL) {
                string name;
                ptr<Object> value = fields.next(name);
                if (name.compare(0, terrain.length(), terrain) == 0) {
                    ptr<SceneNode> tn = value.cast<SceneNode>();
                    if (shadowTexture == NULL) {
                        ptr<Module> m = tn->getModule("material");
                        if (m != NULL) {
                            set<Program*> progs = m->getUsers();
                            set<Program*>::iterator i = progs.begin();
                            while (i != progs.end() && shadowTexture == NULL) {
                                shadowSampler = (*i)->getUniformSampler("treeShadowMap");
                                if (shadowSampler != NULL) {
                                    shadowTexture = shadowSampler->get().cast<Texture2DArray>();
                                }
                                ++i;
                            }
                        }
                    }
                }
            }
        }
        if (shadowTexture != NULL) {
            frameBuffer = shadowFramebufferFactory->get(shadowTexture);
            cameraPosU = plants->shadowProg->getUniform3f("cameraRefPos");
            localToTangentFrameU = plants->shadowProg->getUniformMatrix4f("localToTangentFrame");
            tangentFrameToScreenU = plants->shadowProg->getUniformMatrix4f("tangentFrameToScreen");
            shadowLimitU = plants->shadowProg->getUniform4f("shadowLimit");
            shadowCutsU = plants->shadowProg->getUniform4f("shadowCuts");
            for (int i = 0; i < MAX_SHADOW_MAPS; ++i) {
                ostringstream name;
                name << "tangentFrameToShadow[" << i << "]";
                tangentFrameToShadowU[i] = plants->shadowProg->getUniformMatrix4f(name.str());
            }
            tangentSunDirU = plants->shadowProg->getUniform3f("tangentSunDir");
            focalPosU = plants->shadowProg->getUniform3f("focalPos");
            plantRadiusU = plants->shadowProg->getUniform1f("plantRadius");
        }
    }

    int pid = 0;
    int maxSize = 0;
    for (int i = 0; i < producers.size(); ++i) {
        int size = producers[i].cast<PlantsProducer>()->plantBounds.size();
        if (size > maxSize) {
            maxSize = size;
            pid = i;
        }
    }

    if (maxSize > 0 && frameBuffer != NULL) {
        ptr<PlantsProducer> p = producers[pid].cast<PlantsProducer>();
        mat4d m = p->tangentFrameToWorld;

        double near = p->plantBounds[0].x;
        double far = p->plantBounds[0].y;
        for (unsigned int i = 1; i < p->plantBounds.size(); ++i) {
            near = std::min(near, p->plantBounds[i].x);
            far = std::max(far, p->plantBounds[i].y);
        }
        far = std::min(far, 0.8 * plants->getMaxDistance());
        if (far <= near) {
            return;
        }

        int sliceCount = 0;
        if (frameBuffer->getTextureBuffer(DEPTH) != NULL) {
            sliceCount = frameBuffer->getTextureBuffer(DEPTH).cast<Texture2DArray>()->getLayers();
        } else {
            sliceCount = frameBuffer->getTextureBuffer(COLOR0).cast<Texture2DArray>()->getLayers();
        }
        assert(sliceCount <= MAX_SHADOW_MAPS);

        if (far - near < 100.0) {
            sliceCount = 1;
        }

        double zLimits[MAX_SHADOW_MAPS + 1];
        double zCuts[MAX_SHADOW_MAPS + 1];
        bool zSlice[MAX_SHADOW_MAPS + 1];
        for (int slice = 0; slice <= MAX_SHADOW_MAPS; ++slice) {
            zLimits[slice] = -1.0;
            zCuts[slice] = -1.0;
            zSlice[slice] = false;
        }
        for (int slice = 0; slice <= sliceCount; ++slice) {
            double z = near * pow(far / near, float(slice) / sliceCount);
            vec4d p = scene->getCameraToScreen() * vec4d(0.0, 0.0, -z, 1.0);
            zLimits[slice] = p.xyzw().z;
            zCuts[slice] = p.w;
        }

        // temporary reference frame for shadow map
        vec3d zDir = p->tangentSunDir;
        vec3d xDir = vec3d(-zDir.y, zDir.x, 0.0).normalize();
        vec3d yDir = zDir.crossProduct(xDir);
        vec3d uDir = xDir;
        vec3d vDir = yDir;
        mat3d s = mat3d(uDir.x, uDir.y, uDir.z, vDir.x, vDir.y, vDir.z, zDir.x, zDir.y, zDir.z);

        box3d b = p->plantBox;
        double smax = -INFINITY;
        for (int i = 0; i < 8; ++i) {
            vec3d c(i%2 == 0 ? b.xmin : b.xmax, (i/2)%2 == 0 ? b.ymin : b.ymax, (i/4)%2 == 0 ? b.zmin : b.zmax);
            double sz = zDir.dotproduct(c);
            smax = std::max(smax, sz);
        }

        mat4d t = p->tangentFrameToScreen.inverse();
        const int segments[24] = { 0, 1, 1, 3, 3, 2, 2, 0, 4, 5, 5, 7, 7, 6, 6, 4, 0, 4, 1, 5, 3, 7, 2, 6 };

        for (int slice = 0; slice < sliceCount; ++slice) {
            double zi = zCuts[sliceCount - slice - 1];
            double zj = zCuts[sliceCount - slice];
            double fi = zLimits[sliceCount - slice - 1];
            double fj = zLimits[sliceCount - slice];

            double zmin = +INFINITY;
            double zmax = -INFINITY;
            for (unsigned int i = 0; i < p->plantBounds.size(); ++i) {
                vec4d h = p->plantBounds[i];
                if (h.x <= zj && h.y >= zi) {
                    zmin = std::min(zmin, h.z);
                    zmax = std::max(zmax, h.w);
                }
            }

            vector<vec3d> pts;
            vec3d cf[8];
            bool inside[8];
            for (int i = 0; i < 8; ++i) {
                cf[i] = (t * vec4d(i%2 == 0 ? -1.0 : 1.0, (i/2)%2 == 0 ? -1.0 : 1.0, (i/4)%2 == 0 ? fi : fj, 1.0)).xyzw();
                inside[i] = cf[i].z >= zmin && cf[i].z <= zmax;
                if (inside[i]) {
                    pts.push_back(cf[i]);
                }
            }
            for (int i = 0; i < 24; i += 2) {
                int pointA = segments[i];
                int pointB = segments[i + 1];
                if (!inside[pointA] || !inside[pointB]) {
                    vec3d pA = cf[pointA];
                    vec3d pB = cf[pointB];
                    vec3d AB = pB - pA;
                    double tOut = 1.0;
                    double tIn = 0.0;
                    tIn = std::max(tIn, ((AB.z > 0.0 ? zmin : zmax) - pA.z) / AB.z);
                    tOut = std::min(tOut, ((AB.z > 0.0 ? zmax : zmin) - pA.z) / AB.z);
                    if (tIn < tOut && tIn < 1.0 && tOut > 0.0) {
                        if (tIn > 0.0) {
                            pts.push_back(pA + AB * tIn);
                        }
                        if (tOut < 1.0) {
                            pts.push_back(pA + AB * tOut);
                        }
                    }
                }
            }

            if (zmin < zmax && pts.size() > 2) {
                box3d t;
                for (unsigned int i = 0; i < pts.size(); ++i) {
                    t = t.enlarge(s * pts[i]);
                }
                t.zmax = std::max(t.zmax, smax);
                // tangent frame to shadow
                mat4d ttos = mat4d::orthoProjection(t.xmax, t.xmin, t.ymax, t.ymin, -t.zmax, -t.zmin) * mat4d(s);
                tangentFrameToShadowU[slice]->setMatrix(mat4f(ttos[0][0], ttos[0][1], ttos[0][2], ttos[0][3],
                                                              ttos[1][0], ttos[1][1], ttos[1][2], ttos[1][3],
                                                              ttos[2][0], ttos[2][1], ttos[2][2], ttos[2][3],
                                                              0.0, 0.0, 0.0, 1.0 / (t.zmax - t.zmin)));

                zSlice[slice] = true;
            }
        }

        shadowLimitU->set(vec4f(zLimits[1], zLimits[2], zLimits[3], zLimits[4]) * 0.5 + vec4f(0.5, 0.5, 0.5, 0.5));
        shadowCutsU->set(vec4f(zCuts[1], zCuts[2], zCuts[3], zCuts[4]));

        frameBuffer->clear(true, false, true);
        for (unsigned int i = 0; i < producers.size(); ++i) {
            ptr<PlantsProducer> p = producers[i].cast<PlantsProducer>();
            if (p->count == 0) {
                continue;
            }
            if (cameraPosU != NULL) {
                cameraPosU->set(vec3f(p->localCameraPos.x - p->cameraRefPos.x, p->localCameraPos.y - p->cameraRefPos.y, 0.0));
            }
            localToTangentFrameU->setMatrix(p->localToTangentFrame.cast<float>());
            tangentFrameToScreenU->setMatrix(p->tangentFrameToScreen.cast<float>());
            tangentSunDirU->set(-p->tangentSunDir.cast<float>());
            if (focalPosU != NULL) {
                vec2d cgDir = (p->cameraToTangentFrame * vec4d(0.0, 0.0, 1.0, 0.0)).xyz().xy().normalize(1000.0);
                focalPosU->set(vec3f(cgDir.x, cgDir.y, p->tangentCameraPos.z));
            }
            if (plantRadiusU != NULL) {
                plantRadiusU->set(plants->getPoissonRadius() * p->terrain->root->l / (1 << plants->getMaxLevel()));
            }
            frameBuffer->multiDraw(plants->shadowProg, *(p->getPlantsMesh()), POINTS, p->offsets, p->sizes, p->count);
        }
    }
}

DrawPlantsShadowTask::Impl::Impl(ptr<DrawPlantsShadowTask> owner, ptr<SceneNode> context) :
    Task("DrawPlantsShadow", true, 0), owner(owner), context(context)
{
}

DrawPlantsShadowTask::Impl::~Impl()
{
}

bool DrawPlantsShadowTask::Impl::run()
{
    owner->drawPlantsShadow(context);
    return true;
}

class DrawPlantsShadowTaskResource : public ResourceTemplate<50, DrawPlantsShadowTask>
{
public:
    DrawPlantsShadowTaskResource(ptr<ResourceManager> manager, const string &name, ptr<ResourceDescriptor> desc, const TiXmlElement *e = NULL) :
        ResourceTemplate<50, DrawPlantsShadowTask>(manager, name, desc)
    {
        e = e == NULL ? desc->descriptor : e;
        checkParameters(desc, e, "name,plants,");
        string n = getParameter(desc, e, "name");
        ptr<Plants> plants = manager->loadResource(getParameter(desc, e, "plants")).cast<Plants>();
        init(n, plants);
    }

    virtual bool prepareUpdate()
    {
        bool changed = false;
        if (Resource::prepareUpdate()) {
            changed = true;
        } else if (dynamic_cast<Resource*>(plants.get())->changed()) {
            changed = true;
        }

        if (changed) {
            oldValue = NULL;
            try {
                oldValue = new DrawPlantsShadowTaskResource(manager, name, newDesc == NULL ? desc : newDesc);
            } catch (...) {
            }
            if (oldValue != NULL) {
                swap(oldValue);
                return true;
            }
            return false;
        }
        return true;
    }
};

extern const char drawPlantsShadow[] = "drawPlantsShadow";

static ResourceFactory::Type<drawPlantsShadow, DrawPlantsShadowTaskResource> DrawPlantsShadowTaskType;

}
