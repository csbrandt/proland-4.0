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

#include "proland/preprocess/terrain/AbstractTileCache.h"

namespace proland
{

unsigned char* AbstractTileCache::getTile(int tx, int ty)
{
    int key = Tile(tx, ty).key(width / tileSize + 1);
    Cache::iterator i = tileCache.find(key);
    if (i == tileCache.end()) {
        unsigned char *data = readTile(tx, ty);

        list<Tile*>::iterator li;

        if ((int) tileCache.size() == capacity) {
            // evict least recently used tile if cache is full
            li = tileCacheOrder.begin();
            Tile *t = *li;
            tileCache.erase(t->key(width / tileSize + 1));
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

float AbstractTileCache::getTileHeight(int x, int y)
{
    x = max(min(x, width), 0);
    y = max(min(y, height), 0);
    int tx = min(x, width - 1) / tileSize;
    int ty = min(y, height - 1) / tileSize;
    x = (x == width ? tileSize : x % tileSize) + 2;
    y = (y == height ? tileSize : y % tileSize) + 2;
    unsigned char* data = getTile(tx, ty);
    if (channels == 1) {
        int off = (x + y * (tileSize + 5));
        return float(data[off]);
    } else {
        int off = (x + y * (tileSize + 5)) * 2;
        int hl = data[off++];
        int hh = data[off];
        short sh = short(hh) << 8 | short(hl);
        return float(sh);
    }
}

vec4f AbstractTileCache::getTileColor(int x, int y)
{
    x = max(min(x, width - 1), 0);
    y = max(min(y, height - 1), 0);
    int tx = x / tileSize;
    int ty = y / tileSize;
    x = x % tileSize + 2;
    y = y % tileSize + 2;
    unsigned char* data = getTile(tx, ty);
    int off = (x + y * (tileSize + 4)) * channels;
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

void AbstractTileCache::reset(int width, int height, int tileSize)
{
    list<Tile*>::iterator i = tileCacheOrder.begin();
    while (i != tileCacheOrder.end()) {
        delete *i;
        ++i;
    }
    tileCache.clear();
    tileCacheOrder.clear();
    this->width = width;
    this->height = height;
    this->tileSize = tileSize;
}

}
