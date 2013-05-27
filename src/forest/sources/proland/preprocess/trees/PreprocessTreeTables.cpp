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

#include "proland/preprocess/trees/PreprocessTree.h"

#include <stdlib.h>

#include "ork/core/FileLogger.h"
#include "ork/render/FrameBuffer.h"
#include "ork/resource/XMLResourceLoader.h"
#include "ork/scenegraph/SceneManager.h"
#include "ork/ui/GlutWindow.h"

#include "proland/ui/BasicViewHandler.h"
#include "proland/terrain/ReadbackManager.h"
#include "proland/plants/Plants.h"

#include "proland/preprocess/trees/drawPlantsMethod.xml"
#include "proland/preprocess/trees/drawPlantsShadowMethod.xml"
#include "proland/preprocess/trees/globalsShader.glhl"
#include "proland/preprocess/trees/globalsShader.glsl"
#include "proland/preprocess/trees/helloworld.xml"
#include "proland/preprocess/trees/quad.mesh"
#include "proland/preprocess/trees/renderTreeShader3D.glsl"
#include "proland/preprocess/trees/renderTreeShadow3D.glsl"
#include "proland/preprocess/trees/selectTreeShader.glsl"
#include "proland/preprocess/trees/terrainShader.glsl"
#include "proland/preprocess/trees/treeInfo3D.glsl"

using namespace ork;
using namespace proland;

namespace proland
{

const int WIDTH = 800;
const int HEIGHT = 800;

const int NTHETA = 16;
const int NPHI = 16;
const int NLAMBDA = 8;
const int NA = 32;

float *pass1 = NULL;
float *pass2 = NULL;
float *pass3 = NULL;

vec2i getBounds(int thetav)
{
    assert(WIDTH == 800 && HEIGHT == 800 && NTHETA == 16);
    if (thetav == NTHETA - 1) {
        return vec2i(395, 460);
    } else if (thetav == NTHETA - 2) {
        return vec2i(74, 770);
    }
    return vec2i(0, 800);
}

class MyCallback1 : public ReadbackManager::Callback
{
public:
    int thetav, thetal, phi, lambda;

    MyCallback1(int thetav, int thetal, int phi, int lambda) :
        thetav(thetav), thetal(thetal), phi(phi), lambda(lambda)
    {
    }

    void dataRead(volatile void *data)
    {
        unsigned char *pixels = (unsigned char*) data;
        vec2i bounds = getBounds(thetav);
        float r = 0;
        float g = 0;
        float b = 0;
        int n = 0;
        for (int j = bounds.x; j < bounds.y; ++j) {
            for (int i = 0; i < WIDTH; ++i) {
                int off = 4*(i + j * WIDTH);
                r += pixels[off++] / 255.0;
                g += pixels[off++] / 255.0;
                b += pixels[off] / 255.0;
                n += 1;
            }
        }

        int off = (thetav + thetal * NTHETA + phi * NTHETA * NTHETA + lambda * NTHETA * NTHETA * NPHI) * 4;
        pass1[off++] = r;
        pass1[off++] = g;
        pass1[off++] = b;
        pass1[off] = n;
    }
};

class MyCallback2 : public ReadbackManager::Callback
{
public:
    int thetav, lambda;

    MyCallback2(int thetav, int lambda) : thetav(thetav), lambda(lambda)
    {
    }

    void dataRead(volatile void *data)
    {
        unsigned char *pixels = (unsigned char*) data;
        vec2i bounds = getBounds(thetav);
        float r = 0;
        float g = 0;
        float b = 0;
        int n = 0;
        for (int j = bounds.x; j < bounds.y; ++j) {
            for (int i = 0; i < WIDTH; ++i) {
                int off = 4*(i + j * WIDTH);
                r += pixels[off++] / 255.0;
                g += pixels[off++] / 255.0;
                b += pixels[off] / 255.0;
                n += 1;
            }
        }

        int off = (thetav + lambda * NTHETA) * 4;
        pass2[off++] = r;
        pass2[off++] = g;
        pass2[off++] = b;
        pass2[off] = n;
    }
};

class MyCallback3 : public ReadbackManager::Callback
{
public:
    int thetav;

