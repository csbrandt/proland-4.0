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

#include "proland/preprocess/terrain/Preprocess.h"

#include <sys/types.h>
#include <sys/stat.h>

#include "ork/core/Object.h"
#include "proland/preprocess/terrain/ApertureMipmap.h"
#include "proland/preprocess/terrain/ColorMipmap.h"
#include "proland/preprocess/terrain/HeightMipmap.h"
#include "proland/preprocess/terrain/Util.h"

#define RGB_JPEG_QUALITY 90

namespace proland
{

InputMap::Tile::Tile(int tx, int ty, float *data) :
    tx(tx), ty(ty), data(data)
{
}

InputMap::Tile::~Tile()
{
    if (data != NULL) {
        delete[] data;
    }
}

InputMap::InputMap(int width, int height, int channels, int tileSize, int capacity) :
    width(width), height(height), channels(channels), tileSize(tileSize), capacity(capacity)
{
    assert(tileSize > 0);
    assert(width % tileSize == 0);
    assert(height % tileSize == 0);
}

InputMap::~InputMap()
{
}

float* InputMap::getValues(int x, int y)
{
    float *v = new float[tileSize * tileSize * channels];
    for (int j = 0; j < tileSize; ++j) {
        for (int i = 0; i < tileSize; ++i) {
            vec4f value = getValue(x + i, y + j);
            int off = (i + j * tileSize) * channels;
            v[off] = value.x;
            if (channels > 1) {
                v[off + 1] = value.y;
            }
            if (channels > 2) {
                v[off + 2] = value.z;
            }
            if (channels > 3) {
                v[off + 3] = value.w;
            }
        }
    }
    return v;
}

float* InputMap::getTile(int tx, int ty)
{
    pair<int, int> key = make_pair(tx, ty);
    Cache::iterator i = tileCache.find(key);
    if (i == tileCache.end()) {
        float *data = getValues(tx * tileSize, ty * tileSize);

        list<Tile*>::iterator li;
        if ((int) tileCache.size() == 20) {
            // evict least recently used tile if cache is full
            li = tileCacheOrder.begin();
            Tile *t = *li;
            tileCache.erase(make_pair(t->tx, t->ty));
            tileCacheOrder.erase(li);
            delete t;
        }

        // create tile, put it at the end of tileCacheOrder, and update the map
        Tile *t = new Tile(tx, ty, data);
        li = tileCacheOrder.insert(tileCacheOrder.end(), t);
        tileCache[key] = li;
        assert(tileCache.find(key) != tileCache.end());
        return data;
    } else {
        list<Tile*>::iterator li = i->second;
        list<Tile*>::iterator lj = li;
        Tile *t = *li;
        assert(t->tx == tx && t->ty == ty);
        // put t at the end of tileCacheOrder, and update the map accordingly
        lj++;
        if (lj != tileCacheOrder.end()) {
            tileCacheOrder.erase(li);
            li = tileCacheOrder.insert(tileCacheOrder.end(), t);
            tileCache[key] = li;
        }
        return t->data;
    }
}

vec4f InputMap::get(int x, int y)
{
    x = max(min(x, width - 1), 0);
    y = max(min(y, height - 1), 0);
    int tx = x / tileSize;
    int ty = y / tileSize;
    x = x % tileSize;
    y = y % tileSize;
    int off = (x + y * tileSize) * channels;
    float *data = getTile(tx, ty);
    vec4f c;
    c.x = data[off];
    if (channels > 1) {
        c.y = data[off + 1];
    }
    if (channels > 2) {
        c.z = data[off + 2];
    }
    if (channels > 3) {
        c.w = data[off + 3];
    }
    return c;
}

typedef void (*projectionFunction)(int x, int y, int w, double &sx, double &sy, double &sz);

void projection1(int x, int y, int w, double &sx, double &sy, double &sz) // north pole
{
    double xl = (max(double(min(x, w)), 0.0)) / w * 2.0 - 1.0;
    double yl = (max(double(min(y, w)), 0.0)) / w * 2.0 - 1.0;
    double l =  sqrt(xl * xl + yl * yl + 1.0);
    sx = xl / l;
    sy = yl / l;
    sz = 1.0 / l;
}

void projection2(int x, int y, int w, double &sx, double &sy, double &sz) // face 1
{
    double xl = (max(double(min(x, w)), 0.0)) / w * 2.0 - 1.0;
    double yl = (max(double(min(y, w)), 0.0)) / w * 2.0 - 1.0;
    double l =  sqrt(xl * xl + yl * yl + 1.0);
    sx = xl / l;
    sy = -1.0 / l;
    sz = yl / l;
}

void projection3(int x, int y, int w, double &sx, double &sy, double &sz) // face 2
{
    double xl = (max(double(min(x, w)), 0.0)) / w * 2.0 - 1.0;
    double yl = (max(double(min(y, w)), 0.0)) / w * 2.0 - 1.0;
    double l =  sqrt(xl * xl + yl * yl + 1.0);
    sx = 1.0 / l;
    sy = xl / l;
    sz = yl / l;
}

void projection4(int x, int y, int w, double &sx, double &sy, double &sz) // face 3
{
    double xl = (max(double(min(x, w)), 0.0)) / w * 2.0 - 1.0;
    double yl = (max(double(min(y, w)), 0.0)) / w * 2.0 - 1.0;
    double l =  sqrt(xl * xl + yl * yl + 1.0);
    sx = -xl / l;
    sy = 1.0 / l;
    sz = yl / l;
}

void projection5(int x, int y, int w, double &sx, double &sy, double &sz) // face 4
{
    double xl = (max(double(min(x, w)), 0.0)) / w * 2.0 - 1.0;
    double yl = (max(double(min(y, w)), 0.0)) / w * 2.0 - 1.0;
    double l =  sqrt(xl * xl + yl * yl + 1.0);
    sx = -1.0 / l;
    sy = -xl / l;
    sz = yl / l;
}

void projection6(int x, int y, int w, double &sx, double &sy, double &sz) // south pole
{
    double xl = (max(double(min(x, w)), 0.0)) / w * 2.0 - 1.0;
    double yl = (max(double(min(y, w)), 0.0)) / w * 2.0 - 1.0;
    double l =  sqrt(xl * xl + yl * yl + 1.0);
    sx = xl / l;
    sy = -yl / l;
    sz = -1.0 / l;
}

void projection1f(double x, double y, double w, double &sx, double &sy, double &sz) // north pole
{
    double xl = x / w * 2.0 - 1.0;
    double yl = y / w * 2.0 - 1.0;
    double l =  sqrt(xl * xl + yl * yl + 1.0);
    sx = xl / l;
    sy = yl / l;
    sz = 1.0 / l;
}

void projection2f(double x, double y, double w, double &sx, double &sy, double &sz) // face 1
{
    double xl = x / w * 2.0 - 1.0;
    double yl = y / w * 2.0 - 1.0;
    double l =  sqrt(xl * xl + yl * yl + 1.0);
    sx = xl / l;
    sy = -1.0 / l;
    sz = yl / l;
}

void projection3f(double x, double y, double w, double &sx, double &sy, double &sz) // face 2
{
    double xl = x / w * 2.0 - 1.0;
    double yl = y / w * 2.0 - 1.0;
    double l =  sqrt(xl * xl + yl * yl + 1.0);
    sx = 1.0 / l;
    sy = xl / l;
    sz = yl / l;
}

void projection4f(double x, double y, double w, double &sx, double &sy, double &sz) // face 3
{
    double xl = x / w * 2.0 - 1.0;
    double yl = y / w * 2.0 - 1.0;
    double l =  sqrt(xl * xl + yl * yl + 1.0);
    sx = -xl / l;
    sy = 1.0 / l;
    sz = yl / l;
}

void projection5f(double x, double y, double w, double &sx, double &sy, double &sz) // face 4
{
    double xl = x / w * 2.0 - 1.0;
    double yl = y / w * 2.0 - 1.0;
    double l =  sqrt(xl * xl + yl * yl + 1.0);
    sx = -1.0 / l;
    sy = -xl / l;
    sz = yl / l;
}

void projection6f(double x, double y, double w, double &sx, double &sy, double &sz) // south pole
{
    double xl = x / w * 2.0 - 1.0;
    double yl = y / w * 2.0 - 1.0;
    double l =  sqrt(xl * xl + yl * yl + 1.0);
    sx = xl / l;
    sy = -yl / l;
    sz = -1.0 / l;
}

void projection1h(int x, int y, int w, double &sx, double &sy, double &sz) // north pole
{
    double xl = (max(min(x, w - 1), 0) + 0.5) / w * 2.0 - 1.0;
    double yl = (max(min(y, w - 1), 0) + 0.5) / w * 2.0 - 1.0;
    double l =  sqrt(xl * xl + yl * yl + 1.0);
    sx = xl / l;
    sy = yl / l;
    sz = 1.0 / l;
}

void projection2h(int x, int y, int w, double &sx, double &sy, double &sz) // face 1
{
    double xl = (max(min(x, w - 1), 0) + 0.5) / w * 2.0 - 1.0;
    double yl = (max(min(y, w - 1), 0) + 0.5) / w * 2.0 - 1.0;
    double l =  sqrt(xl * xl + yl * yl + 1.0);
    sx = xl / l;
    sy = -1.0 / l;
    sz = yl / l;
}

void projection3h(int x, int y, int w, double &sx, double &sy, double &sz) // face 2
{
    double xl = (max(min(x, w - 1), 0) + 0.5) / w * 2.0 - 1.0;
    double yl = (max(min(y, w - 1), 0) + 0.5) / w * 2.0 - 1.0;
    double l =  sqrt(xl * xl + yl * yl + 1.0);
    sx = 1.0 / l;
    sy = xl / l;
    sz = yl / l;
}

void projection4h(int x, int y, int w, double &sx, double &sy, double &sz) // face 3
{
    double xl = (max(min(x, w - 1), 0) + 0.5) / w * 2.0 - 1.0;
    double yl = (max(min(y, w - 1), 0) + 0.5) / w * 2.0 - 1.0;
    double l =  sqrt(xl * xl + yl * yl + 1.0);
    sx = -xl / l;
    sy = 1.0 / l;
    sz = yl / l;
}

void projection5h(int x, int y, int w, double &sx, double &sy, double &sz) // face 4
{
    double xl = (max(min(x, w - 1), 0) + 0.5) / w * 2.0 - 1.0;
    double yl = (max(min(y, w - 1), 0) + 0.5) / w * 2.0 - 1.0;
    double l =  sqrt(xl * xl + yl * yl + 1.0);
    sx = -1.0 / l;
    sy = -xl / l;
    sz = yl / l;
}

void projection6h(int x, int y, int w, double &sx, double &sy, double &sz) // south pole
{
    double xl = (max(min(x, w - 1), 0) + 0.5) / w * 2.0 - 1.0;
    double yl = (max(min(y, w - 1), 0) + 0.5) / w * 2.0 - 1.0;
    double l =  sqrt(xl * xl + yl * yl + 1.0);
    sx = xl / l;
    sy = -yl / l;
    sz = -1.0 / l;
}

class PlaneHeightFunction : public HeightMipmap::HeightFunction
{
public:
    InputMap *src;

