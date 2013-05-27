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

#ifndef _PROLAND_GPU_TILE_STORAGE_H_
#define _PROLAND_GPU_TILE_STORAGE_H_

#include "ork/render/FrameBuffer.h"
#include "proland/producer/TileStorage.h"

using namespace ork;

namespace proland
{

/**
 * A TileStorage that stores tiles in 2D array textures. Each tile is stored in
 * its own layer of the array.
 * @ingroup producer
 * @authors Eric Bruneton, Antoine Begault, Guillaume Piolat
 */
PROLAND_API class GPUTileStorage : public TileStorage
{
public:
    /**
     * A slot managed by a GPUTileStorage. Corresponds to a layer of a texture.
     */
    class GPUSlot : public Slot
    {
    public:
        /**
         * The 2D array texture containing the tile stored in this slot.
         */
        ptr<Texture2DArray> t;

        /**
         * The layer of the tile in the 2D texture array 't'.
         */
        const int l;

        /**
         * Creates a new GPUSlot.
         *
         * @param owner the TileStorage that manages this slot.
         * @param index the index of t in the list of textures managed by the
         *      tile storage.
         * @param t the 2D array texture in which the tile is stored.
         * @param l the layer of the tile in the 2D texture array t.
         */
        GPUSlot(TileStorage *owner, int index, ptr<Texture2DArray> t, int l);

        /**
         * Deletes this GPUSlot.
         */
        virtual ~GPUSlot();

        inline int getIndex()
        {
            return index;
        }

        inline int getWidth()
        {
            return t->getWidth();
        }

        inline int getHeight()
        {
            return t->getHeight();
        }

        /**
         * Copies a region of the given frame buffer into this slot.
         *
         * @param fb a frame buffer.
         * @param x lower left corner of the area where the pixels must be read.
         * @param y lower left corner of the area where the pixels must be read.
         * @param w width of the area where the pixels must be read.
         * @param h height of the area where the pixels must be read.
         */
        virtual void copyPixels(ptr<FrameBuffer> fb, int x, int y, int w, int h);

        /**
         * Copies a region of the given pixel buffer into this slot. The region
         * Coordinates are relative to the lower left corner of the slot.
         *
         * @param x lower left corner of the part to be replaced in this slot.
         * @param y lower left corner of the part to be replaced in this slot.
         * @param w the width of the part to be replaced in this slot.
         * @param h the height of the part to be replaced in this slot.
         * @param f the texture components in 'pixels'.
         * @param t the type of each component in 'pixels'.
         * @param pixels the pixels to be copied into this slot.
         */
        virtual void setSubImage(int x, int y, int w, int h, TextureFormat f, PixelType t, const Buffer::Parameters &s, const Buffer &pixels);

    private:
        /**
         * The index of 't' in the list of textures managed by the tile storage.
         */
        int index;

        friend class GPUTileStorage;
    };

    /**
     * Creates a new GPUTileStorage. See #init.
     */
    GPUTileStorage(int tileSize, int nTiles,
        TextureInternalFormat internalf, TextureFormat f, PixelType t,
        const Texture::Parameters &params, bool useTileMap = false);

    /**
     * Deletes this GPUTileStorage.
     */
    virtual ~GPUTileStorage();

    /**
     * Returns the number of textures used to store the tiles.
     */
    int getTextureCount();

    /**
     * Returns the texture storage whose index is given.
     *
     * @param index an index between 0 and #getTextureCount (excluded).
     */
    ptr<Texture2DArray> getTexture(int index);

    /**
     * Returns the tile map that stores the mapping between logical tile
     * coordinates (level,tx,ty) and storage tile coordinates in this
     * storage. This mapping texture can be used as an indirection texture on GPU
     * to find the content of a tile from its logical coordinates. May be NULL.
     */
    ptr<Texture2D> getTileMap();

    /**
     * Notifies this manager that the content of the given slot has changed.
     *
     * @param s a slot whose content has changed.
     */
    void notifyChange(GPUSlot *s);

    /**
     * Generates the mipmap levels of the storage textures. This method only
     * updates the textures whose content has changed since the last call to
     * this method. Changes must be notified with #notifyChange.
     */
    void generateMipMap();

protected:
    /**
     * Creates an uninitialized GPUTileStorage.
     */
    GPUTileStorage();

    /**
     * Initializes this GPUTileStorage.
     *
     * @param tileSize the size in pixel of each (square) tile.
     * @param nTiles the number of slots in this storage.
     * @param tf the texture storage data format on GPU.
     * @param f the texture components in the storage textures.
     * @param t the type of each component in the storage textures.
     * @param params the texture parameters.
     * @param useTileMap the to associate with this tile storage a texture
     *      representing the mapping between logical tile coordinates
     *      (level,tx,ty) and storage tile coordinates (u,v) in this storage.
     *      This mapping texture can be used as an indirection texture on GPU
     *      to find the content of a tile from its logical coordinates. <i>This
     *      option can only be used if nTextures is equal to one</i>.
     */
    void init(int tileSize, int nTiles,
        TextureInternalFormat internalf, TextureFormat f, PixelType t,
        const Texture::Parameters &params, bool useTileMap = false);

    void swap(ptr<GPUTileStorage> s);

private:
    /**
     * The storage textures used to store the tiles.
     */
    vector< ptr<Texture2DArray> > textures;

    /**
     * True if the storage texture format needs mipmaping.
     */
    bool needMipmaps;

    /**
     * True if a least one storage texture has changed since the last call to
     * #generateMipMap.
     */
    bool changes;

    /**
     * The slots whose mipmap levels are not up to date (one set per texture).
     */
    set<GPUSlot*> *dirtySlots;

    /**
     * Framebuffer used to generate mipmaps.
     */
    ptr<FrameBuffer> fbo;

    /**
     * Program used to generate mipmaps.
     */
    ptr<Program> mipmapProg;

    /**
     * Parameters used to generate a mipmap level.
     */
    ptr<Uniform4i> mipmapParams;

    /**
     * The tile map that stores the mapping between logical tile coordinates
     * (level,tx,ty) and storage tile coordinates (u,v) in this storage. May
     * be NULL.
     */
    ptr<Texture2D> tileMap;
};

}

#endif