    MyCallback3(int thetav) : thetav(thetav)
    {
    }

    void dataRead(volatile void *data)
    {
        unsigned char *pixels = (unsigned char*) data;
        vec2i bounds = getBounds(thetav);
        float r = 0;
        float g = 0;
        float b = 0;
        int n = 0;
        for (int j = bounds.x; j < bounds.y; ++j) {
            for (int i = 0; i < WIDTH; ++i) {
                int off = 4*(i + j * WIDTH);
                r += pixels[off++] / 255.0;
                g += pixels[off++] / 255.0;
                b += pixels[off] / 255.0;
                n += 1;
            }
        }

        int off = thetav * 4;
        pass3[off++] = r;
        pass3[off++] = g;
        pass3[off++] = b;
        pass3[off] = n;
    }
};

class TreeResourceLoader : public XMLResourceLoader
{
public:
    virtual string findFile(const TiXmlElement *desc, const vector<string> paths, const string &file)
    {
        return file;
    }

    virtual unsigned char *loadFile(const string &file, unsigned int &size)
    {
        if (file == "./drawPlantsMethod.xml") {
            size = strlen(drawPlantsMethodSource);
            return (unsigned char*) drawPlantsMethodSource;
        } else if (file == "./drawPlantsShadowMethod.xml") {
            size = strlen(drawPlantsShadowMethodSource);
            return (unsigned char*) drawPlantsShadowMethodSource;
        } else if (file == "globalsShader.glhl") {
            size = strlen(globalsShaderHeaderSource);
            return (unsigned char*) globalsShaderHeaderSource;
        } else if (file == "globalsShader.glsl") {
            size = strlen(globalsShaderSource);
            return (unsigned char*) globalsShaderSource;
        } else if (file == "helloworld.xml") {
            size = strlen(helloWorldSource);
            return (unsigned char*) helloWorldSource;
        } else if (file == "quad.mesh") {
            size = strlen(quadSource);
            return (unsigned char*) quadSource;
        } else if (file == "renderTreeShader3D.glsl") {
            size = strlen(renderTreeSource);
            return (unsigned char*) renderTreeSource;
        } else if (file == "renderTreeShadow3D.glsl") {
            size = strlen(renderTreeShadowSource);
            return (unsigned char*) renderTreeShadowSource;
        } else if (file == "selectTreeShader.glsl") {
            size = strlen(selectTreeSource);
            return (unsigned char*) selectTreeSource;
        } else if (file == "terrainShader.glsl") {
            size = strlen(terrainShaderSource);
            return (unsigned char*) terrainShaderSource;
        } else if (file == "treeInfo3D.glsl") {
            size = strlen(treeInfoSource);
            return (unsigned char*) treeInfoSource;
        }
        return NULL;
    }

    virtual void getTimeStamp(const string &file, time_t &t)
    {
        if (file == "./drawPlantsMethod.xml" ||
            file == "./drawPlantsShadowMethod.xml" ||
            file == "globalsShader.glhl" ||
            file == "globalsShader.glsl" ||
            file == "helloworld.xml" ||
            file == "quad.mesh" ||
            file == "renderTreeShader3D.glsl" ||
            file == "renderTreeShadow3D.glsl" ||
            file == "selectTreeShader.glsl" ||
            file == "terrainShader.glsl" ||
            file == "treeInfo3D.glsl")
        {
            t = 1;
        } else {
            t = 0;
        }
    }
};

class PreprocessTreeTables : public GlutWindow
{
public:
    ptr<SceneManager> manager;
    ptr<TerrainViewController> controller;
    ptr<Module> globals;
    ptr<Module> render;
    ptr<Program> terrain;
    float treeHeight;

    int thetav;
    int thetal;
    int phi;
    int lambda;
    int pass;

    ptr<ReadbackManager> readback;

    const char* output;

