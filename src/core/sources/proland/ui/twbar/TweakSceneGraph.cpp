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

#include "proland/ui/twbar/TweakSceneGraph.h"

#include "ork/resource/ResourceTemplate.h"

#include "proland/producer/GPUTileStorage.h"
#include "proland/terrain/TileSampler.h"
#include "proland/particles/ParticleProducer.h"
#include "proland/ui/SceneVisitor.h"

using namespace std;
using namespace ork;

namespace proland
{

const char * renderShader = "\
uniform sampler1D tex1D;\n\
uniform sampler2D tex2D;\n\
uniform sampler2DArray tex2DArray;\n\
uniform sampler3D tex3D;\n\
uniform float type;\n\
uniform float level;\n\
uniform float mode;\n\
uniform vec4 norm;\n\
uniform vec4 position;\n\
uniform vec4 coords;\n\
uniform ivec3 grid;\n\
\n\
\n\
#ifdef _VERTEX_\n\
layout(location=0) in vec2 vertex;\n\
out vec2 uv;\n\
void main() {\n\
    vec2 xy = vertex.xy * 0.5 + vec2(0.5);\n\
    gl_Position = vec4(xy * position.zw + (vec2(1.0) - xy) * position.xy, 0.0, 1.0);\n\
    uv = xy * coords.zw + (vec2(1.0) - xy) * coords.xy;\n\
}\n\
#endif\n\
\n\
#ifdef _FRAGMENT_\n\
layout(location=0) out vec4 data;\n\
in vec2 uv;\n\
void main() {\n\
    vec3 uvl = vec3(uv, 0.0);\n\
    if ((uvl.x >= 0.0 && uvl.x <= 1.0 && uvl.y >= 0.0 && uvl.y <= 1.0) && grid.z > 0) {\n\
        ivec2 ij = ivec2(floor(uv * grid.xy));\n\
        int layer = ij.x + ij.y * grid.x;\n\
        uvl = layer < grid.z ? vec3(fract(uv * grid.xy), layer) : vec3(-1.0, -1.0, 0.0);\n\
    }\n\
    if (type == 0) {\n\
        data = textureLod(tex1D, uv.x, level);\n\
    } else if (type == 1) {\n\
        data = textureLod(tex2D, uv, level);\n\
    } else if (type == 2) {\n\
        data = textureLod(tex2DArray, uvl, level);\n\
    } else if (type == 3) {\n\
        data = textureLod(tex3D, vec3(uvl.xy, (uvl.z + 0.5) / grid.z), level);\n\
    } else {\n\
        data = vec4(255.0, 0.0, 0.0, 1.0);\n\
    }\n\
    if (uvl.x < 0.0 || uvl.x > 1.0 || uvl.y < 0.0 || uvl.y > 1.0) {\n\
        data = vec4(0.0, 0.0, 0.0, 1.0);\n\
    }\n\
    data /= norm;\n\
    if (mode < 0.5) {\n\
        data.w = 1.0;\n\
    } else if (mode < 1.5) {\n\
        vec4 c = data;\n\
        vec3 backgroundColor = vec3(0.95) - mod(dot(floor(uv / 0.1), vec2(1.0)), 2.0) * vec3(0.10);\n\
        data = vec4(c.rgb * c.a + (1.0 - c.a) * backgroundColor, 1.0);\n\
    }\n\
}\n\
#endif\n\
";

const char * selectShader = "\
#ifdef _VERTEX_\n\
uniform vec4 position;\n\
layout(location=0) in vec4 vertex;\n\
void main() {\n\
    vec2 xy = vertex.xy * 0.5 + vec2(0.5);\n\
    gl_Position = vec4(xy * position.zw + (vec2(1.0) - xy) * position.xy, 0.0, 1.0);\n\
}\n\
#endif\n\
#ifdef _FRAGMENT_\n\
layout(location=0) out vec4 data;\n\
void main() {\n\
    data = vec4(1.0, 0.0, 0.0, 0.5);\n\
}\n\
#endif\n\
";

int getTextureType(ptr<Texture> tex)
{
    if (tex.cast<Texture1D>() != NULL) {
        return 0;
    }
    if (tex.cast<Texture2D>() != NULL) {
        return 1;
    }
    if (tex.cast<Texture2DArray>() != NULL) {
        return 2;
    }
    if (tex.cast<Texture3D>() != NULL) {
        return 3;
    }
    assert(false);
    return -1;
}

void TW_CALL getUniformState(void *value, void *clientData)
{
    *(bool*) value = ((TileSampler*) clientData)->getAsync();
}

void TW_CALL setUniformState(const void *value, void *clientData)
{
    ((TileSampler*) clientData)->setAsynchronous(*(bool*) value);
}

void TW_CALL invalidateTiles(void *clientData)
{
    ((TileProducer*) clientData)->invalidateTiles();
}

void TW_CALL getLayerState(void *value, void *clientData)
{
    *(bool*) value = ((TileLayer*) clientData)->isEnabled();
}

void TW_CALL setLayerState(const void *value, void *clientData)
{
    ((TileLayer*) clientData)->setIsEnabled(*(bool*) value);
}

void TW_CALL getParticleLayerState(void *value, void *clientData)
{
    *(bool*) value = ((ParticleLayer*) clientData)->isEnabled();
}

void TW_CALL setParticleLayerState(const void *value, void *clientData)
{
    ((ParticleLayer*) clientData)->setIsEnabled(*(bool*) value);
}

void TW_CALL getMethodState(void *value, void *clientData)
{
    *(bool*) value = ((Method*) clientData)->isEnabled();
}

void TW_CALL setMethodState(const void *value, void *clientData)
{
    ((Method*) clientData)->setIsEnabled(*(bool*) value);
}

void TW_CALL getUsedTiles(void *value, void *clientData)
{
    *(int*) value = ((TileCache*) clientData)->getUsedTiles();
}

void TW_CALL getUnusedTiles(void *value, void *clientData)
{
    *(int*) value = ((TileCache*) clientData)->getUnusedTiles();
}

void TW_CALL GetCurrentTexCallback(void *value, void *clientData)
{
    *static_cast<int*>(value) = static_cast<TweakSceneGraph*>(clientData)->getCurrentTexture();
}

void TW_CALL SetCurrentTexCallback(const void *value, void *clientData)
{
    static_cast<TweakSceneGraph*>(clientData)->setCurrentTexture(*static_cast<const int*>(value));
}

void TW_CALL GetLevelCallback(void *value, void *clientData)
{
    *static_cast<int*>(value) = static_cast<TweakSceneGraph*>(clientData)->getCurrentLevel();
}

void TW_CALL SetLevelCallback(const void *value, void *clientData)
{
    static_cast<TweakSceneGraph*>(clientData)->setCurrentLevel(*static_cast<const int*>(value));
}

void TW_CALL GetModeCallback(void *value, void *clientData)
{
    *static_cast<int*>(value) = static_cast<TweakSceneGraph*>(clientData)->getCurrentMode();
}

void TW_CALL SetModeCallback(const void *value, void *clientData)
{
    static_cast<TweakSceneGraph*>(clientData)->setCurrentMode(*static_cast<const int*>(value));
}

void TW_CALL GetNormXCallback(void *value, void *clientData)
{
    vec4f v = static_cast<TweakSceneGraph*>(clientData)->getCurrentNorm();
    *static_cast<float*>(value) = v.x;
}

void TW_CALL SetNormXCallback(const void *value, void *clientData)
{
    vec4f v = static_cast<TweakSceneGraph*>(clientData)->getCurrentNorm();
    v.x = *static_cast<const float*>(value);
    static_cast<TweakSceneGraph*>(clientData)->setCurrentNorm(v);
}

void TW_CALL GetNormYCallback(void *value, void *clientData)
{
    vec4f v = static_cast<TweakSceneGraph*>(clientData)->getCurrentNorm();
    *static_cast<float*>(value) = v.y;
}

void TW_CALL SetNormYCallback(const void *value, void *clientData)
{
    vec4f v = static_cast<TweakSceneGraph*>(clientData)->getCurrentNorm();
    v.y = *static_cast<const float*>(value);
    static_cast<TweakSceneGraph*>(clientData)->setCurrentNorm(v);
}

void TW_CALL GetNormZCallback(void *value, void *clientData)
{
    vec4f v = static_cast<TweakSceneGraph*>(clientData)->getCurrentNorm();
    *static_cast<float*>(value) = v.z;
}

void TW_CALL SetNormZCallback(const void *value, void *clientData)
{
    vec4f v = static_cast<TweakSceneGraph*>(clientData)->getCurrentNorm();
    v.z = *static_cast<const float*>(value);
    static_cast<TweakSceneGraph*>(clientData)->setCurrentNorm(v);
}

void TW_CALL GetNormWCallback(void *value, void *clientData)
{
    vec4f v = static_cast<TweakSceneGraph*>(clientData)->getCurrentNorm();
    *static_cast<float*>(value) = v.w;
}

void TW_CALL SetNormWCallback(const void *value, void *clientData)
{
    vec4f v = static_cast<TweakSceneGraph*>(clientData)->getCurrentNorm();
    v.w = *static_cast<const float*>(value);
    static_cast<TweakSceneGraph*>(clientData)->setCurrentNorm(v);
}

class TwBarSceneVisitor : public SceneVisitor
{
public:
    TwBar *bar;

