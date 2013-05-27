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

#ifndef _PROLAND_HEIGHT_MIPMAP_
#define _PROLAND_HEIGHT_MIPMAP_

#include <string>
#include <cmath>

#include "tiffio.h"

#include "proland/preprocess/terrain/AbstractTileCache.h"

namespace proland
{

class HeightMipmap : public AbstractTileCache
{
public:
    class HeightFunction
    {
    public:
        virtual float getHeight(int x, int y) = 0;
    };

    HeightMipmap *left;
    HeightMipmap *right;
    HeightMipmap *bottom;
    HeightMipmap *top;
    int leftr;
    int rightr;
    int bottomr;
    int topr;

    HeightMipmap(HeightFunction *height, int topLevelSize, int baseLevelSize, int tileSize, float scale, const string &cache);

    ~HeightMipmap();

    static void setCube(HeightMipmap *hm1, HeightMipmap *hm2, HeightMipmap *hm3, HeightMipmap *hm4, HeightMipmap *hm5, HeightMipmap *hm6);

    void compute1();

    bool compute2();

    void generate(int rootLevel, int rootTx, int rootTy, float scale, const string &file);

    virtual float getTileHeight(int x, int y);

    virtual void reset(int width, int height, int tileSize);

protected:
    virtual unsigned char* readTile(int tx, int ty);

    virtual void getTile(int level, int tx, int ty, float *tile);

private:
    HeightFunction *height;

    int topLevelSize;

    int baseLevelSize;

    int tileSize;

    float scale;

    string cache;

    int minLevel;

    int maxLevel;

    int currentMipLevel;

    unsigned char *tile;

    int currentLevel;

    int constantTile;

    void buildBaseLevelTiles();

    void buildBaseLevelTile(int tx, int ty, TIFF *f);

    void buildMipmapLevel(int level);

    void buildResiduals(int level);

    void getApproxTile(int level, int tx, int ty, float *tile);

    void saveApproxTile(int level, int tx, int ty, float *tile);

    void computeResidual(float *parentTile, float *tile, int level, int tx, int ty, float *residual, float &maxR, float &meanR);

    void encodeResidual(int level, float *residual, unsigned char *encoded);

    void computeApproxTile(float *parentTile, float *residual, int level, int tx, int ty, float *tile, float &maxErr);

    void produceTile(int level, int tx, int ty, unsigned int *offset, unsigned int *offsets, FILE *f);

    void produceTilesLebeguesOrder(int l, int level, int tx, int ty, unsigned int *offset, unsigned int *offsets, FILE *f);
};

}

#endif
