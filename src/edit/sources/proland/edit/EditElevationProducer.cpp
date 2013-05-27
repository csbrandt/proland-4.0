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

#include "proland/edit/EditElevationProducer.h"

#include <sstream>

#include "ork/resource/ResourceTemplate.h"
#include "ork/scenegraph/SetTargetTask.h"
#include "ork/ui/Window.h"
#include "proland/producer/GPUTileStorage.h"
#include "proland/terrain/TerrainNode.h"
#include "proland/edit/EditResidualProducer.h"
#include "proland/math/seg2.h"

namespace proland
{

/*
 * Returns true if the given bounding box is on the right side
 * of the given line.
 */
bool rightSide(const box2f &b, const vec2f &line)
{
    return cross(line, vec2f(b.xmin, b.ymin)) < 0.0f
      && cross(line, vec2f(b.xmin, b.ymax)) < 0.0f
      && cross(line, vec2f(b.xmax, b.ymin)) < 0.0f
      && cross(line, vec2f(b.xmax, b.ymax)) < 0.0f;
}

/*
 * Returns true if the bounding box b intersects the convex
 * hull of the bounding boxes p and q.
 */
bool clipStroke(const box2f &b, const box2f &p, const box2f &q)
{
    if (clipRectangle(b, p) || clipRectangle(b, q)) {
        return true;
    }
    if (!clipRectangle(b, p.enlarge(q))) {
        return false;
    }
    vec2f cp = vec2f((p.xmin + p.xmax) * 0.5f, (p.ymin + p.ymax) * 0.5f);
    vec2f cq = vec2f((q.xmin + q.xmax) * 0.5f, (q.ymin + q.ymax) * 0.5f);
    if (cq.x > cp.x) {
        if (cq.y > cp.y) {
            return rightSide(b, vec2f(q.xmax - p.xmax, q.ymin - p.ymin)) ||
                rightSide(b, vec2f(p.xmin - q.xmin, p.ymax - q.ymax));
        } else {
            return rightSide(b, vec2f(p.xmax - q.xmax, p.ymax - q.ymax)) ||
                rightSide(b, vec2f(q.xmin - p.xmin, q.ymin - p.ymin));
        }
    } else if (cq.y > cp.y) {
        return rightSide(b, vec2f(q.xmax - p.xmax, q.ymax - p.ymax)) ||
            rightSide(b, vec2f(p.xmin - q.xmin, p.ymin - q.ymin));
    } else {
        return rightSide(b, vec2f(p.xmax - q.xmax, p.ymin - q.ymin)) ||
            rightSide(b, vec2f(q.xmin - p.xmin, q.ymax - p.ymax));
    }
}

static_ptr<EditorHandler> EditElevationProducer::HANDLER;

EditElevationProducer::EditElevationProducer(ptr<TileCache> cache, ptr<TileProducer> residualTiles,
    ptr<Texture2D> demTexture, ptr<Texture2D> layerTexture, ptr<Texture2D> residualTexture,
    ptr<Program> upsample, ptr<Program> blend, ptr<Module> edit, ptr<Program> brush, int gridMeshSize,
    ptr<ResourceManager> manager, const string &terrain,
    vector<float> &noiseAmp, bool flipDiagonals) : ElevationProducer()
{
    ElevationProducer::init(cache, residualTiles, demTexture, layerTexture, residualTexture, upsample, blend, gridMeshSize, noiseAmp, flipDiagonals);
    init(manager, edit, brush, terrain);
}

EditElevationProducer::EditElevationProducer() : ElevationProducer()
{
}

void EditElevationProducer::init(ptr<ResourceManager> manager, ptr<Module> edit, ptr<Program> brush, const string &terrain)
{
    assert(residualTiles.cast<EditResidualProducer>() != NULL);
    this->manager = manager.get();
    this->terrainName = terrain;
    this->terrain = NULL;
    this->terrainNode = NULL;
    this->editShader = edit;
    this->brushProg = brush;
    this->tileWidth = getCache()->getStorage()->getTileSize();
    this->brushElevation = 1.f;
    this->editMode = MAX;
    initProg = manager->loadResource("initShader;").cast<Program>();
    initSamplerU = initProg->getUniformSampler("initSampler");
    initOffsetU = initProg->getUniform4f("offset");

    brushOffsetU = brushProg->getUniform4f("offset");
    strokeU = brushProg->getUniform4f("stroke");
    strokeEndU = brushProg->getUniform4f("strokeEnd");
    brushElevationU = brushProg->getUniform1f("brushElevation");

    composeProg = manager->loadResource("composeShader;").cast<Program>();
    composeSamplerU = composeProg->getUniformSampler("initSampler");

    pencilU = NULL;
    pencilColorU = NULL;
    getEditorHandler()->addEditor(this);
}

EditElevationProducer::~EditElevationProducer()
{
    getEditorHandler()->removeEditor(this);
    if (!HANDLER->hasEditors()) {
        HANDLER = NULL;
    }
}

SceneNode *EditElevationProducer::getTerrain()
{
    if (terrain == NULL) {
        terrain = (manager->loadResource(terrainName).cast<SceneNode>()).get();
    }
    return terrain;
}

TerrainNode *EditElevationProducer::getTerrainNode()
{
    if (terrainNode == NULL) {
        terrainNode = (getTerrain()->getField("terrain").cast<TerrainNode>()).get();
    }
    return terrainNode;
}

void EditElevationProducer::setPencil(const vec4f &pencil, const vec4f &brushColor, bool paint)
{
    vec4f color = paint ? vec4f(0.5f, 0.0f, 0.0f, 0.0f) : vec4f(0.0f, 0.0f, 0.5f, 0.0f);
    brushElevation = brushColor.x;

    if (pencilU != NULL || !editShader->getUsers().empty()) {
        if (pencilU == NULL) {
            Program *p = *(editShader->getUsers().begin());
            pencilU = p->getUniform4f("pencil");
            pencilColorU = p->getUniform4f("pencilColor");
        }
        pencilU->set(pencil);
        pencilColorU->set(color);
    }
}

vec4f EditElevationProducer::getBrushColor()
{
    return vec4f(brushElevation, 0.f, 0.f, 0.f);
}

void EditElevationProducer::setEditMode(BlendEquation editMode)
{
    this->editMode = editMode;
}

BlendEquation EditElevationProducer::getEditMode()
{
    return editMode;
}

void EditElevationProducer::edit(const vector<vec4d> &strokes)
{
    ptr<FrameBuffer> old = SceneManager::getCurrentFrameBuffer();
    ptr<FrameBuffer> fb = ElevationProducer::frameBuffer;
    SceneManager::setCurrentFrameBuffer(fb);
    fb->setReadBuffer(COLOR0);
    fb->setDrawBuffer(COLOR0);
    fb->setViewport(vec4<GLint>(0, 0, tileWidth, tileWidth));
    fb->setTextureBuffer(COLOR0, demTexture, 0);
    fb->setTextureBuffer(COLOR1, layerTexture, 0);

    getTerrainNode()->deform->setUniforms(getTerrain(), getTerrainNode(), brushProg);

    float a = tileWidth / (tileWidth - 5.0f);
    float b = -2.5f / (tileWidth - 5.0f);
    brushOffsetU->set(vec4f(a / 2.0f, a / 2.0f, b + a / 2.0f, b + a / 2.0f));

    int lastStroke = strokeBounds.size();
    int newStrokes = strokes.size() - strokeBounds.size();
    for (int i = lastStroke; i < lastStroke + newStrokes; ++i) {
        vec3d s = vec3d(strokes[i].x, strokes[i].y, strokes[i].z);
        vec3d ds = getTerrain()->getWorldToLocal() * s;
        strokeBounds.push_back(getTerrainNode()->deform->deformedToLocalBounds(ds, strokes[i].w));
    }

    edit(getTerrainNode()->root, strokes, strokeBounds, newStrokes);

    fb->setTextureBuffer(COLOR1, ptr<Texture2D>(NULL), 0);

    SceneManager::setCurrentFrameBuffer(old);
}

void EditElevationProducer::edit(ptr<TerrainQuad> q, const vector<vec4d> &strokes, const vector<box2f> &strokeBounds, int newStrokes)
{
    assert(q->l >= 0);
    box2f bounds = box2f(q->ox, q->ox + q->l, q->oy, q->oy + q->l);

    unsigned int n = strokeBounds.size();
    if (n > 1) {
        // edit is called only when new strokes have been added to 'strokes'.
        // the previous strokes have already been drawn by previous calls.
        // hence we only need to draw the new stroke; we therefore look for
        // tiles that intersect the segments between the new strokes.
        bool ok = false;
        if ((int) n != newStrokes) {
            for (int i = 0; i < newStrokes; ++i) {
                if (clipStroke(bounds, strokeBounds[n - 2 - i], strokeBounds[n - 1 - i])) {
                    ok = true;
                    break;
                }
            }
        } else {
            for (int i = 0; i < (int) n - 1; ++i) {
                if (clipStroke(bounds, strokeBounds[i], strokeBounds[i + 1])) { // fix for case n == newstrokes
                    ok = true;
                    break;
                }
            }
        }
        if (!ok) {
            return;
        }
    } else if (n == 1 && !clipRectangle(bounds, strokeBounds[0])) {
        return;
    }

    ptr<FrameBuffer> fb = SceneManager::getCurrentFrameBuffer();

    if (q->isLeaf()) {
        TileCache::Tile *t = findTile(q->level, q->tx, q->ty);
        if (t != NULL) {
            GPUTileStorage::GPUSlot *s = dynamic_cast<GPUTileStorage::GPUSlot*>(t->getData(false));
            if (s != NULL) {
                // first step: copy the original tile content to 'COLOR0'
                initSamplerU->set(s->t);
                initOffsetU->set(vec4f(0.0, 0.0, 1.0, s->l));
                fb->drawQuad(initProg);

                // second step: draw the strokes in alpha channel of 'COLOR0'
                // with MAX blending (write disabled on other channels)
                brushElevationU->set(brushElevation);
                getTerrainNode()->deform->setUniforms(getTerrain(), q, brushProg);

                fb->setColorMask(false, false, false, true);
                fb->setDepthMask(false);
                fb->setBlend(true, editMode, ONE, ONE, editMode, ONE, ONE);
                if (n == 1) {
                    if (clipRectangle(bounds, strokeBounds[0])) {
                        strokeU->set(strokes[0].cast<float>());
                        strokeEndU->set(strokes[0].cast<float>());
                        fb->drawQuad(brushProg);
                    }
                } else {
                    for (unsigned int i = 1; i < n; ++i) {
                        if (clipStroke(bounds, strokeBounds[i - 1], strokeBounds[i])) {
                            strokeU->set(strokes[i - 1].cast<float>());
                            strokeEndU->set(strokes[i].cast<float>());
                            fb->drawQuad(brushProg);
                        }
                    }
                }
                fb->setBlend(false);
                fb->setColorMask(true, true, true, true);
                fb->setDepthMask(true);

                // third step: compose the rgb and alpha channel of 'COLOR0' and
                // put the result in 'COLOR1'
                fb->setDrawBuffer(COLOR1);
                composeSamplerU->set(demTexture);
                fb->drawQuad(composeProg);
                fb->setDrawBuffer(COLOR0);

                // final step: copies the result in 'COLOR1' in the original tile
                // location in tile cache
                fb->setReadBuffer(COLOR1);
                s->copyPixels(fb, 0, 0, tileWidth, tileWidth);
                fb->setReadBuffer(COLOR0);
                dynamic_cast<GPUTileStorage*>(s->getOwner())->notifyChange(s);

                editedTiles.insert(t);
                editedTileIds.insert(t->getId());
                // to recompute dependent tiles (such as normal tiles)
                invalidateTile(q->level, q->tx, q->ty);
            }
        }
    } else {
        for (unsigned int i = 0; i < 4; ++i) {
            edit(q->children[i], strokes, strokeBounds, newStrokes);
        }
    }
}

void EditElevationProducer::update()
{
    ptr<EditResidualProducer> e = residualTiles.cast<EditResidualProducer>();

    // residual and elevation tiles may not have the same size
    // -> a residual tile may correspond to several elevation tiles
    // -> logical tile coordinates may not be the same between elevation and residuals
    // -> several edited elevation tiles may correspond to only one edited residual tile
    int tileSize = tileWidth - 5;
    int residualTileSize = e->getCache()->getStorage()->getTileSize() - 5;
    int mod = residualTileSize / tileSize;

    map<Texture*, float*> textures; // buffers to read back GPU elevation tile storage textures
    map<TileCache::Tile::Id, float*> deltaElevations; // delta z tiles corresponding to the edited *residual* tiles

    // reads back edited elevation tiles and converts them to edited residual tiles
    // for each edited residual tile, computes the modifications as elevation deltas
    set<TileCache::Tile*>::iterator i = editedTiles.begin();
    while (i != editedTiles.end()) {
        TileCache::Tile *t = *i;
        GPUTileStorage::GPUSlot *s = dynamic_cast<GPUTileStorage::GPUSlot*>(t->getData(false));
        assert(s != NULL);

        // gets the GPU elevation tile storage texture containing the elevation data for t
        // reads it back from GPU if this was not already done
        float *values = NULL;
        map<Texture*, float*>::iterator j = textures.find(s->t.get());
        if (j == textures.end()) {
            values = new float[(int)( 3 * s->t->getWidth() * s->t->getHeight() * s->t->getLayers())];
            s->t->getImage(0, RGB, FLOAT, values);
            textures.insert(make_pair(s->t.get(), values));
        } else {
            values = j->second;
        }

        // gets the deta z tile for the residual tile corresponding to t
        // creates and initializes it if necessary
        float *deltaElevation = NULL;
        TileCache::Tile::Id id = make_pair(t->level, make_pair(t->tx / mod, t->ty / mod));
        map<TileCache::Tile::Id, float*>::iterator k = deltaElevations.find(id);
        if (k == deltaElevations.end()) {
            int n = residualTileSize + 1;
            deltaElevation = new float[n * n];
            for (int p = 0; p < n * n; ++p) {
                deltaElevation[p] = 0.0f;
            }
            deltaElevations.insert(make_pair(id, deltaElevation));
        } else {
            deltaElevation = k->second;
        }

        // computes the elevation deltas and copies them to 'deltaElevation'
        int rx = (t->tx % mod) * tileSize;
        int ry = (t->ty % mod) * tileSize;
        for (int y = 0; y <= tileSize; ++y) {
            for (int x = 0; x <= tileSize; ++x) {
                int srcOff = (x + 2) + (y + 2 + s->l * s->getWidth()) * s->getWidth();
                int dstOff = (x + rx) + (y + ry) * (residualTileSize + 1);
                float zf = values[3 * srcOff];
                float zm = values[3 * srcOff + 2];
                deltaElevation[dstOff] = zm - zf;
            }
        }

        i++;
    }

    // deletes temporary buffers
    map<Texture*, float*>::iterator j = textures.begin();
    while (j != textures.end()) {
        delete[] j->second;
        j++;
    }

    // sends the elevation deltas to the residual producer
    map<TileCache::Tile::Id, float*>::iterator k = deltaElevations.begin();
    while (k != deltaElevations.end()) {
        int level = k->first.first;
        int tx = k->first.second.first;
        int ty = k->first.second.second;
        float *deltaElevation = k->second;
        e->editedTile(level, tx, ty, deltaElevation);
        k++;
    }
    // tells the residual producer to recompute the residual tiles
    // according to the elevation deltas sent previously
    e->updateResiduals();

    editedTiles.clear();

    residualTiles->invalidateTiles();

    strokeBounds.clear();
}

void EditElevationProducer::reset()
{
    getEditorHandler()->relativeRadius = 0.1f;
    residualTiles.cast<EditResidualProducer>()->reset();
}

ptr<EditorHandler> EditElevationProducer::getEditorHandler()
{
    if (HANDLER == NULL) {
        HANDLER = new EditorHandler(0.1f);
    }
    return HANDLER;
}

bool EditElevationProducer::doCreateTile(int level, int tx, int ty, TileStorage::Slot *data)
{
    set<TileCache::Tile::Id>::iterator it = editedTileIds.find(TileCache::Tile::getId(level, tx, ty));
    if (it != editedTileIds.end()) {
        editedTileIds.erase(it);
        return true;
    }
    return ElevationProducer::doCreateTile(level, tx, ty, data);
}

void EditElevationProducer::swap(ptr<EditElevationProducer> p)
{
    ElevationProducer::swap(p);
    Editor::swap(p.get());
    std::swap(editedTileIds, p->editedTileIds);
    std::swap(editedTiles, p->editedTiles);
    std::swap(strokeBounds, p->strokeBounds);
    std::swap(manager, p->manager);
    std::swap(terrainName, p->terrainName);
    std::swap(terrain, p->terrain);
    std::swap(terrainNode, p->terrainNode);
    std::swap(editShader, p->editShader);
    std::swap(initProg, p->initProg);
    std::swap(initSamplerU, p->initSamplerU);
    std::swap(initOffsetU, p->initOffsetU);
    std::swap(brushOffsetU, p->brushOffsetU);
    std::swap(composeSamplerU, p->composeSamplerU);
    std::swap(pencilU, p->pencilU);
    std::swap(pencilColorU, p->pencilColorU);
    std::swap(strokeU, p->strokeU);
    std::swap(strokeEndU, p->strokeEndU);
    std::swap(brushElevationU, p->brushElevationU);
    std::swap(brushProg, p->brushProg);
    std::swap(composeProg, p->composeProg);
    std::swap(brushElevation, p->brushElevation);
    std::swap(tileWidth, p->tileWidth);
    std::swap(editMode, p->editMode);
}

class EditElevationProducerResource : public ResourceTemplate<3, EditElevationProducer>
{
public:
    EditElevationProducerResource(ptr<ResourceManager> manager, const string &name, ptr<ResourceDescriptor> desc, const TiXmlElement *e = NULL) :
        ResourceTemplate<3, EditElevationProducer>(manager, name, desc)
    {
        e = e == NULL ? desc->descriptor : e;
        checkParameters(desc, e, "name,cache,residuals,face,upsampleProg,blendProg,edit,brush,terrain,gridSize,noise,deform,flip,");
        ptr<Module> edit = manager->loadResource(getParameter(desc, e, "edit")).cast<Module>();
        ptr<Program> brush = manager->loadResource(getParameter(desc, e, "brush")).cast<Program>();
        string terrain = getParameter(desc, e, "terrain");
        if (e->Attribute("active") != NULL) {
            setActive(strcmp(e->Attribute("active"), "true") == 0);
        }
        Editor::setName(name.c_str());

        ElevationProducer::init(manager, this, name, desc, e);
        init(manager, edit, brush, terrain);
        if (layerTexture == NULL) {
            ostringstream layerTex;
            layerTex << "renderbuffer-" << getCache()->getStorage()->getTileSize() << "-RGBA32F-1";
            layerTexture = manager->loadResource(layerTex.str()).cast<Texture2D>();
        }
    }
};

extern const char editElevationProducer[] = "editElevationProducer";

static ResourceFactory::Type<editElevationProducer, EditElevationProducerResource> EditElevationProducerType;

}