    string parentGroup;

    string group;

    string label;

    int *counter;

    map<string, TweakSceneGraph::TextureInfo> *textures;

    int *cacheCount;

    TwBarSceneVisitor(TwBar *bar, string parentGroup, string group, string label, int *counter,
            map<string, TweakSceneGraph::TextureInfo> *textures, int *cacheCount) :
        bar(bar), parentGroup(parentGroup), group(group), label(label), counter(counter),
        textures(textures), cacheCount(cacheCount)
    {
    }

    virtual ptr<SceneVisitor> visitNode(ptr<SceneNode> node)
    {
        char subgroup[256];
        char sublabel[256];
        sprintf(subgroup, "node-%d", *counter);
        Resource *r = dynamic_cast<Resource*>(node.get());
        if (r != NULL && r->getName().length() > 0) {
            sprintf(sublabel, "%s", r->getName().c_str());
        } else if (node->getFlags().hasNext()) {
            sprintf(sublabel, "%s", node->getFlags().next().c_str());
        } else {
            sprintf(sublabel, "Node");
        }
        *counter += 1;
        SceneNode::ModuleIterator i = node->getModules();
        while (i.hasNext()) {
            set<Program*> progs = i.next()->getUsers();
            set<Program*>::iterator j = progs.begin();
            while (j != progs.end()) {
                vector< ptr<Uniform> > uniforms = (*j)->getUniforms();
                vector< ptr<Uniform> >::iterator k = uniforms.begin();
                while (k != uniforms.end()) {
                    ptr<UniformSampler> u = (*k).cast<UniformSampler>();
                    if (u != NULL && u->get() != NULL) {
                        addTexture(u->getName(), u->get());
                    }
                    ++k;
                }
                ++j;
            }
        }
        return new TwBarSceneVisitor(bar, group, subgroup, sublabel, counter, textures, cacheCount);
    }