    PreprocessTreeTables(float minRadius, float maxRadius, float treeHeight, float treeTau, int nViews, loadTreeViewsFunction tree, const char* output) :
        GlutWindow(Window::Parameters().size(WIDTH, HEIGHT).depth(true)), treeHeight(treeHeight), output(output)
    {
        FileLogger::File *out = new FileLogger::File("log.html");
        Logger::INFO_LOGGER = new FileLogger("INFO", out, Logger::INFO_LOGGER);
        Logger::WARNING_LOGGER = new FileLogger("WARNING", out, Logger::WARNING_LOGGER);
        Logger::ERROR_LOGGER = new FileLogger("ERROR", out, Logger::ERROR_LOGGER);

        ptr<Texture2DArray> t = tree();

        ptr<XMLResourceLoader> resLoader = new TreeResourceLoader();
        resLoader->addPath(".");
        resLoader->addArchive("helloworld.xml");

        ptr<ResourceManager> resManager = new ResourceManager(resLoader, 8);
        setViews(resManager->loadResource("renderTreeShader3D").cast<Module>(), nViews, t);
        setViews(resManager->loadResource("renderTreeShadow3D").cast<Module>(), nViews, t);

        manager = new SceneManager();
        manager->setResourceManager(resManager);

        manager->setScheduler(resManager->loadResource("defaultScheduler").cast<Scheduler>());
        manager->setRoot(resManager->loadResource("scene").cast<SceneNode>());
        manager->setCameraNode("camera");
        manager->setCameraMethod("draw");

        controller = new TerrainViewController(manager->getCameraNode(), 50000.0);

        globals = resManager->loadResource("globalsShaderFS").cast<Module>();
        render = resManager->loadResource("renderTreeShader3D").cast<Module>();
        terrain = resManager->loadResource("globalsShaderFS;terrainShader;").cast<Program>();

        ptr<Plants> trees = resManager->loadResource("trees").cast<Plants>();
        terrain->getUniform1f("plantRadius")->set(trees->getPoissonRadius() * 100000.0 / (1 << trees->getMaxLevel()));

        ptr<Program> p = *(globals->getUsers().begin());
        p->getUniform1f("minRadius")->set(minRadius);
        p->getUniform1f("maxRadius")->set(maxRadius);
        p->getUniform1f("treeHeight")->set(treeHeight);
        p->getUniform1f("treeTau")->set(treeTau);
        p->getUniform1i("nViews")->set(nViews);

        thetav = 0;
        thetal = 0;
        phi = 0;
        lambda = 0;
        pass = 0;
        reshape(WIDTH, HEIGHT);

        readback = new ReadbackManager(1, 3, WIDTH * HEIGHT * 4);
    }

    void setViews(ptr<Module> m, int n, ptr<Texture2DArray> tree)
    {
        m->addInitialValue(new ValueSampler(SAMPLER_2D_ARRAY, "treeSampler", tree));
        const float zmax = 1;
        const float zmin = -1;
        for (int i = -n; i <= n; ++i) {
            for (int j = -n + abs(i); j <= n - abs(i); ++j) {
                float x = (i + j) / float(n);
                float y = (j - i) / float(n);
                float angle = 90.0 - std::max(fabs(x),fabs(y)) * 90.0;
                float alpha = x == 0.0 && y == 0.0 ? 0.0 : atan2(y, x) / M_PI * 180.0;

                mat4f cameraToWorld = mat4f::rotatex(90) * mat4f::rotatex(-angle);
                mat4f worldToCamera = cameraToWorld.inverse();

                box3f b;
                b = b.enlarge((worldToCamera * vec4f(-1.0, -1.0, zmin, 1.0)).xyz());
                b = b.enlarge((worldToCamera * vec4f(+1.0, -1.0, zmin, 1.0)).xyz());
                b = b.enlarge((worldToCamera * vec4f(-1.0, +1.0, zmin, 1.0)).xyz());
                b = b.enlarge((worldToCamera * vec4f(+1.0, +1.0, zmin, 1.0)).xyz());
                b = b.enlarge((worldToCamera * vec4f(-1.0, -1.0, zmax, 1.0)).xyz());
                b = b.enlarge((worldToCamera * vec4f(+1.0, -1.0, zmax, 1.0)).xyz());
                b = b.enlarge((worldToCamera * vec4f(-1.0, +1.0, zmax, 1.0)).xyz());
                b = b.enlarge((worldToCamera * vec4f(+1.0, +1.0, zmax, 1.0)).xyz());
                mat4f c2s = mat4f::orthoProjection(b.xmax, b.xmin, b.ymax, b.ymin, -2.0 * b.zmax, -2.0 * b.zmin);
                mat4f w2s = c2s * worldToCamera * mat4f::rotatez(-90.0 - alpha);
                int view = i * (1 - abs(i)) + j + 2 * n * i + n * (n + 1);

                char name[256];
                sprintf(name, "views[%d]", view);
                m->addInitialValue(new ValueMatrix3f(name, w2s.mat3x3()));
            }
        }
    }

