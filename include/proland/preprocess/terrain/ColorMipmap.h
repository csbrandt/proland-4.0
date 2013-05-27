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

#ifndef _PROLAND_COLOR_MIPMAP_
#define _PROLAND_COLOR_MIPMAP_

#include <string>
#include <cmath>

#include "tiffio.h"

#include "proland/preprocess/terrain/AbstractTileCache.h"

using namespace std;

namespace proland
{

class ColorMipmap : public AbstractTileCache
{
public:
    class ColorFunction
    {
    public:
        virtual vec4f getColor(int x, int y) = 0;
    };

    ColorMipmap *left;
    ColorMipmap *right;
    ColorMipmap *bottom;
    ColorMipmap *top;
    int leftr;
    int rightr;
    int bottomr;
    int topr;

    ColorMipmap(ColorFunction *colorf, int baseLevelSize, int tileSize, int border, int channels, float (*rgbToLinear)(float), float (*linearToRgb)(float), const string &cache);

    ~ColorMipmap();

    static void setCube(ColorMipmap *hm1, ColorMipmap *hm2, ColorMipmap *hm3, ColorMipmap *hm4, ColorMipmap *hm5, ColorMipmap *hm6);

    void compute();

    void computeMipmap();

    void generate(int rootLevel, int rootTx, int rootTy, bool dxt, bool jpg, int jpg_quality, const string &file);

    void generateResiduals(bool jpg, int jpg_quality, const string &in, const string &out);

    void reorderResiduals(const string &in, const string &out);

protected:
    virtual unsigned char* readTile(int tx, int ty);

    ColorFunction *colorf;

    int baseLevelSize;

    int tileSize;

    int tileWidth;

    int border;

    int channels;

    float (*r2l)(float);

    float (*l2r)(float);

    string cache;

    int maxLevel;

    unsigned char *tile;

    unsigned char *rgbaTile;

    unsigned char *dxtTile;

    int currentLevel;

    bool dxt;

    bool jpg;

    int jpg_quality;

    FILE *in;

    int iheader;

    long long *ioffsets;

    bool oJpg;

    int oJpg_quality;

    map<int, int> constantTileIds;

    unsigned char *compressedInputTile;

    unsigned char *inputTile;

    void buildBaseLevelTiles();

    void buildBaseLevelTile(int tx, int ty, TIFF *f);

    virtual void buildMipmapLevel(int level);

    void produceRawTile(int level, int tx, int ty);

    virtual void produceTile(int level, int tx, int ty);

    void produceTile(int level, int tx, int ty, long long *offset, long long *offsets, FILE *f);

    void produceTilesLebeguesOrder(int l, int level, int tx, int ty, long long *offset, long long *offsets, FILE *f);

    void readInputTile(int level, int tx, int ty);

    void convertTile(int level, int tx, int ty, unsigned char *parent);

    void convertTile(int level, int tx, int ty, unsigned char *parent, long long *offset, long long *offsets, FILE *f);

    void convertTiles(int level, int tx, int ty, unsigned char *parent, long long *outOffset, long long *outOffsets, FILE *f);

    void reorderTilesLebeguesOrder(int l, int level, int tx, int ty, long long *outOffset, long long *outOffsets, FILE *f);
};

}

#endif
