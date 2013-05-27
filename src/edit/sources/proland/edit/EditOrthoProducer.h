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

#ifndef _PROLAND_EDIT_ORTHO_PRODUCER_H_
#define _PROLAND_EDIT_ORTHO_PRODUCER_H_

#include "proland/producer/GPUTileStorage.h"
#include "proland/ortho/OrthoProducer.h"
#include "proland/edit/EditorHandler.h"

namespace proland
{

/**
 * An OrthoProducer whose tiles can be edited at runtime.
 * The residual %producer used by this %producer must be an
 * EditOrthoCPUProducer.
 * The edition is performed by drawing a mask in a temporary
 * texture, and then by composing this mask with the oringinal
 * tile colors, to get new colors.
 * @ingroup edit
 * @authors Eric Bruneton, Antoine Begault, Guillaume Piolat
 */
PROLAND_API class EditOrthoProducer : public OrthoProducer, public Editor
{
public:
    /**
     * Creates a new EditOrthoProducer. See #OrthoProducer.
     *
     * @param manager the %resource manager to load the SceneNode defining
     *      the %terrain that uses the colors produced by this producer.
     * @param layerTexture a temporary texture used to draw a stroke mask,
     *      before compositing it with tile colors.
     * @param edit the shader containing a 'pencil' uniform. This uniform is
     *      set to the current pencil position and radius in world frame at
     *      each frame.
     * @param terrain the name of the SceneNode defining the %terrain that
     *      uses the colors produced by this producer.
     */
    EditOrthoProducer(ptr<TileCache> cache, ptr<TileProducer> residualTiles,
        ptr<Texture2D> orthoTexture, ptr<Texture2D> residualTexture, ptr<Texture2D> layerTexture,
        ptr<Program> upsample, vec4f rootNoiseColor, vec4f noiseColor, vector<float> &noiseAmp, bool noiseHsv,
        float scale, int maxLevel, ptr<Module> edit, ptr<Program> brush, ptr<Program> compose,
        ptr<ResourceManager> manager, const string &terrain);

    /**
     * Deletes this EditOrthoProducer.
     */
    virtual ~EditOrthoProducer();

    virtual SceneNode *getTerrain();

    virtual TerrainNode *getTerrainNode();

    virtual void setPencil(const vec4f &pencil, const vec4f &brushColor, bool paint);

    virtual vec4f getBrushColor();

    virtual void edit(const vector<vec4d> &strokes);

    virtual void update();

    /**
     * Cancels all editing operations performed on this %producer.
     */
    virtual void reset();

    /**
     * Returns the EditorHandler shared by all the EditOrthoProducer instances.
     */
    static ptr<EditorHandler> getEditorHandler();

protected:
    /**
     * Creates an uninitialized EditOrthoProducer.
     */
    EditOrthoProducer();

    /**
     * Initializes this EditOrthoProducer from a Resource.
     * See EditOrthoProducer.
     */
    void init(ptr<ResourceManager> manager, ptr<Texture2D> layerTexture, ptr<Module> edit, ptr<Program> brush, ptr<Program> compose, const string &terrain);

    virtual bool doCreateTile(int level, int tx, int ty, TileStorage::Slot *data);

    virtual void swap(ptr<EditOrthoProducer> p);

private:
    /**
     * A temporary texture used to draw a stroke mask, before compositing
     * it with tile colors.
     */
    ptr<Texture2D> layerTexture;

    /**
     * The Tile ids of the tiles that have been edited since the last
     * call to #update.
     */
    set<TileCache::Tile::Id> editedTileIds;

    /**
     * The tiles that have been edited since the last call to #update.
     */
    set<TileCache::Tile*> editedTiles;

    /**
     * The bounding boxes of the edit strokes in terrain physical coordinates.
     */
    vector<box2f> strokeBounds;

    /**
     * The %resource manager used to load the SceneNode defining the %terrain
     * that uses the colors produced by this producer.
     */
    ResourceManager *manager;

    /**
     * The name of the SceneNode defining the %terrain that uses the colors
     * produced by this producer.
     */
    string terrainName;

    /**
     * The SceneNode defining the %terrain that uses the colors
     * produced by this producer.
     */
    SceneNode *terrain;

    /**
     * The TerrainNode that uses the colors produced by this producer.
     */
    TerrainNode *terrainNode;

    /**
     * The shader containing a 'pencil' uniform. This uniform is set to the
     * current pencil position and radius in world frame at each frame.
     */
    ptr<Module> editShader;

    /**
     * Program used to copy an original tile to a temporary CPU buffer to
     * edit it.
     */
    ptr<Program> initProg;

    /**
     * Program used to draw a stroke mask #layerTexture.
     */
    ptr<Program> brushProg;

    /**
     * The GLSL sampler that contains the copied texture.
     */
    ptr<UniformSampler> initSamplerU;

    /**
     * Texture coordinates used in #initProg to access an original tile content.
     */
    ptr<Uniform4f> initOffsetU;

    /**
     * Brush coordinates used in #brushProg.
     */
    ptr<Uniform4f> brushOffsetU;

    /**
     * The start of a stroke segment (position and radius in world coordinates).
     */
    ptr<Uniform4f> strokeU;

    /**
     * The end of a stroke segment (position and radius in world coordinates).
     */
    ptr<Uniform4f> strokeEndU;

    /**
     * Pencil informations (position & size) used in #brushProg.
     */
    ptr<Uniform4f> pencilU;

    /**
     * Pencil color used in #brushProg.
     */
    ptr<Uniform4f> pencilColorU;

    /**
     * Sampler used in #backupProg and #composeProg to access an original
     * tile content.
     */
    ptr<UniformSampler> composeSourceSamplerU;

    /**
     * Sampler used in #composeProg to access the stroke mask to be composed
     * with the original tile colors.
     */
    ptr<UniformSampler> composeBrushSamplerU;

    /**
     * Color of the added strokes.
     */
    ptr<Uniform4f> composeColorU;

    /**
     * color of the added strokes.
     */
    vec4f brushColor;

    /**
     * Program used to compose the stroke mask with original colors
     * to produce modified colors.
     */
    ptr<Program> composeProg;

    /**
     * The size of the ortho tiles, including borders.
     */
    int tileWidth;

    /**
     * The texture format of the ortho tiles.
     */
    TextureFormat format;

    /**
     * A copy of the original colors of the edited tiles in CPU buffers.
     */
    map<GPUTileStorage::GPUSlot*, unsigned char*> backupedTiles;

    /**
     * The EditorHandler shared by all the EditOrthoProducer instances.
     */
    static static_ptr<EditorHandler> HANDLER;

    /**
     * Edits the color tile corresponding to the given terrain quad, and so
     * on recursively for all sub quads of this quad.
     *
     * @param q terrain quad.
     * @param strokes the edition strokes (position and radius in world space).
     * @param strokeBounds the bounding boxes of these strokes (in terrain
     *      physical coordinates).
     * @param newStrokes how many strokes have been added to 'strokes' since the
     *      last call to this method.
     */
    void edit(ptr<TerrainQuad> q, const vector<vec4d> &strokes, const vector<box2f> &strokeBounds, int newStrokes);

    /**
     * Makes a copy of the given tile in #backupTiles and in #residualTexture.
     */
    void backupTile(GPUTileStorage::GPUSlot *s);
};

}

#endif