    virtual ~PreprocessTreeTables()
    {
    }

    virtual void redisplay(double t, double dt)
    {
        controller->x0 = +665.0;
        controller->y0 = -364.0;
        controller->theta = max(min(thetav / (NTHETA - 1.0) * M_PI / 2.0, 89.0 / 180.0 * M_PI), 1.0 / 180.0 * M_PI);
        controller->phi = phi / (NPHI - 1.0) * M_PI + (phi == 0 ? 1e-3 : 0.0) - (phi == NPHI - 1 ? 1e-3 : 0);
        controller->d = 50.0;
        controller->zoom = 70.0;
        SceneManager::NodeIterator i = manager->getNodes("light");
        if (i.hasNext()) {
            float tl = max(min(thetal / (NTHETA - 1.0) * M_PI / 2.0, 89.0 / 180.0 * M_PI), 1.0 / 180.0 * M_PI);
            ptr<SceneNode> n = i.next();
            n->setLocalToParent(mat4d::translate(vec3d(0.0, -sin(tl), cos(tl))));
        }

        float density = lambda / (NLAMBDA - 1.0);
        (*(globals->getUsers().begin()))->getUniform1f("treeDensity")->set(density);
        (*(render->getUsers().begin()))->getUniform1f("pass")->set(pass);
        terrain->getUniform1f("pass")->set(pass);

        readback->newFrame();

        controller->update();
        controller->setProjection();
        FrameBuffer::getDefault()->clear(true, false, true);
        manager->update(t, dt);
        manager->draw();
        GlutWindow::redisplay(t, dt);

        if (pass == 0) {
            readback->readback(FrameBuffer::getDefault(), 0, 0, WIDTH, HEIGHT, RGBA, UNSIGNED_BYTE,
                new MyCallback1(thetav, thetal, phi, lambda));
            phi += 1;
            if (phi == NPHI) {
                phi = 0;
                thetav += 1;
                if (thetav == NTHETA) {
                    thetav = 0;
                    printf("STEP %d of %d\n", thetal + lambda * NTHETA, NTHETA * NLAMBDA);
                    thetal += 1;
                    if (thetal == NTHETA) {
                        thetal = 0;
                        lambda += 1;
                        if (lambda == NLAMBDA) {
                            printf("STEP %d of %d\n", NTHETA * NLAMBDA, NTHETA * NLAMBDA);
                            lambda = 0;
                            pass += 1;
                        }
                    }
                }
            }
        } else if (pass == 1) {
            readback->readback(FrameBuffer::getDefault(), 0, 0, WIDTH, HEIGHT, RGBA, UNSIGNED_BYTE,
                new MyCallback2(thetav, lambda));
            thetav += 1;
            if (thetav == NTHETA) {
                thetav = 0;
                lambda += 1;
                if (lambda == NLAMBDA) {
                    lambda = 0;
                    pass += 1;
                }
            }
        } else if (pass == 2) {
            readback->readback(FrameBuffer::getDefault(), 0, 0, WIDTH, HEIGHT, RGBA, UNSIGNED_BYTE,
                new MyCallback3(thetav));
            thetav += 1;
            if (thetav == NTHETA) {
                readback->newFrame();
                readback->newFrame();
                readback->newFrame();
                saveTables();
                ::exit(0);
            }
        }
    }

    virtual void reshape(int x, int y)
    {
        ptr<FrameBuffer> fb = FrameBuffer::getDefault();
        fb->setDepthTest(true, LESS);
        fb->setViewport(vec4<GLint>(0, 0, x, y));
        fb->setMultisample(true);
        fb->setSampleAlpha(true, true);
        GlutWindow::reshape(x, y);
    }

