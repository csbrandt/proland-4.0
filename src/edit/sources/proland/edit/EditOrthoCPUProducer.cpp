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

#include "proland/edit/EditOrthoCPUProducer.h"

#include "ork/core/Logger.h"
#include "ork/resource/ResourceTemplate.h"
#include "proland/producer/CPUTileStorage.h"

using namespace std;
using namespace ork;

namespace proland
{

int EMPTY_DELTA[4] = { 0, 0, 0, 0 };

EditOrthoCPUProducer::EditOrthoCPUProducer(ptr<TileCache> cache, const char *name) :
    OrthoCPUProducer()
{
    init(cache, name);
}

EditOrthoCPUProducer::EditOrthoCPUProducer() : OrthoCPUProducer()
{
}

void EditOrthoCPUProducer::init(ptr<TileCache> cache, const char *name)
{
    OrthoCPUProducer::init(cache, name);
    assert(getBorder() == 2);
    empty = name == NULL || strlen(name) == 0;
    tWidth = cache->getStorage()->getTileSize();
    tSize = tWidth - 2 * getBorder();
    tChannels = dynamic_cast<CPUTileStorage<unsigned char>*>(cache->getStorage().get())->getChannels();
    curId = make_pair(-1, make_pair(0, 0));
    curDeltaColor = NULL;
}

EditOrthoCPUProducer::~EditOrthoCPUProducer()
{
    map<TileCache::Tile::Id, unsigned char*>::iterator i = modifiedTiles.begin();
    while (i != modifiedTiles.end()) {
        delete[] i->second;
        i++;
    }
}

bool EditOrthoCPUProducer::hasTile(int level, int tx, int ty)
{
    if (OrthoCPUProducer::hasTile(level, tx, ty)) {
        return true;
    }
    TileCache::Tile::Id id = TileCache::Tile::getId(level, tx, ty);
    return modifiedTiles.find(id) != modifiedTiles.end();
}

void EditOrthoCPUProducer::editedTile(int level, int tx, int ty, int *deltaColor)
{
    bool empty = true;
    for (int p = 0; p < tSize * tSize * tChannels; ++p) {
        if (deltaColor[p] != 0) {
            empty = false;
            break;
        }
    }
    if (empty) {
        delete[] deltaColor;
        return;
    }

    TileCache::Tile::Id id = TileCache::Tile::getId(level, tx, ty);
    deltaColors.insert(make_pair(id, deltaColor));

    // computes the color deltas for all the ancestors of the tile
    while (level > 0) {
        // finds the color deltas for the parent tile
        // creates and initializes it if necessary
        int *pDeltaColor = NULL;
        TileCache::Tile::Id pid = TileCache::Tile::getId(level - 1, tx / 2, ty / 2);
        map<TileCache::Tile::Id, int*>::iterator i = deltaColors.find(pid);
        if (i == deltaColors.end()) {
            pDeltaColor = new int[tSize * tSize * tChannels];
            for (int p = 0; p < tSize * tSize * tChannels; ++p) {
                pDeltaColor[p] = 0;
            }
            deltaColors.insert(make_pair(pid, pDeltaColor));
        } else {
            pDeltaColor = i->second;
        }

        // computes the color deltas in the parent tile
        bool changes = false;
        int rx = (tx % 2) * tSize / 2;
        int ry = (ty % 2) * tSize / 2;
        for (int y = 0; y < tSize; y += 2) {
            for (int x = 0; x < tSize; x += 2) {
                int dstOff = ((x / 2 + rx) + (y / 2 + ry) * tSize) * tChannels;
                for (int c = 0; c < tChannels; ++c) {
                    int delta;
                    delta = deltaColor[(x + y * tSize) * tChannels + c];
                    delta += deltaColor[(x + 1 + y * tSize) * tChannels + c];
                    delta += deltaColor[(x + (y + 1) * tSize) * tChannels + c];
                    delta += deltaColor[(x + 1 + (y + 1) * tSize) * tChannels + c];
                    delta = delta / 4;
                    pDeltaColor[dstOff + c] = delta;
                    changes |= delta != 0;
                }
            }
        }
        if (!changes) {
            break;
        }

        level = level - 1;
        tx = tx / 2;
        ty = ty / 2;
        deltaColor = pDeltaColor;
    }
}

void EditOrthoCPUProducer::updateTiles()
{
    // computes the set of modified residual tiles
    // (for each edited residual tile, the tile itself and its 8 neighbors are modified)
    set<TileCache::Tile::Id> changedTiles;
    map<TileCache::Tile::Id, int*>::iterator i = deltaColors.begin();
    while (i != deltaColors.end()) {
        TileCache::Tile::Id id = i->first;
        int level = id.first;
        int tx0 = id.second.first;
        int ty0 = id.second.second;
        int n = 1 << level;
        for (int y = -1; y <= 1; ++y) {
            for (int x = -1; x <= 1; ++x) {
                int tx = tx0 + x;
                int ty = ty0 + y;
                if (tx >= 0 && tx < n && ty >= 0 && ty < n) {
                    changedTiles.insert(make_pair(level, make_pair(tx, ty)));
                }
            }
        }
        i++;
    }

    int *tmp = new int[tWidth * (tWidth / 2 + 2) * tChannels];

    // updates the modified residual tiles
    set<TileCache::Tile::Id>::iterator j = changedTiles.begin();
    while (j != changedTiles.end()) {
        TileCache::Tile::Id id = *j;
        int level = id.first;
        int tx = id.second.first;
        int ty = id.second.second;
        TileCache::Tile::Id key = TileCache::Tile::getId(level, tx, ty);

        // finds the modified residual tile, creates it if necessary
        unsigned char *modifiedTile = NULL;
        map<TileCache::Tile::Id, unsigned char*>::iterator k = modifiedTiles.find(key);
        if (k == modifiedTiles.end()) {
            if (hasTile(level, tx, ty)) {
                CPUTileStorage<unsigned char>::CPUSlot *slot = new CPUTileStorage<unsigned char>::CPUSlot(getCache()->getStorage().get(), tWidth * tWidth * tChannels);
                doCreateTile(level, tx, ty, slot);
                modifiedTiles.insert(make_pair(key, slot->data));
                modifiedTile = slot->data;
                slot->data = NULL;
                delete slot;
            } else {
                modifiedTile = new unsigned char[tWidth * tWidth * tChannels];
                for (int i = 0; i < tWidth * tWidth * tChannels; ++i) {
                    modifiedTile[i] = 128;
                }
                modifiedTiles.insert(make_pair(key, modifiedTile));
            }
        } else {
            modifiedTile = k->second;
        }

        int n = 1 << level;
        int w = tSize;
        int b = getBorder();

        int offset = (b * tWidth + b) * tChannels;
        unsigned char *modifiedTile0 = modifiedTile + offset;

        // updates the modified residual tile content
        if (level == 0) {
            for (int y = -b; y < w + b; ++y) {
                for (int x = -b; x < w + b; ++x) {
                    int *delta = getDeltaColor(level, n, tx, ty, x, y);
                    unsigned char *dst = modifiedTile0 + (x + y * tWidth) * tChannels;
                    for (int c = 0; c < tChannels; ++c) {
                        dst[c] = clamp(int(dst[c]) + delta[c], 0, 255);
                    }
                }
            }
        } else {
            int rx = (tx % 2) * w / 2;
            int ry = (ty % 2) * w / 2;
            /*// unoptimized version
            int masks[4][4] = {
                { 1, 3, 3, 9 },
                { 3, 1, 9, 3 },
                { 3, 9, 1, 3 },
                { 9, 3, 3, 1 }
            };
            for (int y = -b; y < w + b; ++y) {
                int py = (y + 3) / 2 + ry - 2;
                for (int x = -b; x < w + b; ++x) {
                    int px = (x + 3) / 2 + rx - 2;
                    int *m = masks[((x + 2) % 2) + 2 * ((y + 2) % 2)];
                    int *p0 = getDeltaColor(level - 1, n / 2, tx / 2, ty / 2, px, py);
                    int *p1 = getDeltaColor(level - 1, n / 2, tx / 2, ty / 2, px + 1, py);
                    int *p2 = getDeltaColor(level - 1, n / 2, tx / 2, ty / 2, px, py + 1);
                    int *p3 = getDeltaColor(level - 1, n / 2, tx / 2, ty / 2, px + 1, py + 1);
                    int *delta = getDeltaColor(level, n, tx, ty, x, y);
                    unsigned char *dst = modifiedTile0 + (x + y * tWidth) * tChannels;
                    for (int c = 0; c < tChannels; ++c) {
                        int r = delta[c] - (m[0] * p0[c] + m[1] * p1[c] + m[2] * p2[c] + m[3] * p3[c]) / 16;
                        dst[c] = clamp(dst[c] + r / 2, 0, 255);
                    }
                }
            }*/
            // optimized version
            for (int y = -2; y < w / 2 + 2; ++y) {
                int px = rx - 2;
                int py = ry + y;
                int *p0 = getDeltaColor(level - 1, n / 2, tx / 2, ty / 2, px++, py);
                int *p1 = getDeltaColor(level - 1, n / 2, tx / 2, ty / 2, px++, py);
                int off = ((y + 2) * tWidth) * tChannels;
                for (int c = 0; c < tChannels; ++c) {
                    tmp[off + c] = p0[c] + 3 * p1[c];
                }
                for (int x = -1; x < w + 1; x += 2) {
                    p0 = p1;
                    p1 = getDeltaColor(level - 1, n / 2, tx / 2, ty / 2, px++, py);
                    off = (x + 2 + (y + 2) * tWidth) * tChannels;
                    for (int c = 0; c < tChannels; ++c) {
                        tmp[off + c] = 3 * p0[c] + p1[c];
                        tmp[off + tChannels + c] = p0[c] + 3 * p1[c];
                    }
                }
                p0 = p1;
                p1 = getDeltaColor(level - 1, n / 2, tx / 2, ty / 2, px, py);
                off = (w + 3 + (y + 2) * tWidth) * tChannels;
                for (int c = 0; c < tChannels; ++c) {
                    tmp[off + c] = 3 * p0[c] + p1[c];
                }
            }
            for (int y = -2; y < w + 2; ++y) {
                int py = (y + 3) / 2;
                int m0 = y % 2 == 0 ? 1 : 3;
                int m1 = 4 - m0;
                for (int x = -2; x < w + 2; ++x) {
                    int *p0 = tmp + (x + 2 + py * tWidth) * tChannels;
                    int *p1 = tmp + (x + 2 + (py + 1) * tWidth) * tChannels;
                    int *delta = getDeltaColor(level, n, tx, ty, x, y);
                    unsigned char *dst = modifiedTile0 + (x + y * tWidth) * tChannels;
                    for (int c = 0; c < tChannels; ++c) {
                        int r = delta[c] - (m0 * p0[c] + m1 * p1[c]) / 16;
                        dst[c] = clamp(dst[c] + r / 2, 0, 255);
                    }
                }
            }
        }

        j++;
    }

    delete[] tmp;

    i = deltaColors.begin();
    while (i != deltaColors.end()) {
        delete[] i->second;
        i++;
    }
    deltaColors.clear();
    curId = make_pair(-1, make_pair(0, 0));
}

void EditOrthoCPUProducer::reset()
{
    map<TileCache::Tile::Id, unsigned char*>::iterator i = modifiedTiles.begin();
    while (i != modifiedTiles.end()) {
        delete[] i->second;
        i++;
    }
    modifiedTiles.clear();
    invalidateTiles();
}

int *EditOrthoCPUProducer::getDeltaColor(int level, int n, int tx, int ty, int x, int y)
{
    x += tx * tSize;
    y += ty * tSize;
    int nw = n * tSize;
    x = clamp(x, 0, nw - 1);
    y = clamp(y, 0, nw - 1);
    tx = x / tSize;
    ty = y / tSize;
    x = x % tSize;
    y = y % tSize;

    int *deltaColor = NULL;
    TileCache::Tile::Id id = TileCache::Tile::getId(level, tx, ty);
    if (id == curId) {
        deltaColor = curDeltaColor;
    } else {
        map<TileCache::Tile::Id, int*>::iterator i = deltaColors.find(id);
        if (i != deltaColors.end()) {
            deltaColor = i->second;
        }
        curId = id;
        curDeltaColor = deltaColor;
    }

    return deltaColor == NULL ? EMPTY_DELTA : deltaColor + (x + y * tSize) * tChannels;
}

bool EditOrthoCPUProducer::doCreateTile(int level, int tx, int ty, TileStorage::Slot *data)
{
    TileCache::Tile::Id id = TileCache::Tile::getId(level, tx, ty);
    map<TileCache::Tile::Id, unsigned char*>::iterator it = modifiedTiles.find(id);
    if (it != modifiedTiles.end()) {
        unsigned char *src = it->second;
        unsigned char *dst = dynamic_cast<CPUTileStorage<unsigned char>::CPUSlot*>(data)->data;
        for (int i = 0; i < tWidth * tWidth * tChannels; ++i) {
            dst[i] = src[i];
        }
        return true;
    } else if (empty) {
        CPUTileStorage<unsigned char>::CPUSlot *cpuData = dynamic_cast<CPUTileStorage<unsigned char>::CPUSlot*>(data);
        assert(cpuData != NULL);
        int value = level == 0 ? 0 : 128;
        for (int i = 0; i < cpuData->size; i++) {
            cpuData->data[i] = value;
        }
        return true;
    } else {
        return OrthoCPUProducer::doCreateTile(level, tx, ty, data);
    }
}

void EditOrthoCPUProducer::swap(ptr<EditOrthoCPUProducer> p)
{
    OrthoCPUProducer::swap(p);
    std::swap(tWidth, p->tWidth);
    std::swap(tSize, p->tSize);
    std::swap(tChannels, p->tChannels);
    std::swap(curId, p->curId);
    std::swap(curDeltaColor, p->curDeltaColor);
    std::swap(modifiedTiles, p->modifiedTiles);
    std::swap(deltaColors, p->deltaColors);
}

class EditOrthoCPUProducerResource : public ResourceTemplate<2, EditOrthoCPUProducer>
{
public:
    EditOrthoCPUProducerResource(ptr<ResourceManager> manager, const string &name, ptr<ResourceDescriptor> desc, const TiXmlElement *e = NULL) :
        ResourceTemplate<2, EditOrthoCPUProducer>(manager, name, desc)
    {
        e = e == NULL ? desc->descriptor : e;
        ptr<TileCache> cache;
        string file;
        checkParameters(desc, e, "name,cache,file,");
        cache = manager->loadResource(getParameter(desc, e, "cache")).cast<TileCache>();
        if (e->Attribute("file") != NULL) {
            file = getParameter(desc, e, "file");
            file = manager->getLoader()->findResource(file);
        }
        init(cache, file.c_str());
    }
};

extern const char editOrthoCPUProducer[] = "editOrthoCpuProducer";

static ResourceFactory::Type<editOrthoCPUProducer, EditOrthoCPUProducerResource> EditOrthoCPUProducerType;

}
