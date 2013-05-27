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

#ifndef _PROLAND_EDIT_ELEVATION_PRODUCER_H_
#define _PROLAND_EDIT_ELEVATION_PRODUCER_H_

#include "proland/dem/ElevationProducer.h"
#include "proland/edit/EditorHandler.h"

namespace proland
{

/**
 * @defgroup edit edit
 * Provides producers and layers to %edit %terrain elevations, textures, and graphs.
 * \ingroup proland
 */

/**
 * An ElevationProducer whose tiles can be edited at runtime.
 * The residual %producer used by this %producer must be an
 * EditResidualProducer.
 * The edition is performed by drawing a mask in the alpha channel
 * of the edited tiles, and then by composing this mask with the
 * elevations values in the rgb channels, to get new elevations.
 * @ingroup edit
 * @authors Eric Bruneton, Antoine Begault, Guillaume Piolat
 */
PROLAND_API class EditElevationProducer : public ElevationProducer, public Editor
{
public:
    /**
     * Creates a new EditElevationProducer. See #ElevationProducer.
     *
     * @param manager the %resource manager to load the SceneNode defining
     *      the %terrain that uses the elevations produced by this producer.
     * @param edit the shader containing a 'pencil' uniform. This uniform is
     *      set to the current pencil position and radius in world frame at
     *      each frame.
     * @param terrain the name of the SceneNode defining the %terrain that
     *      uses the elevations produced by this producer.
     */
    EditElevationProducer(ptr<TileCache> cache, ptr<TileProducer> residualTiles,
        ptr<Texture2D> demTexture, ptr<Texture2D> layerTexture, ptr<Texture2D> residualTexture,
        ptr<Program> upsample, ptr<Program> blend, ptr<Module> edit, ptr<Program> brush, int gridMeshSize,
        ptr<ResourceManager> manager, const string &terrain,
        vector<float> &noiseAmp, bool flipDiagonals = false);

    /**
     * Deletes this EditElevationProducer.
     */
    virtual ~EditElevationProducer();

    virtual SceneNode *getTerrain();

    virtual TerrainNode *getTerrainNode();

    virtual void setPencil(const vec4f &pencil, const vec4f &brushColor, bool paint);

    virtual vec4f getBrushColor();

    /**
     * Sets the edition mode. see #editMode.
     */
    virtual void setEditMode(BlendEquation editMode);

    /**
     * Returns the edition mode. see #editMode.
     */
    virtual BlendEquation getEditMode();

    virtual void edit(const vector<vec4d> &strokes);

    virtual void update();

    /**
     * Cancels all editing operations performed on this %producer.
     */
    virtual void reset();

    /**
     * Returns the EditorHandler shared by all the EditElevationProducer
     * instances.
     */
    static ptr<EditorHandler> getEditorHandler();

protected:
    /**
     * Creates an uninitialized EditElevationProducer.
     */
    EditElevationProducer();

    /**
     * Initializes this EditElevationProducer from a Resource.
     * See #EditElevationProducer.
     */
    void init(ptr<ResourceManager> manager, ptr<Module> edit, ptr<Program> brush, const string &terrain);

    virtual bool doCreateTile(int level, int tx, int ty, TileStorage::Slot *data);

    virtual void swap(ptr<EditElevationProducer> p);

private:
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
     * that uses the elevations produced by this producer.
     */
    ResourceManager *manager;

    /**
     * The name of the SceneNode defining the %terrain that uses the elevations
     * produced by this producer.
     */
    string terrainName;

    /**
     * The SceneNode defining the %terrain that uses the elevations
     * produced by this producer.
     */
    SceneNode *terrain;

    /**
     * The TerrainNode that uses the elevations produced by this producer.
     */
    TerrainNode *terrainNode;

    /**
     * The shader containing a 'pencil' uniform. This uniform is set to the
     * current pencil position and radius in world frame at each frame.
     */
    ptr<Module> editShader;

    /**
     * Program used to copy an original tile to a temporary texture to edit it.
     */
    ptr<Program> initProg;

    /**
     * Sampler used in #initProg to access an original tile content.
     */
    ptr<UniformSampler> initSamplerU;

    /**
     * Texture coordinates used in #initProg to access an original tile content.
     */
    ptr<Uniform4f> initOffsetU;

    ptr<Uniform4f> brushOffsetU;

    ptr<UniformSampler> composeSamplerU;

    ptr<Uniform4f> pencilU;

    ptr<Uniform4f> pencilColorU;

    /**
     * The start of a stroke segment (position and radius in world coordinates).
     */
    ptr<Uniform4f> strokeU;

    /**
     * The end of a stroke segment (position and radius in world coordinates).
     */
    ptr<Uniform4f> strokeEndU;

    /**
     * Altitude written by the brush.
     */
    ptr<Uniform1f> brushElevationU;

    /**
     * Program used to draw a stroke mask in an edited tile (in alpha channel).
     */
    ptr<Program> brushProg;

    /**
     * Program used to compose the stroke mask with the original elevations
     * (in rgb channels) to produce modified elevations.
     */
    ptr<Program> composeProg;

    /**
     * Altitude written by the brush.
     */
    float brushElevation;

    /**
     * The size of elevation tiles, including borders.
     */
    int tileWidth;

    /**
     * Determines how to edit the elevations. Can be either ADD or MAX.
     */
    BlendEquation editMode;

    /**
     * The EditorHandler shared by all the EditElevationProducer instances.
     */
    static static_ptr<EditorHandler> HANDLER;

    /**
     * Edits the elevation tile corresponding to the given terrain quad, and so
     * on recursively for all sub quads of this quad.
     *
     * @param q a terrain quad.
     * @param strokes the edition strokes (position and radius in world space).
     * @param strokeBounds the bounding boxes of these strokes (in terrain
     *      physical coordinates).
     * @param newStrokes how many strokes have been added to 'strokes' since the
     *      last call to this method.
     */
    void edit(ptr<TerrainQuad> q, const vector<vec4d> &strokes, const vector<box2f> &strokeBounds, int newStrokes);
};

}

#endif