    virtual bool keyTyped(unsigned char c, modifier m, int x, int y)
    {
        if (c == 27) {
            ::exit(0);
        }
        return false;
    }

    void saveTables()
    {
        char name[512];
        FILE *f;

        float *kc = new float[NTHETA*NTHETA*NPHI*NLAMBDA];
        for (int l = 0; l < NLAMBDA; ++l) {
            for (int k = 0; k < NTHETA; ++k) {
                float thetal = k / (NTHETA - 1.0) * M_PI / 2.0;
                for (int i = 0; i < NTHETA; ++i) {
                    float thetav = i / (NTHETA - 1.0) * M_PI / 2.0;
                    for (int j = 0; j < NPHI; ++j) {
                        float phi = j / (NPHI - 1.0) * M_PI;
                        float hs = abs(sin(thetav)*sin(thetal)*cos(phi)+cos(thetal)*cos(thetav));
                        int off = (i + k * NTHETA + j * NTHETA * NTHETA + l * NTHETA * NTHETA * NPHI) * 4;
                        int dst = i + k * NTHETA + j * NTHETA * NTHETA + l * NTHETA * NTHETA * NPHI;
                        kc[dst] = hs > 1 - 1e-4 ? 1.0 : pass1[off+1] / pass1[off];
                    }
                }
            }
        }

        float *ao = new float[NTHETA*NLAMBDA];
        for (int i = 0; i < NTHETA; ++i) {
            for (int j = 0; j < NLAMBDA; ++j) {
                int off = (i + j * NTHETA) * 4;
                int dst = i + j * NTHETA;
                ao[dst] = pass2[off+1] / pass2[off];
            }
        }

        int thetal;
        int lambda;

        float *gc = new float[NTHETA*NTHETA*NPHI*NLAMBDA*2];
        for (int l = 0; l < NLAMBDA; ++l) {
            for (int k = 0; k < NTHETA; ++k) {
                thetal = k / (NTHETA - 1.0) * M_PI / 2.0;
                for (int i = 0; i < NTHETA; ++i) {
                    for (int j = 0; j < NPHI; ++j) {
                        int off = (i + k * NTHETA + min(j, NPHI - 2) * NTHETA * NTHETA + l * NTHETA * NTHETA * NPHI) * 4;
                        int off2 = (i + i * NTHETA + l * NTHETA * NTHETA * NPHI) * 4;
                        int dst = (i + k * NTHETA + j * NTHETA * NTHETA + l * NTHETA * NTHETA * NPHI) * 2;
                        if (j == 0 && i == k) {
                            gc[dst] = (pass1[off+3] - pass1[off]) / pass1[off+3];
                        } else {
                            gc[dst] = pass1[off+2] / pass1[off+3];
                        }
                        gc[dst+1] = (pass1[off2+3] - pass1[off2]) / pass1[off2+3];
                    }
                }
            }
        }

        float *gao = new float[NLAMBDA];
        for (int l = 0; l < NLAMBDA; ++l) {
            lambda = l / (NLAMBDA - 1.0);
            int SAMPLES = 32;
            float result = 0.0;
            for (int s = 0; s < SAMPLES; ++s) {
                float theta = (s + 0.5) / SAMPLES * M_PI / 2.0;
                float dtheta = 1.0 / SAMPLES * M_PI / 2.0;
                float thetap = atan(treeHeight / 2.0 * tan(theta));
                float thetai = thetap / (M_PI / 2.0) * (NTHETA - 1.0);
                int thetaid = floor(thetai);
                assert(thetaid < NTHETA - 1);
                float u = thetai - thetaid;
                int off1 = (thetaid + thetaid * NTHETA + l * NTHETA * NTHETA * NPHI) * 4;
                int off2 = ((thetaid + 1) + (thetaid + 1) * NTHETA + l * NTHETA * NTHETA * NPHI) * 4;
                float c1 = pass1[off1+2] / pass1[off1+3];
                float c2 = pass1[off2+2] / pass1[off2+3];
                float coverage = c1 * (1.0 - u) + c2 * u;
                result += 2.0 * coverage * sin(theta) * cos(theta) * dtheta;
            }
            gao[l] = result;
        }

        sprintf(name, "%s/pass1.dat", output);
        fopen(&f, name, "wb");
        fwrite(pass1, NLAMBDA*NPHI*NTHETA*NTHETA*4*sizeof(float), 1, f);
        fclose(f);

        sprintf(name, "%s/pass2.dat", output);
        fopen(&f, name, "wb");
        fwrite(pass2, NLAMBDA*NTHETA*4*sizeof(float), 1, f);
        fclose(f);

        sprintf(name, "%s/pass3.dat", output);
        fopen(&f, name, "wb");
        fwrite(pass3, NTHETA*4*sizeof(float), 1, f);
        fclose(f);

        int trailer[5];
        trailer[0] = 0xCAFEBABE;

        sprintf(name, "%s/treeKc.raw", output);
        fopen(&f, name, "wb");
        fwrite(kc, NTHETA*NTHETA*NPHI*NLAMBDA*sizeof(float), 1, f);
        trailer[1] = NTHETA;
        trailer[2] = NTHETA*NPHI*NLAMBDA;
        trailer[3] = NPHI*NLAMBDA;
        trailer[4] = 1;
        fwrite(trailer, 5*sizeof(int), 1, f);
        fclose(f);

        sprintf(name, "%s/treeAO.raw", output);
        fopen(&f, name, "wb");
        fwrite(ao, NTHETA*NLAMBDA*sizeof(float), 1, f);
        trailer[1] = NTHETA;
        trailer[2] = NLAMBDA;
        trailer[3] = 0;
        trailer[4] = 1;
        fwrite(trailer, 5*sizeof(int), 1, f);
        fclose(f);

        sprintf(name, "%s/groundCover.raw", output);
        fopen(&f, name, "wb");
        fwrite(gc, NTHETA*NTHETA*NPHI*NLAMBDA*2*sizeof(float), 1, f);
        trailer[1] = NTHETA;
        trailer[2] = NTHETA*NPHI*NLAMBDA;
        trailer[3] = NPHI*NLAMBDA;
        trailer[4] = 2;
        fwrite(trailer, 5*sizeof(int), 1, f);
        fclose(f);

        sprintf(name, "%s/groundAO.raw", output);
        fopen(&f, name, "wb");
        fwrite(gao, NLAMBDA*sizeof(float), 1, f);
        trailer[1] = NLAMBDA;
        trailer[2] = 1;
        trailer[3] = 0;
        trailer[4] = 1;
        fwrite(trailer, 5*sizeof(int), 1, f);
        fclose(f);

        delete[] kc;
        delete[] ao;
        delete[] gc;
        delete[] gao;
    }