    int dstSize;

    PlaneHeightFunction(InputMap *src, int dstSize) :
        src(src), dstSize(dstSize)
    {
    }

    virtual float getHeight(int x, int y)
    {
        return getHeight(double(x) / dstSize * src->width, double(y) / dstSize * src->height);
    }

	float getHeight(double x, double y)
	{
		int ix = (int) floor(x);
		int iy = (int) floor(y);
		x -= ix;
		y -= iy;
		double cx = 1.0 - x;
		double cy = 1.0 - y;
		float h1 = src->get(ix, iy).x;
		float h2 = src->get(ix + 1, iy).x;
		float h3 = src->get(ix, iy + 1).x;
		float h4 = src->get(ix + 1, iy + 1).x;
		return (h1 * cx + h2 * x) * cy + (h3 * cx + h4 * x) * y;
	}
};

class PlaneColorFunction : public ColorMipmap::ColorFunction
{
public:
    InputMap *src;

    int dstSize;

    PlaneColorFunction(InputMap *src, int dstSize) :
        src(src), dstSize(dstSize)
    {
    }

    virtual vec4f getColor(int x, int y)
    {
        return getColor(double(x) / dstSize * src->width, double(y) / dstSize * src->height);
    }

	vec4f getColor(double x, double y)
	{
		int ix = (int) floor(x);
		int iy = (int) floor(y);
		x -= ix;
		y -= iy;
		double cx = 1.0 - x;
		double cy = 1.0 - y;
		vec4f c1 = src->get(ix, iy);
		vec4f c2 = src->get(ix + 1, iy);
		vec4f c3 = src->get(ix, iy + 1);
		vec4f c4 = src->get(ix + 1, iy + 1);
		vec4f c;
		c.x = (c1.x * cx + c2.x * x) * cy + (c3.x * cx + c4.x * x) * y;
		c.y = (c1.y * cx + c2.y * x) * cy + (c3.y * cx + c4.y * x) * y;
		c.z = (c1.z * cx + c2.z * x) * cy + (c3.z * cx + c4.z * x) * y;
		c.w = (c1.w * cx + c2.w * x) * cy + (c3.w * cx + c4.w * x) * y;
		return c;
	}
};

class SphericalHeightFunction : public HeightMipmap::HeightFunction
{
public:
    InputMap *src;

