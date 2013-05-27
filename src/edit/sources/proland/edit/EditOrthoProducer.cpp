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

#include "proland/edit/EditOrthoProducer.h"

#include "ork/resource/ResourceTemplate.h"
#include "ork/scenegraph/SetTargetTask.h"
#include "ork/ui/Window.h"
#include "proland/math/geometry.h"
#include "proland/edit/EditOrthoCPUProducer.h"

namespace proland
{

bool clipStroke(const box2f &b, const box2f &p, const box2f &q);

static_ptr<EditorHandler> EditOrthoProducer::HANDLER;

EditOrthoProducer::EditOrthoProducer(ptr<TileCache> cache, ptr<TileProducer> residualTiles,
    ptr<Texture2D> orthoTexture, ptr<Texture2D> residualTexture, ptr<Texture2D> layerTexture,
    ptr<Program> upsample, vec4f rootNoiseColor, vec4f noiseColor, vector<float> &noiseAmp, bool noiseHsv,
    float scale, int maxLevel, ptr<Module> edit, ptr<Program> brush, ptr<Program> compose,
    ptr<ResourceManager> manager, const string &terrain) : OrthoProducer()
{
    OrthoProducer::init(cache, residualTiles, orthoTexture, residualTexture, upsample, rootNoiseColor, noiseColor, noiseAmp, noiseHsv, scale, maxLevel);
    init(manager, layerTexture, edit, brush, compose, terrain);
}

EditOrthoProducer::EditOrthoProducer() : OrthoProducer()
{
}

void EditOrthoProducer::init(ptr<ResourceManager> manager, ptr<Texture2D> layerTexture, ptr<Module> edit, ptr<Program> brush, ptr<Program> compose, const string &terrain)
{
    assert(residualTiles.cast<EditOrthoCPUProducer>() != NULL);
    this->layerTexture = layerTexture;
    this->manager = manager.get();
    this->terrainName = terrain;
    this->terrain = NULL;
    this->terrainNode = NULL;
    this->editShader = edit;
    this->brushProg = brush;
    this->tileWidth = getCache()->getStorage()->getTileSize();
    switch (residualTexture->getComponents()) {
    case 1:
        format = RED;
        break;
    case 2:
        format = RG;
        break;
    case 3:
        format = RGB;
        break;
    default:
        format = RGBA;
        break;
    }
    initProg = manager->loadResource("initOrthoShader;").cast<Program>();
    composeProg = compose == NULL ? manager->loadResource("composeOrthoShader;").cast<Program>() : compose;

    initSamplerU = initProg->getUniformSampler("initSampler");
    initOffsetU = initProg->getUniform4f("offset");

    brushOffsetU = brushProg->getUniform4f("offset");
    strokeU = brushProg->getUniform4f("stroke");
    strokeEndU = brushProg->getUniform4f("strokeEnd");

    composeSourceSamplerU = composeProg->getUniformSampler("sourceSampler");
    composeBrushSamplerU = composeProg->getUniformSampler("brushSampler");
    composeColorU = composeProg->getUniform4f("brushColor");

    pencilU = NULL;
    pencilColorU = NULL;

    brushColor = vec4f(1.0f, 1.0f, 1.0f, 1.0f);

    getEditorHandler()->addEditor(this);
}

EditOrthoProducer::~EditOrthoProducer()
{
    getEditorHandler()->removeEditor(this);
}

SceneNode *EditOrthoProducer::getTerrain()
{
    if (terrain == NULL) {
        terrain = (manager->loadResource(terrainName).cast<SceneNode>()).get();
    }
    return terrain;
}

TerrainNode *EditOrthoProducer::getTerrainNode()
{
    if (terrainNode == NULL) {
        terrainNode = (getTerrain()->getField("terrain").cast<TerrainNode>()).get();
    }
    return terrainNode;
}

void EditOrthoProducer::setPencil(const vec4f &pencil, const vec4f &brushColor, bool paint)
{
    vec4f col = paint ? vec4f(0.5f, 0.0f, 0.0f, 0.0f) : vec4f(0.0f, 0.0f, 0.5f, 0.0f);
    this->brushColor = brushColor;

    if (pencilU != NULL || !editShader->getUsers().empty()) {
        if (pencilU == NULL) {
            Program *p = *(editShader->getUsers().begin());
            pencilU = p->getUniform4f("pencil");
            pencilColorU = p->getUniform4f("pencilColor");
        }
        pencilU->set(pencil);
        pencilColorU->set(col);
    }
}

vec4f EditOrthoProducer::getBrushColor()
{
    return brushColor;
}

void EditOrthoProducer::edit(const vector<vec4d> &strokes)
{
    ptr<FrameBuffer> old = SceneManager::getCurrentFrameBuffer();
    ptr<FrameBuffer> fb = OrthoProducer::frameBuffer;
    SceneManager::setCurrentFrameBuffer(fb);
    fb->setReadBuffer(COLOR0);
    fb->setDrawBuffer(COLOR0);
    fb->setViewport(vec4<GLint>(0, 0, tileWidth, tileWidth));
    fb->setTextureBuffer(COLOR0, orthoTexture, 0);
    fb->setTextureBuffer(COLOR1, residualTexture, 0);
    fb->setTextureBuffer(COLOR2, layerTexture, 0);

    getTerrainNode()->deform->setUniforms(getTerrain(), getTerrainNode(), brushProg);

    float a = tileWidth / (tileWidth - 4.0f);
    float b = -2.0f / (tileWidth - 4.0f);
    brushOffsetU->set(vec4f(a / 2.0f, a / 2.0f, b + a / 2.0f, b + a / 2.0f));

    unsigned int lastStroke = strokeBounds.size();
    unsigned int newStrokes = strokes.size() - strokeBounds.size();
    for (unsigned int i = lastStroke; i < lastStroke + newStrokes; ++i) {
        vec3d s = vec3d(strokes[i].x, strokes[i].y, strokes[i].z);
        vec3d ds = getTerrain()->getWorldToLocal() * s;
        strokeBounds.push_back(getTerrainNode()->deform->deformedToLocalBounds(ds, strokes[i].w));
    }

    edit(getTerrainNode()->root, strokes, strokeBounds, newStrokes);

    fb->setTextureBuffer(COLOR1, ptr<Texture2D>(NULL), 0);
    fb->setTextureBuffer(COLOR2, ptr<Texture2D>(NULL), 0);

    SceneManager::setCurrentFrameBuffer(old);
}

void EditOrthoProducer::edit(ptr<TerrainQuad> q, const vector<vec4d> &strokes, const vector<box2f> &strokeBounds, int newStrokes)
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
        if ((int)n != newStrokes) {
            for (int i = 0; i < newStrokes; ++i) {
                if (clipStroke(bounds, strokeBounds[n - 2 - i], strokeBounds[n - 1 - i])) {
                    ok = true;
                    break;
                }
            }
        } else {
            for (unsigned int i = 0; i < n - 1; ++i) {
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
                // first step: we draw the strokes in 'COLOR0' with MAX blending
                getTerrainNode()->deform->setUniforms(getTerrain(), q, brushProg);
                fb->clear(true, false, false);
                fb->setBlend(true, MAX, ONE, ONE, MAX, ONE, ONE);
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

                // second step: we make a copy of the original tile colors in 'COLOR1'
                map<GPUTileStorage::GPUSlot*, unsigned char*>::iterator i = backupedTiles.find(s);
                if (i == backupedTiles.end()) {
                    // if the tile has never been edited yet, we copy its original colors
                    // both to 'COLOR1' and to a CPU buffer
                    backupTile(s);
                } else {
                    // otherwise we copy the original tile colors from the corresponding
                    // CPU buffer to 'COLOR1'
                    residualTexture->setSubImage(0, 0, 0, tileWidth, tileWidth, format, UNSIGNED_BYTE, Buffer::Parameters(), CPUBuffer(i->second));
                }

                // third step: we compose the mask in 'COLOR0' with the original colors
                // in 'COLOR1' and put the result in 'COLOR2'
                fb->setDrawBuffer(COLOR2);
                composeColorU->set(brushColor);
                composeSourceSamplerU->set(residualTexture);
                composeBrushSamplerU->set(orthoTexture);
                fb->drawQuad(composeProg);
                fb->setDrawBuffer(COLOR0);

                // fourth step: we copy the result in 'COLOR2' in the original tile
                // location in tile cache
                fb->setReadBuffer(COLOR2);
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

void EditOrthoProducer::update()
{
    ptr<EditOrthoCPUProducer> e = residualTiles.cast<EditOrthoCPUProducer>();

    int tileSize = tileWidth - 4;
    int channels = residualTexture->getComponents();

    map<Texture*, unsigned char*> textures; // buffers to read back GPU ortho tile storage textures
    map<TileCache::Tile::Id, int*> deltaColors; // delta RGB tiles corresponding to the edited tiles

    // reads back edited ortho tiles and converts them to edited residual tiles
    // for each edited residual tile, computes the modifications as color deltas
    set<TileCache::Tile*>::iterator i = editedTiles.begin();
    while (i != editedTiles.end()) {
        TileCache::Tile *t = *i;
        GPUTileStorage::GPUSlot *s = dynamic_cast<GPUTileStorage::GPUSlot*>(t->getData(false));
        assert(s != NULL);

        // gets the GPU ortho tile storage texture containing the color data for t
        // reads it back from GPU if this was not already done
        unsigned char *values = NULL;
        Texture *tex = s->t.get();
        map<Texture*, unsigned char*>::iterator j = textures.find(tex);
        if (j == textures.end()) {
            values = new unsigned char[channels * (int) (s->t->getWidth() * s->t->getHeight() * s->t->getLayers())];
            tex->getImage(0, s->t->getFormat(), UNSIGNED_BYTE, values);
            textures.insert(make_pair(tex, values));
        } else {
            values = j->second;
        }

        // gets the backup data (i.e. original value) for tile t
        map<GPUTileStorage::GPUSlot*, unsigned char*>::iterator k = backupedTiles.find(s);
        assert(k != backupedTiles.end());
        unsigned char* backupedTile = k->second;

        // gets the deta rgb tile corresponding to t
        // creates it if necessary
        int *deltaColor = NULL;
        TileCache::Tile::Id id = make_pair(t->level, make_pair(t->tx, t->ty));
        map<TileCache::Tile::Id, int*>::iterator l = deltaColors.find(id);
        if (l == deltaColors.end()) {
            deltaColor = new int[tileSize * tileSize * channels];
            deltaColors.insert(make_pair(id, deltaColor));
        } else {
            deltaColor = l->second;
        }

        // computes the color deltas and copies them to 'deltaColor'
        for (int y = 0; y < tileSize; ++y) {
            for (int x = 0; x < tileSize; ++x) {
                int oldOff = ((x + 2) + (y + 2) * tileWidth) * channels;
                int newOff = ((x + 2) + (y + 2 + s->l * s->getWidth()) * s->getWidth()) * channels;
                int dstOff = (x + y * tileSize) * channels;
                for (int c = 0; c < channels; ++c) {
                    deltaColor[dstOff + c] = values[newOff + c] - backupedTile[oldOff + c];
                }
            }
        }
        i++;
    }

    // deletes temporary buffers
    map<Texture*, unsigned char*>::iterator j = textures.begin();
    while (j != textures.end()) {
        delete[] j->second;
        j++;
    }

    // sends the color deltas to the residual producer
    map<TileCache::Tile::Id, int*>::iterator k = deltaColors.begin();
    while (k != deltaColors.end()) {
        int level = k->first.first;
        int tx = k->first.second.first;
        int ty = k->first.second.second;
        int *deltaColor = k->second;
        e->editedTile(level, tx, ty, deltaColor);
        k++;
    }
    // tells the residual producer to recompute the residual tiles
    // according to the elevation deltas sent previously
    e->updateTiles();

    editedTiles.clear();

    residualTiles->invalidateTiles();

    map<GPUTileStorage::GPUSlot*, unsigned char*>::iterator l = backupedTiles.begin();
    while (l != backupedTiles.end()) {
        delete[] l->second;
        l++;
    }
    backupedTiles.clear();

    strokeBounds.clear();
}

void EditOrthoProducer::reset()
{
    getEditorHandler()->relativeRadius = 0.02f;
    residualTiles.cast<EditOrthoCPUProducer>()->reset();
}

ptr<EditorHandler> EditOrthoProducer::getEditorHandler()
{
    if (HANDLER == NULL) {
        HANDLER = new EditorHandler(0.02f);
    }
    return HANDLER;
}

void EditOrthoProducer::backupTile(GPUTileStorage::GPUSlot *s)
{
    ptr<FrameBuffer> fb = SceneManager::getCurrentFrameBuffer();
    fb->setReadBuffer(COLOR1);
    fb->setDrawBuffer(COLOR1);

    assert(tileWidth == s->getWidth());
    initSamplerU->set(s->t);
    initOffsetU->set(vec4f(0.0, 0.0, 1.0, s->l));
    fb->drawQuad(initProg);

    unsigned char *data = new unsigned char[tileWidth * tileWidth * residualTexture->getComponents()];
    fb->readPixels(0, 0, tileWidth, tileWidth, format, UNSIGNED_BYTE, Buffer::Parameters(), CPUBuffer(data));
    backupedTiles.insert(make_pair(s, data));
}

bool EditOrthoProducer::doCreateTile(int level, int tx, int ty, TileStorage::Slot *data)
{
    set<TileCache::Tile::Id>::iterator it = editedTileIds.find(TileCache::Tile::getId(level, tx, ty));
    if (it != editedTileIds.end()) {
        editedTileIds.erase(it);
        return true;
    }
    return OrthoProducer::doCreateTile(level, tx, ty, data);
}

void EditOrthoProducer::swap(ptr<EditOrthoProducer> p)
{
    OrthoProducer::swap(p);
    Editor::swap(p.get());
    std::swap(layerTexture, p->layerTexture);
    std::swap(editedTileIds, p->editedTileIds);
    std::swap(editedTiles, p->editedTiles);
    std::swap(strokeBounds, p->strokeBounds);
    std::swap(manager, p->manager);
    std::swap(terrainName, p->terrainName);
    std::swap(terrain, p->terrain);
    std::swap(terrainNode, p->terrainNode);
    std::swap(editShader, p->editShader);
    std::swap(initProg, p->initProg);
    std::swap(brushProg, p->brushProg);
    std::swap(initSamplerU, p->initSamplerU);
    std::swap(initOffsetU, p->initOffsetU);
    std::swap(brushOffsetU, p->brushOffsetU);
    std::swap(strokeU, p->strokeU);
    std::swap(strokeEndU, p->strokeEndU);
    std::swap(pencilU, p->pencilU);
    std::swap(pencilColorU, p->pencilColorU);
    std::swap(composeSourceSamplerU, p->composeSourceSamplerU);
    std::swap(composeBrushSamplerU, p->composeBrushSamplerU);
    std::swap(composeColorU, p->composeColorU);
    std::swap(brushColor, p->brushColor);
    std::swap(composeProg, p->composeProg);
    std::swap(tileWidth, p->tileWidth);
    std::swap(format, p->format);
    std::swap(backupedTiles, p->backupedTiles);
}

class EditOrthoProducerResource : public ResourceTemplate<3, EditOrthoProducer>
{
public:
    EditOrthoProducerResource(ptr<ResourceManager> manager, const string &name, ptr<ResourceDescriptor> desc, const TiXmlElement *e = NULL) :
        ResourceTemplate<3, EditOrthoProducer>(manager, name, desc)
    {
        e = e == NULL ? desc->descriptor : e;
        checkParameters(desc, e, "name,group,cache,active,residuals,face,rnoise,cnoise,noise,hsv,scale,maxLevel,upsampleProg,edit,brush,compose,terrain,");
        ptr<Texture2D> layerTexture;
        ptr<Module> edit;
        ptr<Program> brush;
        ptr<Program> compose;
        string terrain;
        string group = "defaultGroup";

        edit = manager->loadResource(getParameter(desc, e, "edit")).cast<Module>();
        brush = manager->loadResource(getParameter(desc, e, "brush")).cast<Program>();
        compose = e->Attribute("compose") != NULL ? manager->loadResource(getParameter(desc, e, "compose")).cast<Program>() : NULL;
        terrain = getParameter(desc, e, "terrain");
        OrthoProducer::init(manager, this, name, desc, e);

        ostringstream layerTex;
        layerTex << "renderbuffer-" << getCache()->getStorage()->getTileSize();
        switch (getCache()->getStorage().cast<GPUTileStorage>()->getTexture(0)->getComponents()) {
        case 1:
            layerTex << "-R8";
            break;
        case 2:
            layerTex << "-RG8";
            break;
        case 3:
            layerTex << "-RGB8";
            break;
        default:
            layerTex << "-RGBA8";
            break;
        }
        layerTex << "-2";
        layerTexture = manager->loadResource(layerTex.str()).cast<Texture2D>();
        if (e->Attribute("active") != NULL) {
            setActive(strcmp(e->Attribute("active"), "true") == 0);
        }
        if (e->Attribute("group") != NULL) {
            Editor::setGroup(e->Attribute("group"));
        }
        Editor::setName(name);

        init(manager, layerTexture, edit, brush, compose, terrain);
    }
};

extern const char editOrthoProducer[] = "editOrthoProducer";

static ResourceFactory::Type<editOrthoProducer, EditOrthoProducerResource> EditOrthoProducerType;

}