    static void exit() {
        app.cast<PreprocessTreeTables>()->manager->getResourceManager()->close();
        Object::exit();
    }

    static static_ptr<Window> app;
};

static_ptr<Window> PreprocessTreeTables::app;

void preprocessTreeTables(float minRadius, float maxRadius, float treeHeight, float treeTau, int nViews, loadTreeViewsFunction loadTree, const char *output)
{
    pass1 = new float[NLAMBDA*NPHI*NTHETA*NTHETA*4];
    pass2 = new float[NLAMBDA*NTHETA*4];
    pass3 = new float[NTHETA*4];

    atexit(PreprocessTreeTables::exit);
    PreprocessTreeTables::app = new PreprocessTreeTables(minRadius, maxRadius, treeHeight, treeTau, nViews, loadTree, output);
    PreprocessTreeTables::app->start();
}

void mergeTreeTables(const char* input1, const char* input2, const char *output)
{
    pass1 = new float[NLAMBDA*NPHI*NTHETA*NTHETA*4];
    pass2 = new float[NLAMBDA*NPHI*NTHETA*NTHETA*4];
    pass3 = new float[NLAMBDA*NPHI*NTHETA*NTHETA*4];

    char name[512];
    FILE *f;

    int trailer[5];
    trailer[0] = 0xCAFEBABE;

    sprintf(name, "%s/treeKc.raw", input1);
    fopen(&f, name, "rb");
    fread(pass1, NLAMBDA*NPHI*NTHETA*NTHETA*sizeof(float), 1, f);
    fclose(f);
    sprintf(name, "%s/treeKc.raw", input2);
    fopen(&f, name, "rb");
    fread(pass2, NLAMBDA*NPHI*NTHETA*NTHETA*sizeof(float), 1, f);
    fclose(f);
    for (int i = 0; i < NLAMBDA*NPHI*NTHETA*NTHETA; ++i) {
        pass3[2*i] = pass1[i];
        pass3[2*i+1] = pass2[i];
    }
    sprintf(name, "%s/treeKc.raw", output);
    fopen(&f, name, "wb");
    fwrite(pass3, NTHETA*NTHETA*NPHI*NLAMBDA*2*sizeof(float), 1, f);
    trailer[1] = NTHETA;
    trailer[2] = NTHETA*NPHI*NLAMBDA;
    trailer[3] = NPHI*NLAMBDA;
    trailer[4] = 2;
    fwrite(trailer, 5*sizeof(int), 1, f);
    fclose(f);

    sprintf(name, "%s/treeAO.raw", input1);
    fopen(&f, name, "rb");
    fread(pass1, NTHETA*NLAMBDA*sizeof(float), 1, f);
    fclose(f);
    sprintf(name, "%s/treeAO.raw", input2);
    fopen(&f, name, "rb");
    fread(pass2, NTHETA*NLAMBDA*sizeof(float), 1, f);
    fclose(f);
    for (int i = 0; i < NTHETA*NLAMBDA; ++i) {
        pass3[2*i] = pass1[i];
        pass3[2*i+1] = pass2[i];
    }
    sprintf(name, "%s/treeAO.raw", output);
    fopen(&f, name, "wb");
    fwrite(pass3, NTHETA*NLAMBDA*2*sizeof(float), 1, f);
    trailer[1] = NTHETA;
    trailer[2] = NLAMBDA;
    trailer[3] = 0;
    trailer[4] = 2;
    fwrite(trailer, 5*sizeof(int), 1, f);
    fclose(f);

    sprintf(name, "%s/groundCover.raw", input1);
    fopen(&f, name, "rb");
    fread(pass1, NLAMBDA*NPHI*NTHETA*NTHETA*2*sizeof(float), 1, f);
    fclose(f);
    sprintf(name, "%s/groundCover.raw", input2);
    fopen(&f, name, "rb");
    fread(pass2, NLAMBDA*NPHI*NTHETA*NTHETA*2*sizeof(float), 1, f);
    fclose(f);
    for (int i = 0; i < NLAMBDA*NPHI*NTHETA*NTHETA; ++i) {
        pass3[4*i] = pass1[2*i];
        pass3[4*i+1] = pass1[2*i+1];
        pass3[4*i+2] = pass2[2*i];
        pass3[4*i+3] = pass2[2*i+1];
    }
    sprintf(name, "%s/groundCover.raw", output);
    fopen(&f, name, "wb");
    fwrite(pass3, NTHETA*NTHETA*NPHI*NLAMBDA*4*sizeof(float), 1, f);
    trailer[1] = NTHETA;
    trailer[2] = NTHETA*NPHI*NLAMBDA;
    trailer[3] = NPHI*NLAMBDA;
    trailer[4] = 4;
    fwrite(trailer, 5*sizeof(int), 1, f);
    fclose(f);

    sprintf(name, "%s/groundAO.raw", input1);
    fopen(&f, name, "rb");
    fread(pass1, NLAMBDA*sizeof(float), 1, f);
    fclose(f);
    sprintf(name, "%s/groundAO.raw", input2);
    fopen(&f, name, "rb");
    fread(pass2, NLAMBDA*sizeof(float), 1, f);
    fclose(f);
    for (int i = 0; i < NLAMBDA; ++i) {
        pass3[2*i] = pass1[i];
        pass3[2*i+1] = pass2[i];
    }
    sprintf(name, "%s/groundAO.raw", output);
    fopen(&f, name, "wb");
    fwrite(pass3, NLAMBDA*2*sizeof(float), 1, f);
    trailer[1] = NLAMBDA;
    trailer[2] = 1;
    trailer[3] = 0;
    trailer[4] = 2;
    fwrite(trailer, 5*sizeof(int), 1, f);
    fclose(f);
}

}