    projectionFunction projection;

    int dstSize;

    SphericalHeightFunction(InputMap *src, projectionFunction projection, int dstSize) :
        src(src), projection(projection), dstSize(dstSize)
    {
    }

    virtual float getHeight(int x, int y)
    {
        double sx, sy, sz;
        projection(x, y, dstSize, sx, sy, sz);
        double lon = atan2(sy, sx) + M_PI;
        double lat = acos(sz);
        return getHeight(lon, lat);
    }

	float getHeight(double lon, double lat)
	{
		lon = lon / M_PI * (src->width / 2);
		lat = lat / M_PI * src->height;
		int ilon = (int) floor(lon);
		int ilat = (int) floor(lat);
		lon -= ilon;
		lat -= ilat;
		double clon = 1.0 - lon;
		double clat = 1.0 - lat;
		float h1 = src->get((ilon + src->width) % src->width, ilat).x;
		float h2 = src->get((ilon + src->width + 1) % src->width, ilat).x;
		float h3 = src->get((ilon + src->width) % src->width, ilat + 1).x;
		float h4 = src->get((ilon + src->width + 1) % src->width, ilat + 1).x;
		return (h1 * clon + h2 * lon) * clat + (h3 * clon + h4 * lon) * lat;
	}
};

class SphericalColorFunction : public ColorMipmap::ColorFunction
{
public:
    InputMap *src;

