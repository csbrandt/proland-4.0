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

#include "proland/ocean/DrawOceanTask.h"

#include "ork/core/Timer.h"
#include "ork/resource/ResourceTemplate.h"
#include "ork/render/FrameBuffer.h"
#include "ork/scenegraph/SceneManager.h"
#include "proland/terrain/TileSampler.h"
#include "proland/math/noise.h"

using namespace std;
using namespace ork;

namespace proland
{

DrawOceanTask::DrawOceanTask() : AbstractTask("DrawOceanTask")
{
}

DrawOceanTask::DrawOceanTask(float radius, float zmin, ptr<Module> brdfShader) :
    AbstractTask("DrawOceanTask")
{
    init(radius, zmin, brdfShader);
}

void DrawOceanTask::init(float radius, float zmin, ptr<Module> brdfShader)
{
    this->radius = radius;
    this->zmin = zmin;
    this->nbWaves = 60;
    this->lambdaMin = 0.02f;
    this->lambdaMax = 30.0;
    this->heightMax = 0.4f;//0.5;
    this->seaColor = vec3f(10.f / 255.f, 40.f / 255.f, 120.f / 255.f) * 0.1f;
    this->resolution = 8;
    this->oldLtoo = mat4d::IDENTITY;
    this->offset = vec3d::ZERO;
    this->brdfShader = brdfShader;
    this->nbWavesU = NULL;
}

DrawOceanTask::~DrawOceanTask()
{
}

#define srnd() (2*frandom(&seed) - 1)

void DrawOceanTask::generateWaves()
{
    long seed = 1234567;
    float min = log(lambdaMin) / log(2.0f);
    float max = log(lambdaMax) / log(2.0f);

    vec4f *waves = new vec4f[nbWaves];

    sigmaXsq = 0.0;
    sigmaYsq = 0.0;
    meanHeight = 0.0;
    heightVariance = 0.0;
    amplitudeMax = 0.0;

#define nbAngles 5 // impair
#define angle(i)  (1.5*(((i)%nbAngles)/(float)(nbAngles/2)-1))
#define dangle()   (1.5/(float)(nbAngles/2))
    float Wa[nbAngles]; // normalised gaussian samples
    int index[nbAngles]; // to hash angle order
    float s=0;
    for (int i=0; i<nbAngles; i++) {
        index[i]=i;
        float a = angle(i); // (i/(float)(nbAngle/2)-1)*1.5;
        s += Wa[i] = exp(-.5*a*a);
    }
    for (int i=0; i<nbAngles; i++) {
        Wa[i] /= s;
    }

    const float waveDispersion = 0.9f;//6;
    const float U0 = 10.0f;
    const int spectrumType = 2;

    for (int i = 0; i < nbWaves; ++i) {
        float x = i / (nbWaves - 1.0f);

        float lambda = pow(2.0f, (1.0f - x) * min + x * max);
        float ktheta = grandom(0.0f, 1.0f, &seed) * waveDispersion;
        float knorm = 2.0f * M_PI_F / lambda;
        float omega = sqrt(9.81f * knorm);
        float amplitude;

        if (spectrumType == 1) {
            amplitude = heightMax * grandom(0.5f, 0.15f, &seed) / (knorm * lambdaMax / (2.0f * M_PI));
        } else if (spectrumType == 2) {
            float step = (max-min)/(nbWaves-1); // dlambda/di
            float omega0 = 9.81f / U0; // 100.0;
            if ((i%(nbAngles))==0) { // scramble angle ordre
                for (int k=0; k<nbAngles; k++) {   // do N swap in indices
                    int n1=lrandom(&seed)%nbAngles, n2=lrandom(&seed)%nbAngles,n;
                    n=index[n1]; index[n1]=index[n2]; index[n2]=n;
                }
            }
            ktheta = waveDispersion*(angle(index[(i)%nbAngles])+.4*srnd()*dangle());
            ktheta *= 1/(1+40*pow(omega0/omega,4));
            amplitude = (8.1e-3*9.81*9.81) / pow(omega,5) * exp(-0.74*pow(omega0/omega,4));
            amplitude *= .5*sqrt(2*3.14*9.81/lambda)*nbAngles*step; // (2/step-step/2);
            amplitude = 3*heightMax*sqrt(amplitude);
        }

        // cull breaking trochoids ( d(x+Acos(kx))=1-Akcos(); must be >0 )
        if (amplitude > 1.0f / knorm) {
            amplitude = 1.0f / knorm;
        } else if (amplitude < -1.0f / knorm) {
            amplitude = -1.0f / knorm;
        }

        waves[i].x = amplitude;
        waves[i].y = omega;
        waves[i].z = knorm * cos(ktheta);
        waves[i].w = knorm * sin(ktheta);
        sigmaXsq += pow(cos(ktheta), 2.0f) * (1.0f - sqrt(1.0f - knorm * knorm * amplitude * amplitude));
        sigmaYsq += pow(sin(ktheta), 2.0f) * (1.0f - sqrt(1.0f - knorm * knorm * amplitude * amplitude));
        meanHeight -= knorm * amplitude * amplitude * 0.5f;
        heightVariance += amplitude * amplitude * (2.0f - knorm * knorm * amplitude * amplitude) * 0.25f;
        amplitudeMax += fabs(amplitude);
    }

    float var = 4.0f;
    float h0 = meanHeight - var * sqrt(heightVariance);
    float h1 = meanHeight + var * sqrt(heightVariance);
    amplitudeMax = h1 - h0;

    ptr<Texture1D> wavesTexture = new Texture1D(nbWaves, RGBA32F, RGBA,
            FLOAT, Texture::Parameters().wrapS(CLAMP_TO_BORDER).min(NEAREST).mag(NEAREST),
            Buffer::Parameters(), CPUBuffer(waves));
    delete[] waves;

    nbWavesU->set(nbWaves);
    wavesU->set(wavesTexture);

    if (brdfShader != NULL) {
        assert(!brdfShader->getUsers().empty());
        Program *prog = *(brdfShader->getUsers().begin());
        prog->getUniform1f("seaRoughness")->set(sigmaXsq);
        prog->getUniform3f("seaColor")->set(seaColor);
    }
}

ptr<Task> DrawOceanTask::getTask(ptr<Object> context)
{
    ptr<SceneNode> n = context.cast<Method>()->getOwner();
    return new Impl(n, this);
}

void DrawOceanTask::swap(ptr<DrawOceanTask> t)
{
    std::swap(*this, *t);
}

DrawOceanTask::Impl::Impl(ptr<SceneNode> n, ptr<DrawOceanTask> owner) :
    Task("DrawOcean", true, 0), n(n), o(owner)
{
}

DrawOceanTask::Impl::~Impl()
{
}

bool DrawOceanTask::Impl::run()
{
    if (Logger::DEBUG_LOGGER != NULL) {
        Logger::DEBUG_LOGGER->log("OCEAN", "DrawOcean");
    }
    ptr<FrameBuffer> fb = SceneManager::getCurrentFrameBuffer();
    ptr<Program> prog = SceneManager::getCurrentProgram();

    if (o->nbWavesU == NULL) {
        o->nbWavesU = prog->getUniform1f("nbWaves");
        o->wavesU = prog->getUniformSampler("wavesSampler");
        o->cameraToOceanU = prog->getUniformMatrix4f("cameraToOcean");
        o->screenToCameraU = prog->getUniformMatrix4f("screenToCamera");
        o->cameraToScreenU = prog->getUniformMatrix4f("cameraToScreen");
        o->oceanToCameraU = prog->getUniformMatrix3f("oceanToCamera");
        o->oceanToWorldU = prog->getUniformMatrix4f("oceanToWorld");
        o->oceanCameraPosU = prog->getUniform3f("oceanCameraPos");
        o->oceanSunDirU = prog->getUniform3f("oceanSunDir");
        o->horizon1U = prog->getUniform3f("horizon1");
        o->horizon2U = prog->getUniform3f("horizon2");
        o->timeU = prog->getUniform1f("time");
        o->radiusU = prog->getUniform1f("radius");
        o->heightOffsetU = prog->getUniform1f("heightOffset");
        o->lodsU = prog->getUniform4f("lods");

        assert(o->nbWavesU != NULL);
        o->generateWaves();
    }

    vector< ptr<TileSampler> > uniforms;
    SceneNode::FieldIterator ui = n->getFields();
    while (ui.hasNext()) {
        ptr<TileSampler> u = ui.next().cast<TileSampler>();
        if (u != NULL && u->getTerrain(0) != NULL) {
            u->setTileMap();
        }
    }

    // compute ltoo = localToOcean transform, where ocean frame = tangent space at
    // camera projection on sphere o->radius in local space
    mat4d ctol = n->getLocalToCamera().inverse();
    vec3d cl = ctol * vec3d::ZERO; // camera in local space

    if ((o->radius == 0.0 && cl.z > o->zmin) ||
        (o->radius > 0.0 && cl.length() > o->radius + o->zmin) ||
        (o->radius < 0.0 && vec2d(cl.y, cl.z).length() < -o->radius - o->zmin))
    {
        o->oldLtoo = mat4d::IDENTITY;
        o->offset = vec3d::ZERO;
        return true;
    }

    vec3d ux, uy, uz, oo;

    if (o->radius == 0.0) {
        // flat ocean
        ux = vec3d::UNIT_X;
        uy = vec3d::UNIT_Y;
        uz = vec3d::UNIT_Z;
        oo = vec3d(cl.x, cl.y, 0.0);
    } else if (o->radius > 0.0) {
        // spherical ocean
        uz = cl.normalize(); // unit z vector of ocean frame, in local space
        if (o->oldLtoo != mat4d::IDENTITY) {
            ux = vec3d(o->oldLtoo[1][0], o->oldLtoo[1][1], o->oldLtoo[1][2]).crossProduct(uz).normalize();
        } else {
            ux = vec3d::UNIT_Z.crossProduct(uz).normalize();
        }
        uy = uz.crossProduct(ux); // unit y vector
        oo = uz * o->radius; // origin of ocean frame, in local space
    } else {
        // cylindrical ocean
        uz = vec3d(0.0, -cl.y, -cl.z).normalize();
        ux = vec3d::UNIT_X;
        uy = uz.crossProduct(ux);
        oo = vec3d(cl.x, 0.0, 0.0) + uz * o->radius;
    }

    mat4d ltoo = mat4d(
        ux.x, ux.y, ux.z, -ux.dotproduct(oo),
        uy.x, uy.y, uy.z, -uy.dotproduct(oo),
        uz.x, uz.y, uz.z, -uz.dotproduct(oo),
        0.0,  0.0,  0.0,  1.0);
    // compute ctoo = cameraToOcean transform
    mat4d ctoo = ltoo * ctol;

    if (o->oldLtoo != mat4d::IDENTITY) {
        vec3d delta = ltoo * (o->oldLtoo.inverse() * vec3d::ZERO);
        o->offset += delta;
    }
    o->oldLtoo = ltoo;

    mat4d ctos = n->getOwner()->getCameraToScreen();
    mat4d stoc = ctos.inverse();
    vec3d oc = ctoo * vec3d::ZERO;

    if (o->oceanSunDirU != NULL) {
        // TODO how to get sun dir in a better way?
        SceneManager::NodeIterator i = n->getOwner()->getNodes("light");
        if (i.hasNext()) {
            ptr<SceneNode> l = i.next();
            vec3d worldSunDir = l->getLocalToParent() * vec3d::ZERO;
            vec3d oceanSunDir = ltoo.mat3x3() * (n->getWorldToLocal().mat3x3() * worldSunDir);
            o->oceanSunDirU->set(oceanSunDir.cast<float>());
        }
    }

    vec4<GLint> screen = fb->getViewport();

    vec4d frustum[6];
    SceneManager::getFrustumPlanes(ctos, frustum);
    vec3d left = frustum[0].xyz().normalize();
    vec3d right = frustum[1].xyz().normalize();
    float fov = (float) safe_acos(-left.dotproduct(right));
    float pixelSize = atan(tan(fov / 2.0f) / (screen.w / 2.0f)); // angle under which a screen pixel is viewed from the camera

    o->cameraToOceanU->setMatrix(ctoo.cast<float>());
    o->screenToCameraU->setMatrix(stoc.cast<float>());
    o->cameraToScreenU->setMatrix(ctos.cast<float>());
    o->oceanToCameraU->setMatrix(ctoo.inverse().mat3x3().cast<float>());
    o->oceanCameraPosU->set(vec3f(float(-o->offset.x), float(-o->offset.y), float(oc.z)));
    if (o->oceanToWorldU != NULL) {
        o->oceanToWorldU->setMatrix((n->getLocalToWorld() * ltoo.inverse()).cast<float>());
    }

    if (o->horizon1U != NULL) {
        float h = oc.z;
        vec3d A0 = (ctoo * vec4d((stoc * vec4d(0.0, 0.0, 0.0, 1.0)).xyz(), 0.0)).xyz();
        vec3d dA = (ctoo * vec4d((stoc * vec4d(1.0, 0.0, 0.0, 0.0)).xyz(), 0.0)).xyz();
        vec3d B = (ctoo * vec4d((stoc * vec4d(0.0, 1.0, 0.0, 0.0)).xyz(), 0.0)).xyz();
        if (o->radius == 0.0) {
            o->horizon1U->set(vec3f(-(h * 1e-6 + A0.z) / B.z, -dA.z / B.z, 0.0));
            o->horizon2U->set(vec3f::ZERO);
        } else {
            double h1 = h * (h + 2.0 * o->radius);
            double h2 = (h + o->radius) * (h + o->radius);
            double alpha = B.dotproduct(B) * h1 - B.z * B.z * h2;
            double beta0 = (A0.dotproduct(B) * h1 - B.z * A0.z * h2) / alpha;
            double beta1 = (dA.dotproduct(B) * h1 - B.z * dA.z * h2) / alpha;
            double gamma0 = (A0.dotproduct(A0) * h1 - A0.z * A0.z * h2) / alpha;
            double gamma1 = (A0.dotproduct(dA) * h1 - A0.z * dA.z * h2) / alpha;
            double gamma2 = (dA.dotproduct(dA) * h1 - dA.z * dA.z * h2) / alpha;
            o->horizon1U->set(vec3f(-beta0, -beta1, 0.0));
            o->horizon2U->set(vec3f(beta0 * beta0 - gamma0, 2.0 * (beta0 * beta1 - gamma1), beta1 * beta1 - gamma2));
        }
    }

    o->timeU->set(n->getOwner()->getTime() * 1e-6);
    if (o->radiusU != NULL) {
        o->radiusU->set(o->radius < 0.0 ? -o->radius : o->radius);
    }
    o->heightOffsetU->set(-o->meanHeight);
    o->lodsU->set(vec4f(o->resolution,
            pixelSize * o->resolution,
            log(o->lambdaMin) / log(2.0f),
            (o->nbWavesU->get() - 1.0f) / (log(o->lambdaMax) / log(2.0f) -  log(o->lambdaMin) / log(2.0f))));

    if (o->screenGrid == NULL || o->screenWidth != screen.z || o->screenHeight != screen.w) {
        o->screenWidth = screen.z;
        o->screenHeight = screen.w;
        o->screenGrid = new Mesh<vec2f, unsigned int>(TRIANGLES, GPU_STATIC);
        o->screenGrid->addAttributeType(0, 2, A32F, false);

        float f = 1.25f;
        int NX = int(f * screen.z / o->resolution);
        int NY = int(f * screen.w / o->resolution);
        for (int i = 0; i < NY; ++i) {
            for (int j = 0; j < NX; ++j) {
                o->screenGrid->addVertex(vec2f(2.0*f*j/(NX-1.0f)-f, 2.0*f*i/(NY-1.0f)-f));
            }
        }
        for (int i = 0; i < NY-1; ++i) {
            for (int j = 0; j < NX-1; ++j) {
                o->screenGrid->addIndice(i*NX+j);
                o->screenGrid->addIndice(i*NX+j+1);
                o->screenGrid->addIndice((i+1)*NX+j);
                o->screenGrid->addIndice((i+1)*NX+j);
                o->screenGrid->addIndice(i*NX+j+1);
                o->screenGrid->addIndice((i+1)*NX+j+1);
            }
        }
    }

    fb->draw(prog, *(o->screenGrid));

    return true;
}

class DrawOceanTaskResource : public ResourceTemplate<40, DrawOceanTask>
{
public:
    DrawOceanTaskResource(ptr<ResourceManager> manager, const string &name, ptr<ResourceDescriptor> desc, const TiXmlElement *e = NULL) :
        ResourceTemplate<40, DrawOceanTask>(manager, name, desc)
    {
        e = e == NULL ? desc->descriptor : e;
        checkParameters(desc, e, "name,radius,zmin,brdfShader,");
        float radius;
        float zmin;
        ptr<Module> brdfShader;
        if (e->Attribute("brdfShader") != NULL) {
            brdfShader = manager->loadResource(getParameter(desc, e, "brdfShader")).cast<Module>();
        }
        getFloatParameter(desc, e, "radius", &radius);
        getFloatParameter(desc, e, "zmin", &zmin);
        init(radius, zmin, brdfShader);
    }
};

extern const char drawOcean[] = "drawOcean";

static ResourceFactory::Type<drawOcean, DrawOceanTaskResource> DrawOceanTaskType;

}
