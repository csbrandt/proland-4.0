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

#include "proland/preprocess/terrain/HeightMipmap.h"

#include <cstdlib>

#include "ork/core/Object.h"
#include "proland/preprocess/terrain/Util.h"
#include "proland/util/mfs.h"

namespace proland
{

HeightMipmap::HeightMipmap(HeightFunction *height, int topLevelSize, int baseLevelSize, int tileSize, float scale, const string &cache) :
    AbstractTileCache(baseLevelSize, baseLevelSize, tileSize, 2), height(height), topLevelSize(topLevelSize), baseLevelSize(baseLevelSize), tileSize(tileSize), scale(scale), cache(cache)
{
    minLevel = 0;
    maxLevel = 0;
    int size = tileSize;
    while (size > topLevelSize) {
        minLevel += 1;
        size /= 2;
    }
    size = baseLevelSize;
    while (size > topLevelSize) {
        maxLevel += 1;
        size /= 2;
    }
    tile = new unsigned char[(tileSize + 5) * (tileSize + 5) * 2];
    constantTile = -1;
    left = NULL;
    right = NULL;
    bottom = NULL;
    top = NULL;
}

HeightMipmap::~HeightMipmap()
{
    delete[] tile;
}

void HeightMipmap::setCube(HeightMipmap *hm1, HeightMipmap *hm2, HeightMipmap *hm3, HeightMipmap *hm4, HeightMipmap *hm5, HeightMipmap *hm6)
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

void HeightMipmap::compute1()
{
    buildBaseLevelTiles();
    currentMipLevel = maxLevel - 1;
}

bool HeightMipmap::compute2()
{
    if (currentMipLevel >= 0) {
        buildMipmapLevel(currentMipLevel--);
        return true;
    } else {
        return false;
    }
}

void HeightMipmap::generate(int rootLevel, int rootTx, int rootTy, float scale, const string &file)
{
    for (int level = 1; level <= maxLevel; ++level) {
        buildResiduals(level);
    }

    if (flog(file.c_str())) {
        FILE *f;
        fopen(&f, file.c_str(), "wb");
        int nTiles = minLevel + ((1 << (max(maxLevel - minLevel, 0) * 2 + 2)) - 1) / 3;
        unsigned int *offsets = new unsigned int[nTiles * 2];
        fwrite(&minLevel, sizeof(int), 1, f);
        fwrite(&maxLevel, sizeof(int), 1, f);
        fwrite(&tileSize, sizeof(int), 1, f);
        fwrite(&rootLevel, sizeof(int), 1, f);
        fwrite(&rootTx, sizeof(int), 1, f);
        fwrite(&rootTy, sizeof(int), 1, f);
        fwrite(&scale, sizeof(float), 1, f);
        fwrite(offsets, sizeof(int) * nTiles * 2, 1, f);
        unsigned int offset = 0;
        for (int l = 0; l < minLevel; ++l) {
            produceTile(l, 0, 0, &offset, offsets, f);
        }
        for (int l = minLevel; l <= maxLevel; ++l) {
            produceTilesLebeguesOrder(l - minLevel, 0, 0, 0, &offset, offsets, f);
        }
        fseek(f, sizeof(int) * 6 + sizeof(float), SEEK_SET);
        fwrite(offsets, sizeof(int) * nTiles * 2, 1, f);
        delete[] offsets;
        fclose(f);
    }
}
unsigned char* HeightMipmap::readTile(int tx, int ty)
{
    char buf[256];
    unsigned char *data = new unsigned char[(tileSize + 5) * (tileSize + 5) * 2];
    int nTiles = max(1, (baseLevelSize / tileSize) >> (maxLevel - currentLevel));
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

void HeightMipmap::buildBaseLevelTiles()
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

void HeightMipmap::buildBaseLevelTile(int tx, int ty, TIFF *f)
{
	int off = 0;
	for (int j = -2; j <= tileSize + 2; ++j) {
		for (int i = -2; i <= tileSize + 2; ++i) {
			float h = height->getHeight(tx * tileSize + i, ty * tileSize + j);
			short sh = (short) h;
    		tile[off++] = sh & 0xFF;
			tile[off++] = sh >> 8;
		}
	}

    TIFFSetField(f, TIFFTAG_IMAGEWIDTH, tileSize + 5);
    TIFFSetField(f, TIFFTAG_IMAGELENGTH, tileSize + 5);
    TIFFSetField(f, TIFFTAG_SAMPLESPERPIXEL, 1);
    TIFFSetField(f, TIFFTAG_BITSPERSAMPLE, 16);
    TIFFSetField(f, TIFFTAG_COMPRESSION, COMPRESSION_DEFLATE);
    TIFFSetField(f, TIFFTAG_ORIENTATION, ORIENTATION_BOTLEFT);
    TIFFSetField(f, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(f, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
    TIFFWriteEncodedStrip(f, 0, tile, (tileSize + 5) * (tileSize + 5) * 2);
	TIFFWriteDirectory(f);
}

void HeightMipmap::buildMipmapLevel(int level)
{
    char buf[256];
    int nTiles = max(1, (baseLevelSize / tileSize) >> (maxLevel - level));
    int nTilesPerFile = min(nTiles, 16);

    printf("Build mipmap level %d...\n", level);

    currentLevel = level + 1;
    reset(baseLevelSize >> (maxLevel - currentLevel), baseLevelSize >> (maxLevel - currentLevel), min(topLevelSize << currentLevel, tileSize));

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
                        int currentTileSize = min(topLevelSize << level, tileSize);
                        for (int j = -2; j <= currentTileSize + 2; ++j) {
                            for (int i = -2; i <= currentTileSize + 2; ++i) {
                                int ix = 2 * (tx * currentTileSize + i);
                                int iy = 2 * (ty * currentTileSize + j);
                                /*float h1 = getTileHeight(ix, iy);
                                float h2 = getTileHeight(ix+1, iy);
                                float h3 = getTileHeight(ix, iy+1);
                                float h4 = getTileHeight(ix+1, iy+1);
                                short sh = (short) ((h1 + h2 + h3 + h4) / 4);*/
                                short sh = (short) (getTileHeight(ix, iy));
                                tile[off++] = sh & 0xFF;
                                tile[off++] = sh >> 8;
                            }
                        }

                        TIFFSetField(f, TIFFTAG_IMAGEWIDTH, currentTileSize + 5);
                        TIFFSetField(f, TIFFTAG_IMAGELENGTH, currentTileSize + 5);
                        TIFFSetField(f, TIFFTAG_SAMPLESPERPIXEL, 1);
                        TIFFSetField(f, TIFFTAG_BITSPERSAMPLE, 16);
                        TIFFSetField(f, TIFFTAG_COMPRESSION, COMPRESSION_DEFLATE);
                        TIFFSetField(f, TIFFTAG_ORIENTATION, ORIENTATION_BOTLEFT);
                        TIFFSetField(f, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
                        TIFFSetField(f, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
                        TIFFWriteEncodedStrip(f, 0, tile, (currentTileSize + 5) * (currentTileSize + 5) * 2);
                        TIFFWriteDirectory(f);
                    }
                }
                TIFFClose(f);
            }
        }
    }
}

void HeightMipmap::buildResiduals(int level)
{
    int nTiles = max(1, (baseLevelSize / this->tileSize) >> (maxLevel - level));
    int nTilesPerFile = min(nTiles, 16);
    int tileSize = min(topLevelSize << level, this->tileSize);

    printf("Build residuals level %d...\n", level);

    currentLevel = level;
    reset(baseLevelSize >> (maxLevel - currentLevel), baseLevelSize >> (maxLevel - currentLevel), min(topLevelSize << currentLevel, this->tileSize));

    float *parentTile = new float[(this->tileSize + 5) * (this->tileSize + 5)];
    float *currentTile = new float[(this->tileSize + 5) * (this->tileSize + 5)];
    float *residualTile = new float[(this->tileSize + 5) * (this->tileSize + 5)];
    unsigned char *encodedResidual = new unsigned char[(this->tileSize + 5) * (this->tileSize + 5) * 2];

    float maxRR = 0.0;
    float maxEE = 0.0;
    for (int dy = 0; dy < nTiles / nTilesPerFile; ++dy) {
        for (int dx = 0; dx < nTiles / nTilesPerFile; ++dx) {
            char buf[256];
            sprintf(buf, "%s/residual-%.2d-%.4d-%.4d.tiff", cache.c_str(), level, dx, dy);

            if (flog(buf)) {
                TIFF* f = TIFFOpen(buf, "wb");
                for (int ny = 0; ny < nTilesPerFile; ++ny) {
                    for (int nx = 0; nx < nTilesPerFile; ++nx) {
                        int tx = nx + dx * nTilesPerFile;
                        int ty = ny + dy * nTilesPerFile;
                        float maxR, meanR, maxErr;

                        getApproxTile(level - 1, tx / 2, ty / 2, parentTile);
                        getTile(level, tx, ty, currentTile);
                        computeResidual(parentTile, currentTile, level, tx, ty, residualTile, maxR, meanR);
                        encodeResidual(level, residualTile, encodedResidual);
                        computeApproxTile(parentTile, residualTile, level, tx, ty, currentTile, maxErr);
                        if (level < maxLevel) {
                            saveApproxTile(level, tx, ty, currentTile);
                        }

                        TIFFSetField(f, TIFFTAG_IMAGEWIDTH, tileSize + 5);
                        TIFFSetField(f, TIFFTAG_IMAGELENGTH, tileSize + 5);
                        TIFFSetField(f, TIFFTAG_COMPRESSION, COMPRESSION_DEFLATE);
                        TIFFSetField(f, TIFFTAG_ORIENTATION, ORIENTATION_BOTLEFT);
                        TIFFSetField(f, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
                        TIFFSetField(f, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
                        /*TIFFSetField(f, TIFFTAG_SAMPLESPERPIXEL, 1);
                        TIFFSetField(f, TIFFTAG_BITSPERSAMPLE, 16);*/
                        TIFFSetField(f, TIFFTAG_SAMPLESPERPIXEL, 2);
                        TIFFSetField(f, TIFFTAG_BITSPERSAMPLE, 8);
                        TIFFWriteEncodedStrip(f, 0, encodedResidual, (tileSize + 5) * (tileSize + 5) * 2);
                        TIFFWriteDirectory(f);

                        maxRR = max(maxR, maxRR);
                        maxEE = max(maxErr, maxEE);
                    }
                }

                TIFFClose(f);
                printf("%f max residual, %f max err\n", maxRR, maxEE);
            }
        }
    }

    delete[] parentTile;
    delete[] currentTile;
    delete[] residualTile;
    delete[] encodedResidual;
}

void rotation(int r, int n, int x, int y, int &xp, int &yp);

float HeightMipmap::getTileHeight(int x, int y)
{
    int levelSize = 1 + (baseLevelSize >> (maxLevel - currentLevel));
    if (x <= 2 && y <= 2 && left != NULL && bottom != NULL) {
        x = 0;
        y = 0;
    } else if (x > levelSize - 4 && y <= 2 && right != NULL && bottom != NULL) {
        x = levelSize - 1;
        y = 0;
    } else if (x <= 2 && y > levelSize - 4 && left != NULL && top != NULL) {
        x = 0;
        y = levelSize - 1;
    } else if (x > levelSize - 4 && y > levelSize - 4 && right != NULL && top != NULL) {
        x = levelSize - 1;
        y = levelSize - 1;
    }
    if (x < 0 && left != NULL) {
        int xp;
        int yp;
        rotation(leftr, levelSize, levelSize - 1 + x, y, xp, yp);
        assert(left->currentLevel == currentLevel);
        return left->getTileHeight(xp, yp);
    }
    if (x >= levelSize && right != NULL) {
        int xp;
        int yp;
        rotation(rightr, levelSize, x - levelSize + 1, y, xp, yp);
        assert(right->currentLevel == currentLevel);
        return right->getTileHeight(xp, yp);
    }
    if (y < 0 && bottom != NULL) {
        int xp;
        int yp;
        rotation(bottomr, levelSize, x, levelSize - 1 + y, xp, yp);
        assert(bottom->currentLevel == currentLevel);
        return bottom->getTileHeight(xp, yp);
    }
    if (y >= levelSize && top != NULL) {
        int xp;
        int yp;
        rotation(topr, levelSize, x, y - levelSize + 1, xp, yp);
        assert(top->currentLevel == currentLevel);
        return top->getTileHeight(xp, yp);
    }
    return AbstractTileCache::getTileHeight(x, y);
}

void HeightMipmap::reset(int width, int height, int tileSize)
{
    if (getWidth() != width || getHeight() != height) {
        AbstractTileCache::reset(width, height, tileSize);
        if (left != NULL) {
            left->currentLevel = currentLevel;
            left->reset(width, height, tileSize);
        }
        if (right != NULL) {
            right->currentLevel = currentLevel;
            right->reset(width, height, tileSize);
        }
        if (bottom != NULL) {
            bottom->currentLevel = currentLevel;
            bottom->reset(width, height, tileSize);
        }
        if (top != NULL) {
            top->currentLevel = currentLevel;
            top->reset(width, height, tileSize);
        }
    } else {
        if (left != NULL) {
            left->currentLevel = currentLevel;
        }
        if (right != NULL) {
            right->currentLevel = currentLevel;
        }
        if (bottom != NULL) {
            bottom->currentLevel = currentLevel;
        }
        if (top != NULL) {
            top->currentLevel = currentLevel;
        }
    }
}

void HeightMipmap::getTile(int level, int tx, int ty, float *tile)
{
    int tileSize = min(topLevelSize << level, this->tileSize);
    for (int j = 0; j <= tileSize + 4; ++j) {
        for (int i = 0; i <= tileSize + 4; ++i) {
            tile[i + j * (this->tileSize + 5)] = getTileHeight(i + tileSize * tx - 2, j + tileSize * ty - 2) / scale;
        }
    }
}

void HeightMipmap::getApproxTile(int level, int tx, int ty, float *tile)
{
    if (level == 0) {
        int oldLevel = currentLevel;
        currentLevel = 0;
        reset(topLevelSize, topLevelSize, topLevelSize);
        getTile(level, tx, ty, tile);
        currentLevel = oldLevel;
        reset(baseLevelSize >> (maxLevel - currentLevel), baseLevelSize >> (maxLevel - currentLevel), min(topLevelSize << currentLevel, this->tileSize));
        return;
    }
    char buf[256];
    sprintf(buf, "%s/%.2d-%.4d-%.4d.raw", cache.c_str(), level, tx, ty);
    FILE *f;
    fopen(&f, buf, "rb");
    fread(tile, (tileSize + 5) * (tileSize + 5) * sizeof(float), 1, f);
    fclose(f);
}

void HeightMipmap::saveApproxTile(int level, int tx, int ty, float *tile)
{
    char buf[256];
    sprintf(buf, "%s/%.2d-%.4d-%.4d.raw", cache.c_str(), level, tx, ty);
    FILE *f;
    fopen(&f, buf, "wb");
    fwrite(tile, (tileSize + 5) * (tileSize + 5) * sizeof(float), 1, f);
    fclose(f);
}

void HeightMipmap::computeResidual(float *parentTile, float *tile, int level, int tx, int ty, float *residual, float &maxR, float &meanR)
{
    maxR = 0.0;
    meanR = 0.0;
    int tileSize = min(topLevelSize << level, this->tileSize);
    int px = 1 + (tx % 2) * tileSize / 2;
    int py = 1 + (ty % 2) * tileSize / 2;
    const int n = this->tileSize + 5;
    for (int j = 0; j <= tileSize + 4; ++j) {
        for (int i = 0; i <= tileSize + 4; ++i) {
            float z;
            if (j%2 == 0) {
                if (i%2 == 0) {
                    z = parentTile[i/2+px + (j/2+py)*n];
                } else {
                    float z0 = parentTile[i/2+px-1 + (j/2+py)*n];
                    float z1 = parentTile[i/2+px + (j/2+py)*n];
                    float z2 = parentTile[i/2+px+1 + (j/2+py)*n];
                    float z3 = parentTile[i/2+px+2 + (j/2+py)*n];
                    z = ((z1+z2)*9-(z0+z3))/16;
                }
            } else {
                if (i%2 == 0) {
                    float z0 = parentTile[i/2+px + (j/2-1+py)*n];
                    float z1 = parentTile[i/2+px + (j/2+py)*n];
                    float z2 = parentTile[i/2+px + (j/2+1+py)*n];
                    float z3 = parentTile[i/2+px + (j/2+2+py)*n];
                    z = ((z1+z2)*9-(z0+z3))/16;
                } else {
                    int di, dj;
                    z = 0;
                    for (dj = -1; dj <= 2; ++dj) {
                        float f = dj == -1 || dj == 2 ? -1/16.0 : 9/16.0;
                        for (di = -1; di <= 2; ++di) {
                            float g = di == -1 || di == 2 ? -1/16.0 : 9/16.0;
                            z += f*g*parentTile[i/2+di+px + (j/2+dj+py)*n];
                        }
                    }
                }
            }
            int off = i + j * n;
            float diff = tile[off] - z;
            residual[off] = diff;
            maxR = max(diff < 0.0 ? -diff : diff, maxR);
            meanR += diff < 0.0 ? -diff : diff;
        }
    }
    meanR = meanR / (n * n);
}

void HeightMipmap::encodeResidual(int level, float *residual, unsigned char *encoded)
{
    int tileSize = min(topLevelSize << level, this->tileSize);
    for (int j = 0; j <= tileSize + 4; ++j) {
        for (int i = 0; i <= tileSize + 4; ++i) {
            int off = i + j * (this->tileSize + 5);
            short z = short(roundf(residual[off]));
            residual[off] = z;
            off = i + j * (tileSize + 5);
            encoded[2 * off] = z & 0xFF;
            encoded[2 * off + 1] = z >> 8;
        }
    }
}

void HeightMipmap::computeApproxTile(float *parentTile, float *residual, int level, int tx, int ty, float *tile, float &maxErr)
{
    maxErr = 0.0;
    int tileSize = min(topLevelSize << level, this->tileSize);
    int px = 1 + (tx % 2) * tileSize / 2;
    int py = 1 + (ty % 2) * tileSize / 2;
    const int n = this->tileSize + 5;
    for (int j = 0; j <= tileSize + 4; ++j) {
        for (int i = 0; i <= tileSize + 4; ++i) {
            float z;
            if (j%2 == 0) {
                if (i%2 == 0) {
                    z = parentTile[i/2+px + (j/2+py)*n];
                } else {
                    float z0 = parentTile[i/2+px-1 + (j/2+py)*n];
                    float z1 = parentTile[i/2+px + (j/2+py)*n];
                    float z2 = parentTile[i/2+px+1 + (j/2+py)*n];
                    float z3 = parentTile[i/2+px+2 + (j/2+py)*n];
                    z = ((z1+z2)*9-(z0+z3))/16;
                }
            } else {
                if (i%2 == 0) {
                    float z0 = parentTile[i/2+px + (j/2-1+py)*n];
                    float z1 = parentTile[i/2+px + (j/2+py)*n];
                    float z2 = parentTile[i/2+px + (j/2+1+py)*n];
                    float z3 = parentTile[i/2+px + (j/2+2+py)*n];
                    z = ((z1+z2)*9-(z0+z3))/16;
                } else {
                    int di, dj;
                    z = 0.0;
                    for (dj = -1; dj <= 2; ++dj) {
                        float f = dj == -1 || dj == 2 ? -1/16.0 : 9/16.0;
                        for (di = -1; di <= 2; ++di) {
                            float g = di == -1 || di == 2 ? -1/16.0 : 9/16.0;
                            z += f*g*parentTile[i/2+di+px + (j/2+dj+py)*n];
                        }
                    }
                }
            }
            int off = i + j * n;
            float err = tile[off] - (z + residual[off]);
            maxErr = max(err < 0.0 ? -err : err, maxErr);
            tile[off] = z + residual[off];
        }
    }
}

void HeightMipmap::produceTile(int level, int tx, int ty, unsigned int *offset, unsigned int *offsets, FILE *f)
{
    int nTiles = max(1, (baseLevelSize / this->tileSize) >> (maxLevel - level));
    int nTilesPerFile = min(nTiles, 16);
    int tileSize = min(topLevelSize << level, this->tileSize);

    if (level == 0) {
        currentLevel = 0;
        reset(tileSize, tileSize, tileSize);

        for (int j = 0; j <= tileSize + 4; ++j) {
            for (int i = 0; i <= tileSize + 4; ++i) {
                int off = i + j * (tileSize + 5);
                short z = short(roundf(getTileHeight(i - 2, j - 2) / scale));
                off = i + j * (tileSize + 5);
                tile[2 * off] = z & 0xFF;
                tile[2 * off + 1] = z >> 8;
            }
        }
    } else {
        int dx = tx / nTilesPerFile;
        int dy = ty / nTilesPerFile;
        int x = tx % nTilesPerFile;
        int y = ty % nTilesPerFile;
        char buf[256];
        sprintf(buf, "%s/residual-%.2d-%.4d-%.4d.tiff", cache.c_str(), level, dx, dy);
        TIFF* f = TIFFOpen(buf, "rb");
        TIFFSetDirectory(f, x + y * nTilesPerFile);
        TIFFReadEncodedStrip(f, 0, tile, (tsize_t) -1);
        TIFFClose(f);
    }

    int tileid;
	if (level < minLevel) {
	    tileid = level;
	} else {
        int l = max(level - minLevel, 0);
	    tileid = minLevel + tx + ty * (1 << l) + ((1 << (2 * l)) - 1) / 3;
	}

    bool isConstant = true;
    for (int i = 0; i < (tileSize + 5) * (tileSize + 5) * 2; ++i) {
        if (tile[i] != 0) {
            isConstant = false;
            break;
        }
    }

    if (isConstant && constantTile != -1) {
        offsets[2 * tileid] = offsets[2 * constantTile];
        offsets[2 * tileid + 1] = offsets[2 * constantTile + 1];
    } else {
        mfs_file fd;
        mfs_open(NULL, 0, (char*)"w", &fd);
        TIFF* tf = TIFFClientOpen("", "w", &fd,
            (TIFFReadWriteProc) mfs_read, (TIFFReadWriteProc) mfs_write, (TIFFSeekProc) mfs_lseek,
            (TIFFCloseProc) mfs_close, (TIFFSizeProc) mfs_size, (TIFFMapFileProc) mfs_map,
            (TIFFUnmapFileProc) mfs_unmap);
        TIFFSetField(tf, TIFFTAG_IMAGEWIDTH, tileSize + 5);
        TIFFSetField(tf, TIFFTAG_IMAGELENGTH, tileSize + 5);
        TIFFSetField(tf, TIFFTAG_COMPRESSION, COMPRESSION_DEFLATE);
        TIFFSetField(tf, TIFFTAG_ORIENTATION, ORIENTATION_BOTLEFT);
        TIFFSetField(tf, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
        TIFFSetField(tf, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
        /*TIFFSetField(tf, TIFFTAG_SAMPLESPERPIXEL, 1);
        TIFFSetField(tf, TIFFTAG_BITSPERSAMPLE, 16);*/
        TIFFSetField(tf, TIFFTAG_SAMPLESPERPIXEL, 2);
        TIFFSetField(tf, TIFFTAG_BITSPERSAMPLE, 8);
        TIFFWriteEncodedStrip(tf, 0, tile, (tileSize + 5) * (tileSize + 5) * 2);
        TIFFClose(tf);

        fwrite(fd.buf, fd.buf_size, 1, f);
        free(fd.buf);

        offsets[2 * tileid] = *offset;
        *offset += fd.buf_size;
        offsets[2 * tileid + 1] = *offset;
    }

    if (isConstant && constantTile == -1) {
        constantTile = tileid;
    }
}

void HeightMipmap::produceTilesLebeguesOrder(int l, int level, int tx, int ty, unsigned int *offset, unsigned int *offsets, FILE *f)
{
    if (level < l) {
        produceTilesLebeguesOrder(l, level+1, 2*tx, 2*ty, offset, offsets, f);
        produceTilesLebeguesOrder(l, level+1, 2*tx+1, 2*ty, offset, offsets, f);
        produceTilesLebeguesOrder(l, level+1, 2*tx, 2*ty+1, offset, offsets, f);
        produceTilesLebeguesOrder(l, level+1, 2*tx+1, 2*ty+1, offset, offsets, f);
    } else {
        produceTile(minLevel + level, tx, ty, offset, offsets, f);
    }
}

}