    projectionFunction projection;

    int dstSize;

    SphericalColorFunction(InputMap *src, projectionFunction projection, int dstSize) :
        src(src), projection(projection), dstSize(dstSize)
    {
    }

    virtual vec4f getColor(int x, int y)
    {
        double sx, sy, sz;
        projection(x, y, dstSize, sx, sy, sz);
        double lon = atan2(sy, sx) + M_PI;
        double lat = acos(sz);
        return getColor(lon, lat);
    }

	vec4f getColor(double lon, double lat)
	{
		lon = lon / M_PI * (src->width / 2);
		lat = lat / M_PI * src->height;
		int ilon = (int) floor(lon);
		int ilat = (int) floor(lat);
		lon -= ilon;
		lat -= ilat;
		double clon = 1.0 - lon;
		double clat = 1.0 - lat;
		vec4f c1 = src->get((ilon + src->width) % src->width, ilat);
		vec4f c2 = src->get((ilon + src->width + 1) % src->width, ilat);
		vec4f c3 = src->get((ilon + src->width) % src->width, ilat + 1);
		vec4f c4 = src->get((ilon + src->width + 1) % src->width, ilat + 1);
		vec4f c;
		c.x = (c1.x * clon + c2.x * lon) * clat + (c3.x * clon + c4.x * lon) * lat;
		c.y = (c1.y * clon + c2.y * lon) * clat + (c3.y * clon + c4.y * lon) * lat;
		c.z = (c1.z * clon + c2.z * lon) * clat + (c3.z * clon + c4.z * lon) * lat;
		c.w = (c1.w * clon + c2.w * lon) * clat + (c3.w * clon + c4.w * lon) * lat;
		return c;
	}
};

void createDir(const string &dir)
{
    unsigned int index = dir.find_last_of('/');
    if (index != string::npos) {
        FILE *f;
        fopen(&f, dir.substr(0, index).c_str(), "r");
        if (f != NULL) {
            fclose(f);
        } else {
            createDir(dir.substr(0, index));
        }
    }
    int status = mkdir(dir.c_str());
    if (status != 0 && errno != EEXIST) {
        fprintf(stderr, "Cannot create directory %s\n", dir.c_str());
        throw exception();
    }
}

void preprocessDem(InputMap *src, int dstMinTileSize, int dstTileSize, int dstMaxLevel,
        const string &dstFolder, const string &tmpFolder, float residualScale)
{
    assert(dstTileSize % dstMinTileSize == 0);
    if (fexists(dstFolder + "/DEM.dat")) {
        return;
    }
    createDir(tmpFolder);
    createDir(dstFolder);
    int dstSize = dstTileSize << dstMaxLevel;
    HeightMipmap::HeightFunction *hf = new PlaneHeightFunction(src, dstSize);
    HeightMipmap *hm = new HeightMipmap(hf, dstMinTileSize, dstSize, dstTileSize, residualScale, tmpFolder);
    hm->compute1();
    while (true) {
        if (!hm->compute2()) {
            break;
        }
    }
    hm->generate(0, 0, 0, residualScale, dstFolder + "/DEM.dat");
}

void preprocessSphericalDem(InputMap *src, int dstMinTileSize, int dstTileSize, int dstMaxLevel,
        const string &dstFolder, const string &tmpFolder, float residualScale)
{
    assert(dstTileSize % dstMinTileSize == 0);
    if (fexists(dstFolder + "/DEM1.dat") && fexists(dstFolder + "/DEM2.dat") && fexists(dstFolder + "/DEM3.dat") &&
        fexists(dstFolder + "/DEM4.dat") && fexists(dstFolder + "/DEM5.dat") && fexists(dstFolder + "/DEM6.dat"))
    {
        return;
    }
    createDir(tmpFolder + "1");
    createDir(tmpFolder + "2");
    createDir(tmpFolder + "3");
    createDir(tmpFolder + "4");
    createDir(tmpFolder + "5");
    createDir(tmpFolder + "6");
    createDir(dstFolder);
    int dstSize = dstTileSize << dstMaxLevel;
    HeightMipmap::HeightFunction *hf1 = new SphericalHeightFunction(src, projection1, dstSize);
    HeightMipmap::HeightFunction *hf2 = new SphericalHeightFunction(src, projection2, dstSize);
    HeightMipmap::HeightFunction *hf3 = new SphericalHeightFunction(src, projection3, dstSize);
    HeightMipmap::HeightFunction *hf4 = new SphericalHeightFunction(src, projection4, dstSize);
    HeightMipmap::HeightFunction *hf5 = new SphericalHeightFunction(src, projection5, dstSize);
    HeightMipmap::HeightFunction *hf6 = new SphericalHeightFunction(src, projection6, dstSize);
    HeightMipmap *hm1 = new HeightMipmap(hf1, dstMinTileSize, dstSize, dstTileSize, residualScale, tmpFolder + "1");
    HeightMipmap *hm2 = new HeightMipmap(hf2, dstMinTileSize, dstSize, dstTileSize, residualScale, tmpFolder + "2");
    HeightMipmap *hm3 = new HeightMipmap(hf3, dstMinTileSize, dstSize, dstTileSize, residualScale, tmpFolder + "3");
    HeightMipmap *hm4 = new HeightMipmap(hf4, dstMinTileSize, dstSize, dstTileSize, residualScale, tmpFolder + "4");
    HeightMipmap *hm5 = new HeightMipmap(hf5, dstMinTileSize, dstSize, dstTileSize, residualScale, tmpFolder + "5");
    HeightMipmap *hm6 = new HeightMipmap(hf6, dstMinTileSize, dstSize, dstTileSize, residualScale, tmpFolder + "6");
    HeightMipmap::setCube(hm1, hm2, hm3, hm4, hm5, hm6);
    hm1->compute1();
    hm2->compute1();
    hm3->compute1();
    hm4->compute1();
    hm5->compute1();
    hm6->compute1();
    while (true) {
        hm1->compute2();
        hm2->compute2();
        hm3->compute2();
        hm4->compute2();
        hm5->compute2();
        if (!hm6->compute2()) {
            break;
        }
    }
    hm1->generate(0, 0, 0, residualScale, dstFolder + "/DEM1.dat");
    hm2->generate(0, 0, 0, residualScale, dstFolder + "/DEM2.dat");
    hm3->generate(0, 0, 0, residualScale, dstFolder + "/DEM3.dat");
    hm4->generate(0, 0, 0, residualScale, dstFolder + "/DEM4.dat");
    hm5->generate(0, 0, 0, residualScale, dstFolder + "/DEM5.dat");
    hm6->generate(0, 0, 0, residualScale, dstFolder + "/DEM6.dat");
}

void preprocessSphericalAperture(const string &srcFolder, int minLevel, int maxLevel, int samples,
        const string &dstFolder, const string &tmpFolder)
{
    if (fexists(dstFolder + "/APERTURE1.dat") && fexists(dstFolder + "/APERTURE2.dat") && fexists(dstFolder + "/APERTURE3.dat") &&
        fexists(dstFolder + "/APERTURE4.dat") && fexists(dstFolder + "/APERTURE5.dat") && fexists(dstFolder + "/APERTURE6.dat"))
    {
        return;
    }
    createDir(tmpFolder + "1");
    createDir(tmpFolder + "2");
    createDir(tmpFolder + "3");
    createDir(tmpFolder + "4");
    createDir(tmpFolder + "5");
    createDir(tmpFolder + "6");
    createDir(dstFolder);
    DemTileCache *r1 = new DemTileCache(srcFolder + "/DEM1.dat", 60);
    DemTileCache *r2 = new DemTileCache(srcFolder + "/DEM2.dat", 60);
    DemTileCache *r3 = new DemTileCache(srcFolder + "/DEM3.dat", 60);
    DemTileCache *r4 = new DemTileCache(srcFolder + "/DEM4.dat", 60);
    DemTileCache *r5 = new DemTileCache(srcFolder + "/DEM5.dat", 60);
    DemTileCache *r6 = new DemTileCache(srcFolder + "/DEM6.dat", 60);
    ElevationTileCache *z1 = new ElevationTileCache(r1, 40);
    ElevationTileCache *z2 = new ElevationTileCache(r2, 40);
    ElevationTileCache *z3 = new ElevationTileCache(r3, 40);
    ElevationTileCache *z4 = new ElevationTileCache(r4, 40);
    ElevationTileCache *z5 = new ElevationTileCache(r5, 40);
    ElevationTileCache *z6 = new ElevationTileCache(r6, 40);
    ElevationTileCache** faces = new ElevationTileCache*[6];
    faces[0] = z1;
    faces[1] = z2;
    faces[2] = z3;
    faces[3] = z4;
    faces[4] = z5;
    faces[5] = z6;
    PlanetElevationTileCache *pz = new PlanetElevationTileCache(faces, maxLevel);
    ApertureMipmap *a1 = new ApertureMipmap(pz, projection1f, 1.0, maxLevel, minLevel, samples);
    a1->build(tmpFolder + "1");
    a1->generate(tmpFolder + "1", dstFolder + "/APERTURE1.dat");
    ApertureMipmap *a2 = new ApertureMipmap(pz, projection2f, 1.0, maxLevel, minLevel, samples);
    a2->build(tmpFolder + "2");
    a2->generate(tmpFolder + "2", dstFolder + "/APERTURE2.dat");
    ApertureMipmap *a3 = new ApertureMipmap(pz, projection3f, 1.0, maxLevel, minLevel, samples);
    a3->build(tmpFolder + "3");
    a3->generate(tmpFolder + "3", dstFolder + "/APERTURE3.dat");
    ApertureMipmap *a4 = new ApertureMipmap(pz, projection4f, 1.0, maxLevel, minLevel, samples);
    a4->build(tmpFolder + "4");
    a4->generate(tmpFolder + "4", dstFolder + "/APERTURE4.dat");
    ApertureMipmap *a5 = new ApertureMipmap(pz, projection5f, 1.0, maxLevel, minLevel, samples);
    a5->build(tmpFolder + "5");
    a5->generate(tmpFolder + "5", dstFolder + "/APERTURE5.dat");
    ApertureMipmap *a6 = new ApertureMipmap(pz, projection6f, 1.0, maxLevel, minLevel, samples);
    a6->build(tmpFolder + "6");
    a6->generate(tmpFolder + "6", dstFolder + "/APERTURE6.dat");
}

void preprocessOrtho(InputMap *src, int dstTileSize, int dstChannels, int dstMaxLevel,
        const string &dstFolder, const string &tmpFolder, float (*rgbToLinear)(float), float (*linearToRgb)(float))
{
    if (fexists(dstFolder + "/RGB.dat") && fexists(dstFolder + "/dxt/RGB.dat") && fexists(dstFolder + "/residuals/RGB.dat")) {
        return;
    }
    createDir(tmpFolder);
    createDir(dstFolder);
    createDir(dstFolder + "/dxt");
    createDir(dstFolder + "/residuals");
    int dstSize = dstTileSize << dstMaxLevel;
    ColorMipmap::ColorFunction *cf = new PlaneColorFunction(src, dstSize);
    ColorMipmap *cm = new ColorMipmap(cf, dstSize, dstTileSize, 2, dstChannels,
        rgbToLinear == NULL ? id : rgbToLinear, linearToRgb == NULL ? id : linearToRgb, tmpFolder);
    cm->compute();
    cm->generate(0, 0, 0, false, true, RGB_JPEG_QUALITY, dstFolder + "/RGB.dat");
    cm->generate(0, 0, 0, true, true, RGB_JPEG_QUALITY, dstFolder + "/dxt/RGB.dat");
    cm->generateResiduals(true, RGB_JPEG_QUALITY, dstFolder + "/RGB.dat", tmpFolder + "/residuals/RGB.dat");
    cm->reorderResiduals(tmpFolder + "/RGB.dat", dstFolder + "/residuals/RGB.dat");
}

void preprocessSphericalOrtho(InputMap *src, int dstTileSize, int dstChannels, int dstMaxLevel,
        const string &dstFolder, const string &tmpFolder, float (*rgbToLinear)(float), float (*linearToRgb)(float))
{
    if (fexists(dstFolder + "/RGB1.dat") && fexists(dstFolder + "/dxt/RGB1.dat") && fexists(dstFolder + "/residuals/RGB1.dat") &&
        fexists(dstFolder + "/RGB2.dat") && fexists(dstFolder + "/dxt/RGB2.dat") && fexists(dstFolder + "/residuals/RGB2.dat") &&
        fexists(dstFolder + "/RGB3.dat") && fexists(dstFolder + "/dxt/RGB3.dat") && fexists(dstFolder + "/residuals/RGB3.dat") &&
        fexists(dstFolder + "/RGB4.dat") && fexists(dstFolder + "/dxt/RGB4.dat") && fexists(dstFolder + "/residuals/RGB4.dat") &&
        fexists(dstFolder + "/RGB5.dat") && fexists(dstFolder + "/dxt/RGB5.dat") && fexists(dstFolder + "/residuals/RGB5.dat") &&
        fexists(dstFolder + "/RGB6.dat") && fexists(dstFolder + "/dxt/RGB6.dat") && fexists(dstFolder + "/residuals/RGB6.dat"))
    {
        return;
    }
    createDir(tmpFolder + "1");
    createDir(tmpFolder + "2");
    createDir(tmpFolder + "3");
    createDir(tmpFolder + "4");
    createDir(tmpFolder + "5");
    createDir(tmpFolder + "6");
    createDir(dstFolder);
    createDir(dstFolder + "/dxt");
    createDir(dstFolder + "/residuals");
    int dstSize = dstTileSize << dstMaxLevel;
    ColorMipmap::ColorFunction *cf1 = new SphericalColorFunction(src, projection1h, dstSize);
    ColorMipmap::ColorFunction *cf2 = new SphericalColorFunction(src, projection2h, dstSize);
    ColorMipmap::ColorFunction *cf3 = new SphericalColorFunction(src, projection3h, dstSize);
    ColorMipmap::ColorFunction *cf4 = new SphericalColorFunction(src, projection4h, dstSize);
    ColorMipmap::ColorFunction *cf5 = new SphericalColorFunction(src, projection5h, dstSize);
    ColorMipmap::ColorFunction *cf6 = new SphericalColorFunction(src, projection6h, dstSize);
    ColorMipmap *cm1 = new ColorMipmap(cf1, dstSize, dstTileSize, 2, dstChannels,
        rgbToLinear == NULL ? id : rgbToLinear, linearToRgb == NULL ? id : linearToRgb, tmpFolder + "1");
    ColorMipmap *cm2 = new ColorMipmap(cf2, dstSize, dstTileSize, 2, dstChannels,
        rgbToLinear == NULL ? id : rgbToLinear, linearToRgb == NULL ? id : linearToRgb, tmpFolder + "2");
    ColorMipmap *cm3 = new ColorMipmap(cf3, dstSize, dstTileSize, 2, dstChannels,
        rgbToLinear == NULL ? id : rgbToLinear, linearToRgb == NULL ? id : linearToRgb, tmpFolder + "3");
    ColorMipmap *cm4 = new ColorMipmap(cf4, dstSize, dstTileSize, 2, dstChannels,
        rgbToLinear == NULL ? id : rgbToLinear, linearToRgb == NULL ? id : linearToRgb, tmpFolder + "4");
    ColorMipmap *cm5 = new ColorMipmap(cf5, dstSize, dstTileSize, 2, dstChannels,
        rgbToLinear == NULL ? id : rgbToLinear, linearToRgb == NULL ? id : linearToRgb, tmpFolder + "5");
    ColorMipmap *cm6 = new ColorMipmap(cf6, dstSize, dstTileSize, 2, dstChannels,
        rgbToLinear == NULL ? id : rgbToLinear, linearToRgb == NULL ? id : linearToRgb, tmpFolder + "6");
    ColorMipmap::setCube(cm1, cm2, cm3, cm4, cm5, cm6);
    cm1->compute();
    cm2->compute();
    cm3->compute();
    cm4->compute();
    cm5->compute();
    cm6->compute();
    cm1->generate(0, 0, 0, false, true, RGB_JPEG_QUALITY, dstFolder + "/RGB1.dat");
    cm2->generate(0, 0, 0, false, true, RGB_JPEG_QUALITY, dstFolder + "/RGB2.dat");
    cm3->generate(0, 0, 0, false, true, RGB_JPEG_QUALITY, dstFolder + "/RGB3.dat");
    cm4->generate(0, 0, 0, false, true, RGB_JPEG_QUALITY, dstFolder + "/RGB4.dat");
    cm5->generate(0, 0, 0, false, true, RGB_JPEG_QUALITY, dstFolder + "/RGB5.dat");
    cm6->generate(0, 0, 0, false, true, RGB_JPEG_QUALITY, dstFolder + "/RGB6.dat");
    cm1->generate(0, 0, 0, true, true, RGB_JPEG_QUALITY, dstFolder + "/dxt/RGB1.dat");
    cm2->generate(0, 0, 0, true, true, RGB_JPEG_QUALITY, dstFolder + "/dxt/RGB2.dat");
    cm3->generate(0, 0, 0, true, true, RGB_JPEG_QUALITY, dstFolder + "/dxt/RGB3.dat");
    cm4->generate(0, 0, 0, true, true, RGB_JPEG_QUALITY, dstFolder + "/dxt/RGB4.dat");
    cm5->generate(0, 0, 0, true, true, RGB_JPEG_QUALITY, dstFolder + "/dxt/RGB5.dat");
    cm6->generate(0, 0, 0, true, true, RGB_JPEG_QUALITY, dstFolder + "/dxt/RGB6.dat");
    cm1->generateResiduals(true, RGB_JPEG_QUALITY, dstFolder + "/RGB1.dat", tmpFolder + "1/RGB.dat");
    cm1->reorderResiduals(tmpFolder + "1/RGB.dat", dstFolder + "/residuals/RGB1.dat");
    cm2->generateResiduals(true, RGB_JPEG_QUALITY, dstFolder + "/RGB2.dat", tmpFolder + "2/RGB.dat");
    cm2->reorderResiduals(tmpFolder + "2/RGB.dat", dstFolder + "/residuals/RGB2.dat");
    cm3->generateResiduals(true, RGB_JPEG_QUALITY, dstFolder + "/RGB3.dat", tmpFolder + "3/RGB.dat");
    cm3->reorderResiduals(tmpFolder + "3/RGB.dat", dstFolder + "/residuals/RGB3.dat");
    cm4->generateResiduals(true, RGB_JPEG_QUALITY, dstFolder + "/RGB4.dat", tmpFolder + "4/RGB.dat");
    cm4->reorderResiduals(tmpFolder + "4/RGB.dat", dstFolder + "/residuals/RGB4.dat");
    cm5->generateResiduals(true, RGB_JPEG_QUALITY, dstFolder + "/RGB5.dat", tmpFolder + "5/RGB.dat");
    cm5->reorderResiduals(tmpFolder + "5/RGB.dat", dstFolder + "/residuals/RGB5.dat");
    cm6->generateResiduals(true, RGB_JPEG_QUALITY, dstFolder + "/RGB6.dat", tmpFolder + "6/RGB.dat");
    cm6->reorderResiduals(tmpFolder + "6/RGB.dat", dstFolder + "/residuals/RGB6.dat");
}

}