    virtual ptr<SceneVisitor> visitProducer(ptr<TileProducer> producer)
    {
        char subgroup[256];
        char sublabel[256];
        sprintf(subgroup, "producer-%d", *counter);
        Resource *r = dynamic_cast<Resource*>(producer.get());
        if (r != NULL && r->getName().length() > 0) {
            sprintf(sublabel, "%s", r->getName().c_str());
        } else {
            sprintf(sublabel, "Producer");
        }
        *counter += 1;
        char id[256];
        char def[256];
        sprintf(id, "producer-%d", *counter);
        sprintf(def, "label='Invalidate' group='%s'", subgroup);
        TwAddButton(bar, id, invalidateTiles, producer.get(), def);
        *counter += 1;
        return new TwBarSceneVisitor(bar, group, subgroup, sublabel, counter, textures, cacheCount);
    }

    virtual ptr<SceneVisitor> visitLayer(ptr<TileLayer> layer)
    {
        char subgroup[256];
        char sublabel[256];
        sprintf(subgroup, "layer-%d", *counter);
        Resource *r = dynamic_cast<Resource*>(layer.get());
        if (r != NULL && r->getName().length() > 0) {
            sprintf(sublabel, "%s", r->getName().c_str());
        } else {
            sprintf(sublabel, "Layer");
        }
        *counter += 1;
        char id[256];
        char def[256];
        sprintf(id, "layer-%d", *counter);
        sprintf(def, "label='Enabled' group='%s'", subgroup);
        TwAddVarCB(bar, id, TW_TYPE_BOOL8, setLayerState, getLayerState, layer.get(), def);
        *counter += 1;
        return new TwBarSceneVisitor(bar, group, subgroup, sublabel, counter, textures, cacheCount);
    }

