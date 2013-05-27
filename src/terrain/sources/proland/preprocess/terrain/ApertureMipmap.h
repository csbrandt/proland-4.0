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

#ifndef _PROLAND_APERTURE_MIPMAP_
#define _PROLAND_APERTURE_MIPMAP_

#include <string>
#include <list>
#include <map>

#include "ork/math/vec3.h"
#include "proland/preprocess/terrain/Util.h"

using namespace std;
using namespace ork;

namespace proland
{

class FloatTileCache {
public:
    struct FloatTile
    {
        int level, tx, ty;

        float *data;

        FloatTile(int level, int tx, int ty, float *data = NULL) :
            level(level), tx(tx), ty(ty), data(data)
        {
        }

        ~FloatTile()
        {
            if (data != NULL) {
                delete[] data;
            }
        }

        int key()
        {
            return tx + ty * (1 << level) + ((1 << (2 * level)) - 1) / 3;
        }
    };

	FloatTileCache(int capacity = 20) :
        capacity(capacity)
	{
	}

	virtual ~FloatTileCache()
	{
        list<FloatTile*>::iterator i = tileCacheOrder.begin();
        while (i != tileCacheOrder.end()) {
            delete *i;
            ++i;
        }
    }


	float* getTile(int level, int tx, int ty);

protected:
	virtual float* readTile(int level, int tx, int ty) = 0;

private:
    int capacity;

	typedef map<int, list<FloatTile*>::iterator> Cache;

	Cache tileCache;

	list<FloatTile*> tileCacheOrder;
};

class DemTileCache : public FloatTileCache
{
public:
    FILE *tileFile;

    int tileSize;

    int rootLevel;

    int deltaLevel;

    int rootTx;

    int rootTy;

    int minLevel;

    int maxLevel;

    int analyzeLevel;

    float scale;

    unsigned int header;

    unsigned int* offsets;

    unsigned char *compressedData;

    unsigned char *uncompressedData;

	DemTileCache(const string &name, int capacity = 20);

	~DemTileCache();

    virtual float* readTile(int level, int tx, int ty);

	int getTileId(int level, int tx, int ty);

	int getTileSize(int level);
};

class ElevationTileCache : public FloatTileCache
{
public:
    DemTileCache *r;

	ElevationTileCache(DemTileCache *residuals, int capacity = 20);

	~ElevationTileCache();

    virtual float* readTile(int level, int tx, int ty);

    float getHeight(int level, int x, int y);

    float getHeight(int level, int x, int y, float dx, float dy);
};

class PlanetElevationTileCache
{
public:
    ElevationTileCache **faces;

    int level;

    int m;

    PlanetElevationTileCache(ElevationTileCache **faces, int level);

    float getHeight(double sx, double sy, double sz);
};

class ApertureMipmap
{
public:
    typedef void (*projFun)(double x, double y, double w, double &sx, double &sy, double &sz);

    ApertureMipmap(PlanetElevationTileCache *z, projFun proj, double R, int buildLevel, int minLevel, int samples);

    ~ApertureMipmap();

    void build(const string &temp);

    void generate(const string &cache, const string &file);

private:
	PlanetElevationTileCache *z;

	projFun proj;

    double R;

	int buildLevel;

	int minLevel;

    int samples;

    void compute(int level, int x, int y, float dx, float dy, float z0, float *len, int flen, float *slopes);

    float ambient(float *phi, float *cphi, float *sphi, float *slopes, vec3f &norm);
};

}

#endif
