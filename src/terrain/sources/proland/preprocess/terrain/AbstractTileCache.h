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

#ifndef _PROLAND_TILE_CACHES_
#define _PROLAND_TILE_CACHES_

#include <cstdio>
#include <map>
#include <list>
#include "ork/math/vec4.h"

using namespace std;
using namespace ork;

namespace proland
{

class AbstractTileCache {
public:
    struct Tile
    {
        int tx, ty;

        unsigned char *data;

        Tile(int tx, int ty, unsigned char *data = NULL) :
            tx(tx), ty(ty), data(data)
        {
        }

        ~Tile()
        {
            if (data != NULL) {
                delete[] data;
            }
        }

        int key(int width)
        {
            return tx + ty * width;
        }
    };

	AbstractTileCache(int width, int height, int tileSize, int channels, int capacity = 20) :
        width(width), height(height), tileSize(tileSize), channels(channels), capacity(capacity)
	{
	}

	virtual ~AbstractTileCache()
	{
	    reset(0, 0, 0);
	}

    int getWidth()
    {
        return width;
    }

    int getHeight()
    {
        return height;
    }

    int getTileSize()
    {
        return tileSize;
    }

    int getChannels()
    {
        return channels;
    }

	unsigned char* getTile(int tx, int ty);

	virtual float getTileHeight(int x, int y);

    virtual vec4f getTileColor(int x, int y);

	virtual void reset(int width, int height, int tileSize);

protected:
	virtual unsigned char* readTile(int tx, int ty) = 0;

private:
    int width;

    int height;

    int tileSize;

    int channels;

    int capacity;

	typedef map<int, list<Tile*>::iterator> Cache;

	Cache tileCache;

	list<Tile*> tileCacheOrder;
};

}

#endif