    virtual ptr<SceneVisitor> visitNodeField(string &name, ptr<Object> field)
    {
        ptr<TileSampler> u = field.cast<TileSampler>();
        if (u != NULL) {
            char subgroup[256];
            char sublabel[256];
            sprintf(subgroup, "uniform-%d", *counter);
            sprintf(sublabel, "%s",name.c_str());
            *counter += 1;
            char id[256];
            char def[256];
            sprintf(id, "uniform-%d", *counter);
            sprintf(def, "label='Async' group='%s'", subgroup);
            TwAddVarCB(bar, id, TW_TYPE_BOOL8, setUniformState, getUniformState, u.get(), def);
            *counter += 1;
            return new TwBarSceneVisitor(bar, group, subgroup, sublabel, counter, textures, cacheCount);
        }

        ptr<ParticleProducer> p = field.cast<ParticleProducer>();
        if (p != NULL) {
            char subgroup[256];
            char sublabel[256];
            sprintf(subgroup, "particles-%d", *counter);
            Resource *r = dynamic_cast<Resource*>(p.get());
            if (r != NULL && r->getName().length() > 0) {
                sprintf(sublabel, "%s", r->getName().c_str());
            } else {
                sprintf(sublabel, "Particles");
            }
            *counter += 1;
            for (int i = 0; i < p->getLayerCount(); ++i) {
                ptr<ParticleLayer> pl = p->getLayer(i);
                char id[256];
                char def[256];
                char label[256];
                Resource *r = dynamic_cast<Resource*>(pl.get());
                if (r != NULL && r->getName().length() > 0) {
                    sprintf(label, "%s", r->getName().c_str());
                } else {
                    sprintf(label, "Layer");
                }
                sprintf(id, "particlelayer-%d", *counter);
                sprintf(def, "label='%s' group='%s'", label, subgroup);
                TwAddVarCB(bar, id, TW_TYPE_BOOL8, setParticleLayerState, getParticleLayerState, pl.get(), def);
                *counter += 1;
            }
            char def[256];
            sprintf(def, "%s/%s group='%s' label='%s'", TwGetBarName(bar), subgroup, group.c_str(), sublabel);
            TwDefine(def);
        }
        return NULL;
    }

    virtual ptr<SceneVisitor> visitNodeMethod(string &name, ptr<Method> method)
    {
        char id[256];
        char def[256];
        sprintf(id, "method-%d", *counter);
        sprintf(def, "label='%s' group='%s'", name.c_str(), group.c_str());
        TwAddVarCB(bar, id, TW_TYPE_BOOL8, setMethodState, getMethodState, method.get(), def);
        *counter += 1;
        return this;
    }

    virtual ptr<SceneVisitor> visitCache(ptr<TileCache> cache)
    {
        char id[256];
        char def[256];
        char group[256];
        char label[256];
        sprintf(group, "cache-%d", *counter);
        *counter += 1;
        Resource *r = dynamic_cast<Resource*>(cache.get());
        if (r != NULL && r->getName().length() > 0) {
            sprintf(label, "%s (%d)", r->getName().c_str(), cache->getStorage()->getCapacity());
        } else {
            sprintf(label, "Cache (%d)", cache->getStorage()->getCapacity());
        }
        sprintf(id, "cache-%d", *counter);
        sprintf(def, "label='Used tiles' group='%s'", group);
        TwAddVarCB(bar, id, TW_TYPE_INT32, NULL, getUsedTiles, cache.get(), def);
        *counter += 1;
        sprintf(id, "cache-%d", *counter);
        sprintf(def, "label='Unused tiles' group='%s'", group);
        TwAddVarCB(bar, id, TW_TYPE_INT32, NULL, getUnusedTiles, cache.get(), def);
        *counter += 1;
        ptr<GPUTileStorage> storage = cache->getStorage().cast<GPUTileStorage>();
        if (storage != NULL) {
            Resource *r = dynamic_cast<Resource*>(cache.get());
            for (int i = 0; i < storage->getTextureCount(); ++i) {
                char name[256];
                sprintf(name, "%s-%d", r == NULL ? "Storage" : r->getName().c_str(), i);
                addTexture(name, storage->getTexture(i));
            }
        }
        sprintf(def, "%s/%s group='caches' label='%s'", TwGetBarName(bar), group, label);
        TwDefine(def);
        ++(*cacheCount);
        return this;
    }

