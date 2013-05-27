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

#include "proland/preprocess/terrain/ColorMipmap.h"

#include <cstdlib>

#include "ork/core/Object.h"
#include "proland/preprocess/terrain/Util.h"
#include "proland/util/mfs.h"

#define RESIDUAL_STEPS 2

namespace proland
{

ColorMipmap::ColorMipmap(ColorFunction *colorf, int baseLevelSize, int tileSize, int border, int channels, float (*rgbToLinear)(float), float (*linearToRgb)(float), const string &cache) :
    AbstractTileCache(baseLevelSize, baseLevelSize, tileSize, channels), colorf(colorf), baseLevelSize(baseLevelSize), tileSize(tileSize), border(border), channels(channels), r2l(rgbToLinear), l2r(linearToRgb), cache(cache)
{
    maxLevel = 0;
    int size = baseLevelSize;
    while (size > tileSize) {
        maxLevel += 1;
        size /= 2;
    }
    tile = new unsigned char[(tileSize + 2*border) * (tileSize + 2*border) * channels];
    rgbaTile = new unsigned char[(tileSize + 2*border) * (tileSize + 2*border) * 4];
    dxtTile = new unsigned char[(tileSize + 2*border) * (tileSize + 2*border) * 4];
    left = NULL;
    right = NULL;
    bottom = NULL;
    top = NULL;
}

ColorMipmap::~ColorMipmap()
{
    delete[] tile;
    delete[] rgbaTile;
    delete[] dxtTile;
}

void ColorMipmap::setCube(ColorMipmap *hm1, ColorMipmap *hm2, ColorMipmap *hm3, ColorMipmap *hm4, ColorMipmap *hm5, ColorMipmap *hm6)
{
    hm1->left = hm5; hm1->right = hm3; hm1->bottom = hm2; hm1->top = hm4;
    hm2->left = hm5; hm2->right = hm3; hm2->bottom = hm6; hm2->top = hm1;
    hm3->left = hm2; hm3->right = hm4; hm3->bottom = hm6; hm3->top = hm1;
    hm4->left = hm3; hm4->right = hm5; hm4->bottom = hm6; hm4->top = hm1;
    hm5->left = hm4; hm5->right = hm2; hm5->bottom = hm6; hm5->top = hm1;
    hm6->left = hm5; hm6->right = hm3; hm6->bottom = hm4; hm6->top = hm2;
    hm1->leftr = 3; hm1->rightr = 1; hm1->bottomr = 0; hm1->topr = 2;
    hm2->leftr = 0; hm2->rightr = 0; hm2->bottomr = 0; hm2->topr = 0;
    hm3->leftr = 0; hm3->rightr = 0; hm3->bottomr = 1; hm3->topr = 3;
    hm4->leftr = 0; hm4->rightr = 0; hm4->bottomr = 2; hm4->topr = 2;
    hm5->leftr = 0; hm5->rightr = 0; hm5->bottomr = 3; hm5->topr = 1;
    hm6->leftr = 1; hm6->rightr = 3; hm6->bottomr = 2; hm6->topr = 0;
}

void ColorMipmap::compute()
{
    buildBaseLevelTiles();
    computeMipmap();
}

void ColorMipmap::computeMipmap()
{
    for (int level = maxLevel - 1; level >= 0; --level) {
        buildMipmapLevel(level);
    }
}

void ColorMipmap::generate(int rootLevel, int rootTx, int rootTy, bool dxt, bool jpg, int jpg_quality, const string &file)
{
    this->dxt = dxt;
    this->jpg = jpg;
    this->jpg_quality = jpg_quality;
    int flags = dxt ? 1 : 0;
    if (border == 0) {
		flags += 2;
	}
    int fchannels = dxt ? max(3, channels) : channels;

    if (flog(file.c_str())) {
        FILE *f;
        fopen(&f, file.c_str(), "wb");
        int nTiles = ((1 << (maxLevel * 2 + 2)) - 1) / 3;
        long long *offsets = new long long[nTiles * 2];
        fwrite(&maxLevel, sizeof(int), 1, f);
        fwrite(&tileSize, sizeof(int), 1, f);
        fwrite(&fchannels, sizeof(int), 1, f);
        fwrite(&rootLevel, sizeof(int), 1, f);
        fwrite(&rootTx, sizeof(int), 1, f);
        fwrite(&rootTy, sizeof(int), 1, f);
        fwrite(&flags, sizeof(int), 1, f);

        fwrite(offsets, sizeof(long long) * nTiles * 2, 1, f);
        long long offset = 0;
        for (int l = 0; l <= maxLevel; ++l) {
            produceTilesLebeguesOrder(l, 0, 0, 0, &offset, offsets, f);
        }
        fseek(f, sizeof(int) * 7, SEEK_SET);
        fwrite(offsets, sizeof(long long) * nTiles * 2, 1, f);
        fclose(f);

        delete[] offsets;
    }

    constantTileIds.clear();
}

void ColorMipmap::generateResiduals(bool jpg, int jpg_quality, const string &input, const string &output)
{
    if (flog(output.c_str())) {
        int root, tx, ty;
        fopen(&in, input.c_str(), "rb");
        fread(&maxLevel, sizeof(int), 1, in);
        fread(&tileSize, sizeof(int), 1, in);
        fread(&channels, sizeof(int), 1, in);
        fread(&root, sizeof(int), 1, in);
        fread(&tx, sizeof(int), 1, in);
        fread(&ty, sizeof(int), 1, in);
        int flags;
        fread(&flags, sizeof(int), 1, in);
        dxt = (flags & 1) != 0;
        border = (flags & 2) != 0 ? 0 : 2;
        int ntiles = ((1 << (maxLevel * 2 + 2)) - 1) / 3;
        iheader = 7 * sizeof(int) + 2 * ntiles * sizeof(long long);
        ioffsets = new long long[2 * ntiles];
        fread(ioffsets, sizeof(long long) * ntiles * 2, 1, in);

        tileWidth = tileSize + 2 * border;
        compressedInputTile = new unsigned char[tileWidth * tileWidth * 8];
        inputTile = new unsigned char[tileWidth * tileWidth * 4];

        oJpg = jpg;
        oJpg_quality = jpg_quality;
        int oflags = dxt ? 1 : 0;
        if (border == 0) {
            oflags += 2;
        }
        int ochannels = dxt ? max(3, channels) : channels;

        FILE *f;
        fopen(&f, output.c_str(), "wb");
        int nTiles = ((1 << (maxLevel * 2 + 2)) - 1) / 3;
        long long *offsets = new long long[nTiles * 2];
        fwrite(&maxLevel, sizeof(int), 1, f);
        fwrite(&tileSize, sizeof(int), 1, f);
        fwrite(&ochannels, sizeof(int), 1, f);
        fwrite(&root, sizeof(int), 1, f);
        fwrite(&tx, sizeof(int), 1, f);
        fwrite(&ty, sizeof(int), 1, f);
        fwrite(&oflags, sizeof(int), 1, f);

        fwrite(offsets, sizeof(long long) * nTiles * 2, 1, f);
        long long offset = 0;
        convertTiles(0, 0, 0, NULL, &offset, offsets, f);
        fseek(f, sizeof(int) * 7, SEEK_SET);
        fwrite(offsets, sizeof(long long) * nTiles * 2, 1, f);
        fclose(f);
        fclose(in);
        delete[] ioffsets;
        delete[] offsets;
        delete[] compressedInputTile;
        delete[] inputTile;
    }

    constantTileIds.clear();
}

void ColorMipmap::reorderResiduals(const string &input, const string &output)
{
    if (flog(output.c_str())) {
        int root, tx, ty;
        fopen(&in, input.c_str(), "rb");
        fread(&maxLevel, sizeof(int), 1, in);
        fread(&tileSize, sizeof(int), 1, in);
        fread(&channels, sizeof(int), 1, in);
        fread(&root, sizeof(int), 1, in);
        fread(&tx, sizeof(int), 1, in);
        fread(&ty, sizeof(int), 1, in);
        int flags;
        fread(&flags, sizeof(int), 1, in);
        dxt = (flags & 1) != 0;
        border = (flags & 2) != 0 ? 0 : 2;
        int ntiles = ((1 << (maxLevel * 2 + 2)) - 1) / 3;
        iheader = 7 * sizeof(int) + 2 * ntiles * sizeof(long long);
        ioffsets = new long long[2 * ntiles];
        fread(ioffsets, sizeof(long long) * ntiles * 2, 1, in);

        tileWidth = tileSize + 2 * border;
        compressedInputTile = new unsigned char[tileWidth * tileWidth * 8];
        inputTile = new unsigned char[tileWidth * tileWidth * 4];

        FILE *f;
        fopen(&f, output.c_str(), "wb");
        int nTiles = ((1 << (maxLevel * 2 + 2)) - 1) / 3;
        long long *offsets = new long long[nTiles * 2];
        for (int i = 0; i < 2 * nTiles; ++i) {
            offsets[i] = -1;
        }
        fwrite(&maxLevel, sizeof(int), 1, f);
        fwrite(&tileSize, sizeof(int), 1, f);
        fwrite(&channels, sizeof(int), 1, f);
        fwrite(&root, sizeof(int), 1, f);
        fwrite(&tx, sizeof(int), 1, f);
        fwrite(&ty, sizeof(int), 1, f);
        fwrite(&flags, sizeof(int), 1, f);

        fwrite(offsets, sizeof(long long) * nTiles * 2, 1, f);
        long long offset = 0;
        for (int l = 0; l <= maxLevel; ++l) {
            reorderTilesLebeguesOrder(l, 0, 0, 0, &offset, offsets, f);
        }
        fseek(f, sizeof(int) * 7, SEEK_SET);
        fwrite(offsets, sizeof(long long) * nTiles * 2, 1, f);
        fclose(f);
        fclose(in);
        delete[] compressedInputTile;
        delete[] inputTile;
        delete[] ioffsets;
        delete[] offsets;
    }
}

unsigned char* ColorMipmap::readTile(int tx, int ty)
{
    char buf[256];
    unsigned char *data = new unsigned char[(tileSize + 2*border) * (tileSize + 2*border) * channels];
    int nTiles = 1 << currentLevel;
    int nTilesPerFile = min(nTiles, 16);
    int dx = tx / nTilesPerFile;
    int dy = ty / nTilesPerFile;
    tx = tx % nTilesPerFile;
    ty = ty % nTilesPerFile;
    sprintf(buf, "%s/%.2d-%.4d-%.4d.tiff", cache.c_str(), currentLevel, dx, dy);
    TIFF* f = TIFFOpen(buf, "rb");
    TIFFSetDirectory(f, tx + ty * nTilesPerFile);
    TIFFReadEncodedStrip(f, 0, data, (tsize_t) -1);
    TIFFClose(f);
    return data;
}

void ColorMipmap::buildBaseLevelTiles()
{
    char buf[256];
    int nTiles = baseLevelSize / tileSize;
    int nTilesPerFile = min(nTiles, 16);

    printf("Build mipmap level %d...\n", maxLevel);

    for (int dy = 0; dy < nTiles / nTilesPerFile; ++dy) {
		for (int dx = 0; dx < nTiles / nTilesPerFile; ++dx) {
	   	    sprintf(buf, "%s/%.2d-%.4d-%.4d.tiff", cache.c_str(), maxLevel, dx, dy);
	   	    if (flog(buf)) {
                TIFF* f = TIFFOpen(buf, "wb");
                for (int ny = 0; ny < nTilesPerFile; ++ny) {
                    for (int nx = 0; nx < nTilesPerFile; ++nx) {
                        int tx = nx + dx * nTilesPerFile;
                        int ty = ny + dy * nTilesPerFile;
                        buildBaseLevelTile(tx, ty, f);
                    }
                }
                TIFFClose(f);
	   	    }
		}
	}
}

void ColorMipmap::buildBaseLevelTile(int tx, int ty, TIFF *f)
{
	int off = 0;
	for (int j = -border; j < tileSize + border; ++j) {
		for (int i = -border; i < tileSize + border; ++i) {
			vec4f c = colorf->getColor(tx * tileSize + i, ty * tileSize + j);
    		tile[off++] = int(roundf(c.x));
    		if (channels > 1) {
    		    tile[off++] = int(roundf(c.y));
    		}
    		if (channels > 2) {
    		    tile[off++] = int(roundf(c.z));
    		}
    		if (channels > 3) {
    		    tile[off++] = int(roundf(c.w));
    		}
		}
	}

    TIFFSetField(f, TIFFTAG_IMAGEWIDTH, tileSize + 2*border);
    TIFFSetField(f, TIFFTAG_IMAGELENGTH, tileSize + 2*border);
    TIFFSetField(f, TIFFTAG_SAMPLESPERPIXEL, channels);
    TIFFSetField(f, TIFFTAG_BITSPERSAMPLE, 8);
    TIFFSetField(f, TIFFTAG_COMPRESSION, COMPRESSION_DEFLATE);
    TIFFSetField(f, TIFFTAG_ORIENTATION, ORIENTATION_BOTLEFT);
    TIFFSetField(f, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    if (channels == 1) {
        TIFFSetField(f, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
    } else {
        TIFFSetField(f, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
    }
    TIFFWriteEncodedStrip(f, 0, tile, (tileSize + 2*border) * (tileSize + 2*border) * channels);
	TIFFWriteDirectory(f);
}

void ColorMipmap::buildMipmapLevel(int level)
{
    char buf[256];
    int nTiles = 1 << level;
    int nTilesPerFile = min(nTiles, 16);

    printf("Build mipmap level %d...\n", level);

    currentLevel = level + 1;
    reset(tileSize << currentLevel, tileSize << currentLevel, tileSize);

    for (int dy = 0; dy < nTiles / nTilesPerFile; ++dy) {
        for (int dx = 0; dx < nTiles / nTilesPerFile; ++dx) {
            sprintf(buf, "%s/%.2d-%.4d-%.4d.tiff", cache.c_str(), level, dx, dy);
            if (flog(buf)) {
                TIFF* f = TIFFOpen(buf, "wb");
                for (int ny = 0; ny < nTilesPerFile; ++ny) {
                    for (int nx = 0; nx < nTilesPerFile; ++nx) {
                        int tx = nx + dx * nTilesPerFile;
                        int ty = ny + dy * nTilesPerFile;

                        int off = 0;
                        for (int j = -border; j < tileSize + border; ++j) {
                            for (int i = -border; i < tileSize + border; ++i) {
                                int ix = 2 * (tx * tileSize + i);
                                int iy = 2 * (ty * tileSize + j);

                                vec4f c1 = getTileColor(ix, iy);
                                vec4f c2 = getTileColor(ix+1, iy);
                                vec4f c3 = getTileColor(ix, iy+1);
                                vec4f c4 = getTileColor(ix+1, iy+1);

                                tile[off++] = int(roundf(l2r((r2l(c1.x)+r2l(c2.x)+r2l(c3.x)+r2l(c4.x))/4.0)));
                                if (channels > 1) {
                                    tile[off++] = int(roundf(l2r((r2l(c1.y)+r2l(c2.y)+r2l(c3.y)+r2l(c4.y))/4.0)));
                                }
                                if (channels > 2) {
                                    tile[off++] = int(roundf(l2r((r2l(c1.z)+r2l(c2.z)+r2l(c3.z)+r2l(c4.z))/4.0)));
                                }
                                if (channels > 3) {
                                    float w1 = max(2.0 * c1.w - 255.0, 0.0);
                                    float n1 = max(255.0 - 2.0 * c1.w, 0.0);
                                    float w2 = max(2.0 * c2.w - 255.0, 0.0);
                                    float n2 = max(255.0 - 2.0 * c2.w, 0.0);
                                    float w3 = max(2.0 * c3.w - 255.0, 0.0);
                                    float n3 = max(255.0 - 2.0 * c3.w, 0.0);
                                    float w4 = max(2.0 * c4.w - 255.0, 0.0);
                                    float n4 = max(255.0 - 2.0 * c4.w, 0.0);
                                    int w = int(roundf((w1 + w2 + w3 + w4) / 4));
                                    int n = int(roundf((n1 + n2 + n3 + n4) / 4));
                                    tile[off++] = 127 + w / 2 - n / 2;
                                }
                            }
                        }

                        TIFFSetField(f, TIFFTAG_IMAGEWIDTH, tileSize + 2*border);
                        TIFFSetField(f, TIFFTAG_IMAGELENGTH, tileSize + 2*border);
                        TIFFSetField(f, TIFFTAG_SAMPLESPERPIXEL, channels);
                        TIFFSetField(f, TIFFTAG_BITSPERSAMPLE, 8);
                        TIFFSetField(f, TIFFTAG_COMPRESSION, COMPRESSION_DEFLATE);
                        TIFFSetField(f, TIFFTAG_ORIENTATION, ORIENTATION_BOTLEFT);
                        TIFFSetField(f, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
                        if (channels == 1) {
                            TIFFSetField(f, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
                        } else {
                            TIFFSetField(f, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
                        }
                        TIFFWriteEncodedStrip(f, 0, tile, (tileSize + 2*border) * (tileSize + 2*border) * channels);
                        TIFFWriteDirectory(f);
                    }
                }
                TIFFClose(f);
            }
        }
    }
}

void ColorMipmap::produceRawTile(int level, int tx, int ty)
{
    int nTiles = 1 << level;
    int nTilesPerFile = min(nTiles, 16);
    int dx = tx / nTilesPerFile;
    int dy = ty / nTilesPerFile;
    int x = tx % nTilesPerFile;
    int y = ty % nTilesPerFile;
    char buf[256];
    sprintf(buf, "%s/%.2d-%.4d-%.4d.tiff", cache.c_str(), level, dx, dy);
    TIFF* tf = TIFFOpen(buf, "rb");
    TIFFSetDirectory(tf, x + y * nTilesPerFile);
    TIFFReadEncodedStrip(tf, 0, tile, (tsize_t) -1);
    TIFFClose(tf);
}

void rotation(int r, int n, int x, int y, int &xp, int &yp)
{
    switch (r) {
        case 0:
            xp = x;
            yp = y;
            break;
        case 1:
            xp = y;
            yp = n - 1 - x;
            break;
        case 2:
            xp = n - 1 - x;
            yp = n - 1 - y;
            break;
        case 3:
            xp = n - 1 - y;
            yp = x;
            break;
    }
}

void ColorMipmap::produceTile(int level, int tx, int ty)
{
    int nTiles = 1 << level;
    int tileWidth = tileSize + 2 * border;
    produceRawTile(level, tx, ty);

    if (tx == 0 && border > 0 && left != NULL) {
        int txp, typ;
        rotation(leftr, nTiles, nTiles - 1, ty, txp, typ);
        left->produceRawTile(level, txp, typ);
        for (int y = 0; y < tileWidth; ++y) {
            for (int x = 0; x < border; ++x) {
                int xp, yp;
                rotation(leftr, tileWidth, tileSize - x, y, xp, yp);
                for (int c = 0; c < channels; ++c) {
                    tile[(x+y*tileWidth)*channels+c] = left->tile[(xp+yp*tileWidth)*channels+c];
                }
            }
        }
    }
    if (tx == nTiles - 1 && border > 0 && right != NULL) {
        int txp, typ;
        rotation(rightr, nTiles, 0, ty, txp, typ);
        right->produceRawTile(level, txp, typ);
        for (int y = 0; y < tileWidth; ++y) {
            for (int x = tileSize + border; x < tileWidth; ++x) {
                int xp, yp;
                rotation(rightr, tileWidth, x - tileSize, y, xp, yp);
                for (int c = 0; c < channels; ++c) {
                    tile[(x+y*tileWidth)*channels+c] = right->tile[(xp+yp*tileWidth)*channels+c];
                }
            }
        }
    }
    if (ty == 0 && border > 0 && bottom != NULL) {
        int txp, typ;
        rotation(bottomr, nTiles, tx, nTiles - 1, txp, typ);
        bottom->produceRawTile(level, txp, typ);
        for (int y = 0; y < border; ++y) {
            for (int x = 0; x < tileWidth; ++x) {
                int xp, yp;
                rotation(bottomr, tileWidth, x, tileSize - y, xp, yp);
                for (int c = 0; c < channels; ++c) {
                    tile[(x+y*tileWidth)*channels+c] = bottom->tile[(xp+yp*tileWidth)*channels+c];
                }
            }
        }
    }
    if (ty == nTiles - 1 && border > 0 && top != NULL) {
        int txp, typ;
        rotation(topr, nTiles, tx, 0, txp, typ);
        top->produceRawTile(level, txp, typ);
        for (int y = tileSize + border; y < tileWidth; ++y) {
            for (int x = 0; x < tileWidth; ++x) {
                int xp, yp;
                rotation(topr, tileWidth, x, y - tileSize, xp, yp);
                for (int c = 0; c < channels; ++c) {
                    tile[(x+y*tileWidth)*channels+c] = top->tile[(xp+yp*tileWidth)*channels+c];
                }
            }
        }
    }
    if (tx == 0 && ty == 0 && border > 0 && left != NULL && bottom != NULL) {
        for (int c = 0; c < channels; ++c) {
            int x1 = border;
            int y1 = border;
            int x2;
            int y2;
            int x3;
            int y3;
            rotation(leftr, tileWidth, tileWidth - 1 - border, border, x2, y2);
            rotation(bottomr, tileWidth, border, tileWidth - 1 - border, x3, y3);
            int corner1 = tile[(x1+y1*tileWidth)*channels+c];
            int corner2 = left->tile[(x2+y2*tileWidth)*channels+c];
            int corner3 = bottom->tile[(x3+y3*tileWidth)*channels+c];
            int corner = (corner1 + corner2 + corner3) / 3;
            for (int y = 0; y < 2 * border; ++y) {
                for (int x = 0; x < 2 * border; ++x) {
                    tile[(x+y*tileWidth)*channels+c] = corner;
                }
            }
        }
    }
    if (tx == nTiles - 1 && ty == 0 && border > 0 && right != NULL && bottom != NULL) {
        for (int c = 0; c < channels; ++c) {
            int x1 = tileWidth - 1 - border;
            int y1 = border;
            int x2;
            int y2;
            int x3;
            int y3;
            rotation(rightr, tileWidth, border, border, x2, y2);
            rotation(bottomr, tileWidth, tileWidth - 1 - border, tileWidth - 1 - border, x3, y3);
            int corner1 = tile[(x1+y1*tileWidth)*channels+c];
            int corner2 = right->tile[(x2+y2*tileWidth)*channels+c];
            int corner3 = bottom->tile[(x3+y3*tileWidth)*channels+c];
            int corner = (corner1 + corner2 + corner3) / 3;
            for (int y = 0; y < 2 * border; ++y) {
                for (int x = tileSize; x < tileWidth; ++x) {
                    tile[(x+y*tileWidth)*channels+c] = corner;
                }
            }
        }
    }
    if (tx == 0 && ty == nTiles - 1 && border > 0 && left != NULL && top != NULL) {
        for (int c = 0; c < channels; ++c) {
            int x1 = border;
            int y1 = tileWidth - 1 - border;
            int x2;
            int y2;
            int x3;
            int y3;
            rotation(leftr, tileWidth, tileWidth - 1 - border, tileWidth - 1 - border, x2, y2);
            rotation(topr, tileWidth, border, border, x3, y3);
            int corner1 = tile[(x1+y1*tileWidth)*channels+c];
            int corner2 = left->tile[(x2+y2*tileWidth)*channels+c];
            int corner3 = top->tile[(x3+y3*tileWidth)*channels+c];
            int corner = (corner1 + corner2 + corner3) / 3;
            for (int y = tileSize; y < tileWidth; ++y) {
                for (int x = 0; x < 2 * border; ++x) {
                    tile[(x+y*tileWidth)*channels+c] = corner;
                }
            }
        }
    }
    if (tx == nTiles - 1 && ty == nTiles - 1 && border > 0 && right != NULL && top != NULL) {
        for (int c = 0; c < channels; ++c) {
            int x1 = tileWidth - 1 - border;
            int y1 = tileWidth - 1 - border;
            int x2;
            int y2;
            int x3;
            int y3;
            rotation(rightr, tileWidth, border, tileWidth - 1 - border, x2, y2);
            rotation(topr, tileWidth, tileWidth - 1 - border, border, x3, y3);
            int corner1 = tile[(x1+y1*tileWidth)*channels+c];
            int corner2 = right->tile[(x2+y2*tileWidth)*channels+c];
            int corner3 = top->tile[(x3+y3*tileWidth)*channels+c];
            int corner = (corner1 + corner2 + corner3) / 3;
            for (int y = tileSize; y < tileWidth; ++y) {
                for (int x = tileSize; x < tileWidth; ++x) {
                    tile[(x+y*tileWidth)*channels+c] = corner;
                }
            }
        }
    }
}

void ColorMipmap::produceTile(int level, int tx, int ty, long long *offset, long long *offsets, FILE *f)
{
    produceTile(level, tx, ty);

    int tileid = tx + ty * (1 << level) + ((1 << (2 * level)) - 1) / 3;

    bool isConstant;
    int constantValue;
    if (channels == 1) {
        isConstant = true;
        for (int i = 1; i < (tileSize + 2*border) * (tileSize + 2*border); ++i) {
            if (tile[i] != tile[i - 1]) {
                isConstant = false;
                break;
            }
        }
        constantValue = tile[0];
    } else if (channels == 2) {
        isConstant = true;
        for (int i = 1; i < (tileSize + 2*border) * (tileSize + 2*border); ++i) {
            if (tile[2*i] != tile[2*(i - 1)] || tile[2*i+1] != tile[2*(i - 1)+1]) {
                isConstant = false;
                break;
            }
        }
        constantValue = (tile[0] << 8) | tile[1];
    } else if (channels == 4) {
        isConstant = true;
        int *pixels = (int*) tile;
        for (int i = 1; i < (tileSize + 2*border) * (tileSize + 2*border); ++i) {
            if (pixels[i] != pixels[i - 1]) {
                isConstant = false;
                break;
            }
        }
        constantValue = pixels[0];
    } else {
        isConstant = false;
    }

    map<int, int>::iterator it;
    if (isConstant) {
        it = constantTileIds.find(constantValue);
    }
    if (isConstant && it != constantTileIds.end()) {
        int constantId = it->second;
        offsets[2 * tileid] = offsets[2 * constantId];
        offsets[2 * tileid + 1] = offsets[2 * constantId + 1];
    } else {
        int size;
        if (dxt) {
            if (channels == 4) {
                CompressImageDXT5(tile, dxtTile, tileSize + 2*border, tileSize + 2*border, size);
                fwrite(dxtTile, size, 1, f);
            } else {
                for (int i = 0; i < (tileSize + 2*border) * (tileSize + 2*border); ++i) {
                    rgbaTile[4*i] = tile[i*channels];
                    rgbaTile[4*i+1] = tile[i*channels+min(1, channels-1)];
                    rgbaTile[4*i+2] = tile[i*channels+min(2, channels-1)];
                    rgbaTile[4*i+3] = 255;
                }
                CompressImageDXT1(rgbaTile, dxtTile, tileSize + 2*border, tileSize + 2*border, size);
                fwrite(dxtTile, size, 1, f);
            }
        } else {
            mfs_file fd;
            mfs_open(NULL, 0, (char*)"w", &fd);
            TIFF* tf = TIFFClientOpen("", (char*)"w", &fd,
                (TIFFReadWriteProc) mfs_read, (TIFFReadWriteProc) mfs_write, (TIFFSeekProc) mfs_lseek,
                (TIFFCloseProc) mfs_close, (TIFFSizeProc) mfs_size, (TIFFMapFileProc) mfs_map,
                (TIFFUnmapFileProc) mfs_unmap);
            TIFFSetField(tf, TIFFTAG_IMAGEWIDTH, tileSize + 2*border);
            TIFFSetField(tf, TIFFTAG_IMAGELENGTH, tileSize + 2*border);
            if (jpg && (tx > 0 && ty > 0 && tx < (1 << level) - 1 && ty < (1 << level) - 1)) {
                TIFFSetField(tf, TIFFTAG_COMPRESSION, COMPRESSION_JPEG);
                TIFFSetField(tf, TIFFTAG_JPEGQUALITY, jpg_quality);
            } else {
                TIFFSetField(tf, TIFFTAG_COMPRESSION, COMPRESSION_DEFLATE);
            }
            TIFFSetField(tf, TIFFTAG_ORIENTATION, ORIENTATION_BOTLEFT);
            TIFFSetField(tf, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
            if (channels == 1) {
                TIFFSetField(tf, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
            } else {
                TIFFSetField(tf, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
            }
            TIFFSetField(tf, TIFFTAG_SAMPLESPERPIXEL, channels);
            TIFFSetField(tf, TIFFTAG_BITSPERSAMPLE, 8);
            TIFFWriteEncodedStrip(tf, 0, tile, (tileSize + 2*border) * (tileSize + 2*border) * channels);
            TIFFClose(tf);

            fwrite(fd.buf, fd.buf_size, 1, f);
            free(fd.buf);
            size = fd.buf_size;
        }
        offsets[2 * tileid] = *offset;
        *offset += size;
        offsets[2 * tileid + 1] = *offset;
    }

    if (isConstant && it == constantTileIds.end()) {
        constantTileIds[constantValue] = tileid;
    }
}

void ColorMipmap::produceTilesLebeguesOrder(int l, int level, int tx, int ty, long long *offset, long long *offsets, FILE *f)
{
    if (level < l) {
        produceTilesLebeguesOrder(l, level+1, 2*tx, 2*ty, offset, offsets, f);
        produceTilesLebeguesOrder(l, level+1, 2*tx+1, 2*ty, offset, offsets, f);
        produceTilesLebeguesOrder(l, level+1, 2*tx, 2*ty+1, offset, offsets, f);
        produceTilesLebeguesOrder(l, level+1, 2*tx+1, 2*ty+1, offset, offsets, f);
    } else {
        produceTile(level, tx, ty, offset, offsets, f);
    }
}

void ColorMipmap::readInputTile(int level, int tx, int ty)
{
    int tileid = tx + ty * (1 << level) + ((1 << (2 * level)) - 1) / 3;

    int fsize = (int) (ioffsets[2 * tileid + 1] - ioffsets[2 * tileid]);
    assert(fsize < tileSize * tileSize * 8);

    fseek64(in, iheader + ioffsets[2 * tileid], SEEK_SET);
    fread(compressedInputTile, fsize, 1, in);

    mfs_file fd;
    mfs_open(compressedInputTile, fsize, (char *)"r", &fd);
    TIFF* tf = TIFFClientOpen("name", "r", &fd,
        (TIFFReadWriteProc) mfs_read, (TIFFReadWriteProc) mfs_write, (TIFFSeekProc) mfs_lseek,
        (TIFFCloseProc) mfs_close, (TIFFSizeProc) mfs_size, (TIFFMapFileProc) mfs_map,
        (TIFFUnmapFileProc) mfs_unmap);
    TIFFReadEncodedStrip(tf, 0, inputTile, (tsize_t) -1);
    TIFFClose(tf);
}

void ColorMipmap::convertTile(int level, int tx, int ty, unsigned char *parent)
{
    readInputTile(level, tx, ty);
    if (level == 0) {
        for (int i = 0; i < tileWidth * tileWidth * channels; ++i) {
            tile[i] = inputTile[i];
        }
    } else {
        int masks[4][4] = {
            { 1, 3, 3, 9 },
            { 3, 1, 9, 3 },
            { 3, 9, 1, 3 },
            { 9, 3, 3, 1 }
        };
        int rx = (tx % 2) * tileSize / 2;
        int ry = (ty % 2) * tileSize / 2;
        int overflows = 0;
        for (int y = 0; y < tileWidth; ++y) {
            int py = (y + 1) / 2 + ry;
            for (int x = 0; x < tileWidth; ++x) {
                int px = (x + 1) / 2 + rx;
                int *m = masks[(x % 2) + 2 * (y % 2)];
                for (int c = 0; c < channels; ++c) {
                    int p0 = parent[(px + py * tileWidth) * channels + c];
                    int p1 = parent[(px + 1 + py * tileWidth) * channels + c];
                    int p2 = parent[(px + (py + 1) * tileWidth) * channels + c];
                    int p3 = parent[(px + 1 + (py + 1) * tileWidth) * channels + c];
                    int off = (x + y * tileWidth) * channels + c;
                    int r = inputTile[off] - (m[0] * p0 + m[1] * p1 + m[2] * p2 + m[3] * p3) / 16;
                    r = r / RESIDUAL_STEPS + 128;
                    overflows += (r < 0 || r > 255) ? 1 : 0;
                    tile[off] = max(0, min(255, r));
                }
            }
        }
        if (overflows > 0) {
            printf("%d %d %d - %d overflows\n", level, tx, ty, overflows);
        }
    }
}

void ColorMipmap::convertTile(int level, int tx, int ty, unsigned char *parent, long long *offset, long long *offsets, FILE *f)
{
    convertTile(level, tx, ty, parent);

    int tileid = tx + ty * (1 << level) + ((1 << (2 * level)) - 1) / 3;

    bool isConstant;
    int constantValue;
    if (channels == 1) {
        isConstant = true;
        for (int i = 1; i < tileWidth * tileWidth; ++i) {
            if (tile[i] != tile[i - 1]) {
                isConstant = false;
                break;
            }
        }
        constantValue = tile[0];
    } else if (channels == 2) {
        isConstant = true;
        for (int i = 1; i < tileWidth * tileWidth; ++i) {
            if (tile[2*i] != tile[2*(i - 1)] || tile[2*i+1] != tile[2*(i - 1)+1]) {
                isConstant = false;
                break;
            }
        }
        constantValue = (tile[0] << 8) | tile[1];
    } else if (channels == 3) {
        isConstant = true;
        for (int i = 1; i < tileWidth * tileWidth; ++i) {
            if (tile[3*i] != tile[3*(i - 1)] || tile[3*i+1] != tile[3*(i - 1)+1] || tile[3*i+2] != tile[3*(i - 1)+2]) {
                isConstant = false;
                break;
            }
        }
        constantValue = (tile[0] << 16) | (tile[1] << 8) | tile[2];
    } else {
        isConstant = true;
        int *pixels = (int*) tile;
        for (int i = 1; i < tileWidth * tileWidth; ++i) {
            if (pixels[i] != pixels[i - 1]) {
                isConstant = false;
                break;
            }
        }
        constantValue = pixels[0];
    }

    map<int, int>::iterator it;
    if (isConstant) {
        it = constantTileIds.find(constantValue);
    }
    if (isConstant && it != constantTileIds.end()) {
        int constantId = it->second;
        offsets[2 * tileid] = offsets[2 * constantId];
        offsets[2 * tileid + 1] = offsets[2 * constantId + 1];
    } else {
        int size;

        mfs_file fd;
        mfs_open(NULL, 0, (char*)"w", &fd);
        TIFF* tf = TIFFClientOpen("", "w", &fd,
            (TIFFReadWriteProc) mfs_read, (TIFFReadWriteProc) mfs_write, (TIFFSeekProc) mfs_lseek,
            (TIFFCloseProc) mfs_close, (TIFFSizeProc) mfs_size, (TIFFMapFileProc) mfs_map,
            (TIFFUnmapFileProc) mfs_unmap);
        TIFFSetField(tf, TIFFTAG_IMAGEWIDTH, tileWidth);
        TIFFSetField(tf, TIFFTAG_IMAGELENGTH, tileWidth);
        if (oJpg) {
            TIFFSetField(tf, TIFFTAG_COMPRESSION, COMPRESSION_JPEG);
            TIFFSetField(tf, TIFFTAG_JPEGQUALITY, oJpg_quality);
        } else {
            TIFFSetField(tf, TIFFTAG_COMPRESSION, COMPRESSION_DEFLATE);
        }
        TIFFSetField(tf, TIFFTAG_ORIENTATION, ORIENTATION_BOTLEFT);
        TIFFSetField(tf, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
        if (channels == 1) {
            TIFFSetField(tf, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
        } else {
            TIFFSetField(tf, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
        }
        TIFFSetField(tf, TIFFTAG_SAMPLESPERPIXEL, channels);
        TIFFSetField(tf, TIFFTAG_BITSPERSAMPLE, 8);
        TIFFWriteEncodedStrip(tf, 0, tile, tileWidth * tileWidth * channels);
        TIFFClose(tf);

        if (oJpg) {
            // uncompress back into 'tile'
            mfs_open(fd.buf, fd.buf_size, (char *) "r", &fd);
            tf = TIFFClientOpen("name", "r", &fd,
                (TIFFReadWriteProc) mfs_read, (TIFFReadWriteProc) mfs_write, (TIFFSeekProc) mfs_lseek,
                (TIFFCloseProc) mfs_close, (TIFFSizeProc) mfs_size, (TIFFMapFileProc) mfs_map,
                (TIFFUnmapFileProc) mfs_unmap);
            TIFFReadEncodedStrip(tf, 0, tile, (tsize_t) -1);
            TIFFClose(tf);
        }

        fwrite(fd.buf, fd.buf_size, 1, f);
        free(fd.buf);
        size = fd.buf_size;

        offsets[2 * tileid] = *offset;
        *offset += size;
        offsets[2 * tileid + 1] = *offset;
    }

    if (isConstant && it == constantTileIds.end()) {
        constantTileIds[constantValue] = tileid;
    }

    // recomputes inputTile with uncompressed quantified residuals (lossy process)
    if (level == 0) {
        for (int i = 0; i < tileWidth * tileWidth * channels; ++i) {
            inputTile[i] = tile[i];
        }
    } else {
        int masks[4][4] = {
            { 1, 3, 3, 9 },
            { 3, 1, 9, 3 },
            { 3, 9, 1, 3 },
            { 9, 3, 3, 1 }
        };
        int rx = (tx % 2) * tileSize / 2;
        int ry = (ty % 2) * tileSize / 2;
        for (int y = 0; y < tileWidth; ++y) {
            int py = (y + 1) / 2 + ry;
            for (int x = 0; x < tileWidth; ++x) {
                int px = (x + 1) / 2 + rx;
                int *m = masks[(x % 2) + 2 * (y % 2)];
                for (int c = 0; c < channels; ++c) {
                    int p0 = parent[(px + py * tileWidth) * channels + c];
                    int p1 = parent[(px + 1 + py * tileWidth) * channels + c];
                    int p2 = parent[(px + (py + 1) * tileWidth) * channels + c];
                    int p3 = parent[(px + 1 + (py + 1) * tileWidth) * channels + c];
                    int off = (x + y * tileWidth) * channels + c;
                    int r = (tile[off] - 128) * RESIDUAL_STEPS;
                    int v = (m[0] * p0 + m[1] * p1 + m[2] * p2 + m[3] * p3) / 16 + r;
                    inputTile[off] = max(0, min(255, v));
                }
            }
        }
    }
}

void ColorMipmap::convertTiles(int level, int tx, int ty, unsigned char *parent, long long *outOffset, long long *outOffsets, FILE *f)
{
    convertTile(level, tx, ty, parent, outOffset, outOffsets, f);
    if (level < maxLevel) {
        unsigned char *p = new unsigned char[tileWidth * tileWidth * 4];
        for (int i = 0; i < tileWidth * tileWidth * 4; ++i) {
            p[i] = inputTile[i];
        }
        convertTiles(level+1, 2*tx, 2*ty, p, outOffset, outOffsets, f);
        convertTiles(level+1, 2*tx+1, 2*ty, p, outOffset, outOffsets, f);
        convertTiles(level+1, 2*tx, 2*ty+1, p, outOffset, outOffsets, f);
        convertTiles(level+1, 2*tx+1, 2*ty+1, p, outOffset, outOffsets, f);
        delete[] p;
    }
}

void ColorMipmap::reorderTilesLebeguesOrder(int l, int level, int tx, int ty, long long *outOffset, long long *outOffsets, FILE *f)
{
    if (level < l) {
        reorderTilesLebeguesOrder(l, level+1, 2*tx, 2*ty, outOffset, outOffsets, f);
        reorderTilesLebeguesOrder(l, level+1, 2*tx+1, 2*ty, outOffset, outOffsets, f);
        reorderTilesLebeguesOrder(l, level+1, 2*tx, 2*ty+1, outOffset, outOffsets, f);
        reorderTilesLebeguesOrder(l, level+1, 2*tx+1, 2*ty+1, outOffset, outOffsets, f);
    } else {
        int tileid = tx + ty * (1 << level) + ((1 << (2 * level)) - 1) / 3;
        int fsize = (int) (ioffsets[2 * tileid + 1] - ioffsets[2 * tileid]);
        assert(fsize < tileSize * tileSize * 8);
        for (int i = 0; i < 2 * tileid; i += 2) {
            if (ioffsets[i] == ioffsets[2 * tileid]) {
                if (outOffsets[i] == -1) {
                    fseek64(in, iheader + ioffsets[2 * tileid], SEEK_SET);
                    fread(compressedInputTile, fsize, 1, in);
                    fwrite(compressedInputTile, fsize, 1, f);
                    outOffsets[i] = *outOffset;
                    *outOffset += fsize;
                    outOffsets[i + 1] = *outOffset;
                }
                outOffsets[2 * tileid] = outOffsets[i];
                outOffsets[2 * tileid + 1] = outOffsets[i + 1];
                return;
            }
        }
        fseek64(in, iheader + ioffsets[2 * tileid], SEEK_SET);
        fread(compressedInputTile, fsize, 1, in);
        fwrite(compressedInputTile, fsize, 1, f);
        outOffsets[2 * tileid] = *outOffset;
        *outOffset += fsize;
        outOffsets[2 * tileid + 1] = *outOffset;
    }
}

}