    virtual void visitEnd()
    {
        char def[256];
        sprintf(def, "%s/%s group='%s' label='%s' opened='false'", TwGetBarName(bar), group.c_str(), parentGroup.c_str(), label.c_str());
        TwDefine(def);
    }

    void addTexture(const string &name, ptr<Texture> t)
    {
        map<string, TweakSceneGraph::TextureInfo>::iterator i = textures->begin();
        while (i != textures->end()) {
            if (i->second.tex == t) {
                textures->erase(i);
                break;
            }
            ++i;
        }
        textures->insert(make_pair(name, t));
    }
};

TweakSceneGraph::TweakSceneGraph() : TweakBarHandler()
{
}

TweakSceneGraph::TweakSceneGraph(ptr<SceneNode> scene, bool active)
{
    init(scene, active);
}

void TweakSceneGraph::init(ptr<SceneNode> scene, bool active)
{
    TweakBarHandler::init("Scene", NULL, active);
    this->currentTexture = -1;
    this->currentInfo = NULL;
    this->scene = scene;
    mode = NONE;
    displayBox = box2f(0.0f, 1.0f, 0.0f, 1.0f);
    renderProg = new Program(new Module(330, renderShader));
    renderTexture1DU = renderProg->getUniformSampler("tex1D");
    renderTexture2DU = renderProg->getUniformSampler("tex2D");
    renderTexture2DArrayU = renderProg->getUniformSampler("tex2DArray");
    renderTexture3DU = renderProg->getUniformSampler("tex3D");
    renderTypeU = renderProg->getUniform1f("type");
    renderLevelU = renderProg->getUniform1f("level");
    renderModeU = renderProg->getUniform1f("mode");
    renderNormU = renderProg->getUniform4f("norm");
    renderPositionU = renderProg->getUniform4f("position");
    renderCoordsU = renderProg->getUniform4f("coords");
    renderGridU = renderProg->getUniform3i("grid");
    selectProg = new Program(new Module(330, selectShader));
    selectPositionU = selectProg->getUniform4f("position");
}

TweakSceneGraph::~TweakSceneGraph()
{
}

int TweakSceneGraph::getCurrentTexture() const
{
    return currentTexture;
}

int TweakSceneGraph::getCurrentLevel() const
{
    return currentInfo == NULL ? 0 : currentInfo->level;
}

int TweakSceneGraph::getCurrentMode() const
{
    return currentInfo == NULL ? 1 : currentInfo->mode;
}

box2f TweakSceneGraph::getCurrentArea() const
{
    return currentInfo == NULL ? box2f(0.0f, 1.0f, 0.0f, 1.0f) : currentInfo->area;
}

vec4f TweakSceneGraph::getCurrentNorm() const
{
    return currentInfo == NULL ? vec4f(1.0f, 1.0f, 1.0f, 1.0f) : currentInfo->norm;
}

void TweakSceneGraph::setCurrentTexture(int tex)
{
    currentTexture = tex;
    currentInfo = NULL;
    int j = 0;
    map<string, TextureInfo>::iterator i = textures.begin();
    while (i != textures.end() && j < tex) {
        i++;
        j++;
    }
    if (i != textures.end()) {
        currentInfo = &(i->second);
    }
}

void TweakSceneGraph::setCurrentLevel(int level)
{
    if (currentInfo != NULL) {
        currentInfo->level = level;
    }
}

void TweakSceneGraph::setCurrentMode(int mode)
{
    if (currentInfo != NULL) {
        currentInfo->mode = mode;
    }
}

void TweakSceneGraph::setCurrentArea(const box2f &area)
{
    if (currentInfo != NULL) {
        currentInfo->area = area;
    }
}

void TweakSceneGraph::setCurrentNorm(const vec4f &norm)
{
    if (currentInfo != NULL) {
        currentInfo->norm = norm;
    }
}

void TweakSceneGraph::redisplay(double t, double dt, bool &needUpdate)
{
    needUpdate = false;
    if (isActive() && currentTexture != -1) {
        ptr<FrameBuffer> fb = SceneManager::getCurrentFrameBuffer();
        FrameBuffer::Parameters old = fb->getParameters();
        fb->setBlend(true, ADD, SRC_ALPHA, ONE_MINUS_SRC_ALPHA, ADD, ZERO, ONE);
        fb->setColorMask(true, true, true, false);
        fb->setDepthMask(false);
        fb->setDepthTest(false);
        fb->setPolygonMode(FILL, FILL);

        int l, w, h;
        int type = getTextureType(currentInfo->tex);
        switch (type) {
            case 0:
                renderTexture1DU->set(currentInfo->tex);
                renderGridU->set(vec3i::ZERO);
                break;
            case 1:
                renderTexture2DU->set(currentInfo->tex);
                renderGridU->set(vec3i::ZERO);
                break;
            case 2:
                l = currentInfo->tex.cast<Texture2DArray>()->getLayers();
                w = (int) sqrt(float(l));
                h = l % w == 0 ? l / w : l / w + 1;
                renderTexture2DArrayU->set(currentInfo->tex);
                renderGridU->set(vec3i(w, h, l));
                break;
            case 3:
                l = currentInfo->tex.cast<Texture3D>()->getDepth();
                w = (int) sqrt(float(l));
                h = l % w == 0 ? l / w : l / w + 1;
                renderTexture3DU->set(currentInfo->tex);
                renderGridU->set(vec3i(w, h, l));
                break;
        }
        renderTypeU->set(type);
        renderLevelU->set(currentInfo->level);
        renderModeU->set(currentInfo->mode);
        renderNormU->set(currentInfo->norm);
        renderPositionU->set(vec4f(displayBox.xmin, displayBox.ymin, displayBox.xmax, displayBox.ymax));
        renderCoordsU->set(vec4f(currentInfo->area.xmin, currentInfo->area.ymin, currentInfo->area.xmax, currentInfo->area.ymax));
        fb->drawQuad(renderProg);

        if (mode == ZOOMING) {
            selectPositionU->set(vec4f(lastPos.x, lastPos.y, newPos.x, newPos.y));
            fb->drawQuad(selectProg);
        }

        fb->setParameters(old);
    }
}

bool TweakSceneGraph::mouseClick(EventHandler::button b, EventHandler::state s, EventHandler::modifier m, int x, int y, bool &needUpdate)
{
    needUpdate = false;
    if (isActive() && currentTexture != -1) {
        vec4i viewport = SceneManager::getCurrentFrameBuffer()->getViewport();
        vec2f realV = vec2f(2.0f * x / (viewport.z - viewport.x) - 1.0f, -(2.0 * y / (viewport.w - viewport.y) - 1.0f));
        if (displayBox.contains(realV) || mode == ZOOMING) {
            if (b == EventHandler::LEFT_BUTTON) {
                if (s == EventHandler::DOWN) { // left mouse down
                    if ((m & EventHandler::CTRL) == 0 && (m & EventHandler::SHIFT) == 0) { // no modifier -> moving
                        mode = MOVING;
                    } else if ((m & EventHandler::SHIFT) == 0) { // CTRL pressed -> zoom
                        mode = ZOOMING;
                    } else { // SHIFT PRESSED -> moving uv coordinates.
                        mode = MOVING_UV;
                    }
                    lastPos = realV;
                    newPos = realV;
                } else { // left mouse UP
                    if (mode == MOVING) {
                        if (realV == lastPos) {
                            vec2f uv;
                            box2f old = currentInfo->area;
                            uv.x = ((realV.x  - displayBox.xmin) / (displayBox.xmax - displayBox.xmin)) * (old.xmax - old.xmin) + old.xmin;
                            uv.y  = ((realV.y - displayBox.ymin) / (displayBox.ymax - displayBox.ymin)) * (old.ymax - old.ymin) + old.ymin;
                            currentInfo->area.xmax = uv.x + (old.xmax - uv.x) / 1.25;
                            currentInfo->area.xmin = uv.x + (old.xmin - uv.x) / 1.25;
                            currentInfo->area.ymax = uv.y + (old.ymax - uv.y) / 1.25;
                            currentInfo->area.ymin = uv.y + (old.ymin - uv.y) / 1.25;
                        }
                    } else if (mode == ZOOMING) {
                        vec2f first;
                        vec2f second;
                        box2f old = currentInfo->area;
                        first.x  = ((lastPos.x - displayBox.xmin) / (displayBox.xmax - displayBox.xmin)) * (old.xmax - old.xmin) + old.xmin;
                        first.y  = ((lastPos.y - displayBox.ymin) / (displayBox.ymax - displayBox.ymin)) * (old.ymax - old.ymin) + old.ymin;
                        second.x = ((newPos.x  - displayBox.xmin) / (displayBox.xmax - displayBox.xmin)) * (old.xmax - old.xmin) + old.xmin;
                        second.y = ((newPos.y  - displayBox.ymin) / (displayBox.ymax - displayBox.ymin)) * (old.ymax - old.ymin) + old.ymin;
                        currentInfo->area = box2f(first, second);// compute correct uv coordinates in the new region.
                    }
                    mode = NONE;
                }
            } else { //if right button
                currentInfo->area = box2f(0.f, 1.f, 0.f, 1.f);// unzoom.
            }
            return true;
        }
    }
    return false;
}

bool TweakSceneGraph::mouseWheel(EventHandler::wheel b, EventHandler::modifier m, int x, int y, bool &needUpdate)
{
    needUpdate = false;
    vec4<GLint> viewport = SceneManager::getCurrentFrameBuffer()->getViewport();
    vec2f realV = vec2f(2.0f * x / (viewport.z - viewport.x) - 1.0f, -(2.0 * y / (viewport.w - viewport.y) - 1.0f));
    if (isActive() && currentTexture != -1 && displayBox.contains(realV)) {
        vec2f uv;
        box2f old = currentInfo->area;
        uv.x = ((realV.x  - displayBox.xmin) / (displayBox.xmax - displayBox.xmin)) * (old.xmax - old.xmin) + old.xmin;
        uv.y = ((realV.y - displayBox.ymin) / (displayBox.ymax - displayBox.ymin)) * (old.ymax - old.ymin) + old.ymin;
        if (b == EventHandler::WHEEL_DOWN) {
            currentInfo->area.xmax = uv.x + (old.xmax - uv.x) * 1.25;
            currentInfo->area.xmin = uv.x + (old.xmin - uv.x) * 1.25;
            currentInfo->area.ymax = uv.y + (old.ymax - uv.y) * 1.25;
            currentInfo->area.ymin = uv.y + (old.ymin - uv.y) * 1.25;
        } else {
            currentInfo->area.xmax = uv.x + (old.xmax - uv.x) / 1.25;
            currentInfo->area.xmin = uv.x + (old.xmin - uv.x) / 1.25;
            currentInfo->area.ymax = uv.y + (old.ymax - uv.y) / 1.25;
            currentInfo->area.ymin = uv.y + (old.ymin - uv.y) / 1.25;
        }
        return true;
    }
    return false;
}

bool TweakSceneGraph::mousePassiveMotion(int x, int y, bool &needUpdate)
{
    return mouseMotion(x, y, needUpdate);
}

bool TweakSceneGraph::mouseMotion(int x, int y, bool &needUpdate)
{
    needUpdate = false;
    bool res = false;
    if (isActive() && currentTexture != -1) {
        vec4i viewport = SceneManager::getCurrentFrameBuffer()->getViewport();
        vec2f realV = vec2f(2.0f * x / (viewport.z - viewport.x) - 1.0f, -(2.0 * y / (viewport.w - viewport.y) - 1.0f));
        if (displayBox.contains(realV) || mode == ZOOMING) {
            if (mode == MOVING) {
                vec2f v = realV - newPos;
                if (abs(displayBox.xmax - realV.x) > 0.05) {
                    displayBox.xmin += v.x;
                }
                if (abs(displayBox.xmin - realV.x) > 0.05) {
                    displayBox.xmax += v.x;
                }
                if (abs(displayBox.ymax - realV.y) > 0.05) {
                    displayBox.ymin += v.y;
                }
                if (abs(displayBox.ymin - realV.y) > 0.05) {
                    displayBox.ymax += v.y;
                }
            } else if (mode == ZOOMING) {
            } else if (mode == MOVING_UV) {
                box2f old = currentInfo->area;
                vec2f v = (realV - newPos) * vec2f(old.xmax - old.xmin, old.ymax - old.ymin);
                currentInfo->area.xmin -= v.x;
                currentInfo->area.xmax -= v.x;
                currentInfo->area.ymin -= v.y;
                currentInfo->area.ymax -= v.y;
            }
            res = true;
        } else {
            mode = NONE;
        }
        newPos = realV;
    }
    return res;
}

void TweakSceneGraph::updateBar(TwBar *bar)
{
    char def[256];
    int counter = 0;
    textures.clear();
    int cacheCount = 0;
    ptr<SceneVisitor> v = new TwBarSceneVisitor(bar, "", "Scene", "Scene", &counter, &textures, &cacheCount);
    v->accept(scene);
    if (textures.size() > 0) {
        int j = 1;
        int n = textures.size();
        TwEnumVal *textureNames = new TwEnumVal[n + 1];
        textureNames[0].Value = -1;
        textureNames[0].Label = "None";
        map<string, TextureInfo>::iterator i = textures.begin();
        while (i != textures.end()) {
            textureNames[j].Value = j - 1;
            textureNames[j].Label = i->first.c_str();
            i++;
            j++;
        }
        TwType textureType = TwDefineEnum("TextureType", textureNames, n + 1);
        TwType alphaType = TwDefineEnum("AlphaType", NULL, 0);
        delete[] textureNames;
        TwAddVarCB(bar, "texCurrent", textureType, SetCurrentTexCallback, GetCurrentTexCallback, this, "label='Current texture' group='textures'");
        TwAddVarCB(bar, "texLevel", TW_TYPE_FLOAT, SetLevelCallback, GetLevelCallback, this, "group=textures label='Level'");
        TwAddVarCB(bar, "texMode", alphaType, SetModeCallback, GetModeCallback, this, "group=textures label='Alpha Mode' enum='0 {No Transparency}, 1 {Semi Transparency}, 2 {Transparency} '");
        TwAddVarCB(bar, "texNormX", TW_TYPE_FLOAT, SetNormXCallback, GetNormXCallback, this, "group=textures label='R Norm'");
        TwAddVarCB(bar, "texNormY", TW_TYPE_FLOAT, SetNormYCallback, GetNormYCallback, this, "group=textures label='G Norm'");
        TwAddVarCB(bar, "texNormZ", TW_TYPE_FLOAT, SetNormZCallback, GetNormZCallback, this, "group=textures label='B Norm'");
        TwAddVarCB(bar, "texNormW", TW_TYPE_FLOAT, SetNormWCallback, GetNormWCallback, this, "group=textures label='A Norm'");
        sprintf(def, "%s/textures group='Scene' opened='false' label='textures'", TwGetBarName(bar));
        TwDefine(def);
    }
    if (cacheCount > 0) {
        sprintf(def, "%s/caches group='Scene' opened='false'", TwGetBarName(bar));
        TwDefine(def);
    }
}

void TweakSceneGraph::swap(ptr<TweakSceneGraph> o)
{
    TweakBarHandler::swap(o);
    std::swap(scene, o->scene);
}

class TweakSceneGraphResource : public ResourceTemplate<55, TweakSceneGraph>
{
public:
    TweakSceneGraphResource(ptr<ResourceManager> manager, const string &name, ptr<ResourceDescriptor> desc, const TiXmlElement *e = NULL) :
        ResourceTemplate<55, TweakSceneGraph> (manager, name, desc)
    {
        e = e == NULL ? desc->descriptor : e;
        checkParameters(desc, e, "name,scene,active,");

        ptr<SceneNode> scene = manager->loadResource(getParameter(desc, e, "scene")).cast<SceneNode>();
        bool active = true;
        if (e->Attribute("active") != NULL) {
            active = strcmp(e->Attribute("active"), "true") == 0;
        }

        init(scene, active);
    }
};

extern const char tweakScene[] = "tweakScene";

static ResourceFactory::Type<tweakScene, TweakSceneGraphResource> TweakSceneGraphType;

}
