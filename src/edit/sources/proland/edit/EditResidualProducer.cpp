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

#include "proland/edit/EditResidualProducer.h"

#include "ork/core/Logger.h"
#include "ork/resource/ResourceTemplate.h"
#include "proland/producer/CPUTileStorage.h"

using namespace std;
using namespace ork;

namespace proland
{

EditResidualProducer::EditResidualProducer(ptr<TileCache> cache, const char *name, int deltaLevel, float zscale) :
    ResidualProducer()
{
    ResidualProducer::init(cache, name, deltaLevel, zscale);
    init();
}

EditResidualProducer::EditResidualProducer() : ResidualProducer()
{
    map<TileCache::Tile::Id, float*>::iterator i = modifiedTiles.begin();
    while (i != modifiedTiles.end()) {
        delete[] i->second;
        i++;
    }
}

void EditResidualProducer::init()
{
    tWidth = getCache()->getStorage()->getTileSize();
    tSize = tWidth - 5;
    curId = make_pair(-1, make_pair(0, 0));
    curDeltaElevation = NULL;
}

EditResidualProducer::~EditResidualProducer()
{
}

void EditResidualProducer::editedTile(int level, int tx, int ty, float *deltaElevation)
{
    bool empty = true;
    for (int p = 0; p < (tSize + 1) * (tSize + 1); ++p) {
        if (deltaElevation[p] != 0.0f) {
            empty = false;
            break;
        }
    }
    if (empty) {
        delete[] deltaElevation;
        return;
    }

    TileCache::Tile::Id id = make_pair(level, make_pair(tx, ty));
    deltaElevations.insert(make_pair(id, deltaElevation));

    // computes the elevation deltas for all the ancestors of the tile
    while (level > 0) {
        // finds the elevation deltas for the parent tile
        // creates and initializes it if necessary
        float *pDeltaElevation = NULL;
        TileCache::Tile::Id pid = make_pair(level - 1, make_pair(tx / 2, ty / 2));
        map<TileCache::Tile::Id, float*>::iterator i = deltaElevations.find(pid);
        if (i == deltaElevations.end()) {
            pDeltaElevation = new float[(tSize + 1) * (tSize + 1)];
            for (int p = 0; p < (tSize + 1) * (tSize + 1); ++p) {
                pDeltaElevation[p] = 0.0f;
            }
            deltaElevations.insert(make_pair(pid, pDeltaElevation));
        } else {
            pDeltaElevation = i->second;
        }

        // computes the elevation deltas in the parent tile
        bool changes = false;
        int rx = (tx % 2) * tSize / 2;
        int ry = (ty % 2) * tSize / 2;
        for (int y = 0; y <= tSize; y += 2) {
            for (int x = 0; x <= tSize; x += 2) {
                int srcOff = x + y * (tSize + 1);
                int dstOff = (x / 2 + rx) + (y / 2 + ry) * (tSize + 1);
                float deltaz = (pDeltaElevation[dstOff] = deltaElevation[srcOff]);
                changes |= deltaz != 0.0f;
            }
        }
        if (!changes) {
            break;
        }

        level = level - 1;
        tx = tx / 2;
        ty = ty / 2;
        deltaElevation = pDeltaElevation;
    }
}

void EditResidualProducer::updateResiduals()
{
    // computes the set of modified residual tiles
    // (for each edited residual tile, the tile itself and its 8 neighbors are modified)
    set<TileCache::Tile::Id> modifiedResiduals;
    map<TileCache::Tile::Id, float*>::iterator i = deltaElevations.begin();
    while (i != deltaElevations.end()) {
        TileCache::Tile::Id id = i->first;
        int level = id.first;
        int tx0 = id.second.first;
        int ty0 = id.second.second;
        int l = level + getDeltaLevel();
        int n = l < getMinLevel() ? 1 : 1 << (l - getMinLevel());
        for (int y = -1; y <= 1; ++y) {
            for (int x = -1; x <= 1; ++x) {
                int tx = tx0 + x;
                int ty = ty0 + y;
                if (tx >= 0 && tx < n && ty >= 0 && ty < n) {
                    modifiedResiduals.insert(make_pair(level, make_pair(tx, ty)));
                }
            }
        }
        i++;
    }

    float *tmp = new float[tWidth * (tWidth / 2 + 3)];

    // updates the modified residual tiles
    set<TileCache::Tile::Id>::iterator j = modifiedResiduals.begin();
    while (j != modifiedResiduals.end()) {
        TileCache::Tile::Id id = *j;
        int level = id.first;
        int tx = id.second.first;
        int ty = id.second.second;
        TileCache::Tile::Id key = TileCache::Tile::getId(level, tx, ty);

        // finds the modified residual tile, creates it if necessary
        float *modifiedTile = NULL;
        map<TileCache::Tile::Id, float*>::iterator k = modifiedTiles.find(key);
        if (k == modifiedTiles.end()) {
            CPUTileStorage<float>::CPUSlot *slot = new CPUTileStorage<float>::CPUSlot(getCache()->getStorage().get(), tWidth * tWidth);
            doCreateTile(level, tx, ty, slot);
            modifiedTiles.insert(make_pair(key, slot->data));
            modifiedTile = slot->data;
            slot->data = NULL;
            delete slot;
        } else {
            modifiedTile = k->second;
        }

        int offset = 2 * tWidth + 2;
        float *tmp0 = tmp + offset;
        float *modifiedTile0 = modifiedTile + offset;

        int l = level + getDeltaLevel();
        int n = l < getMinLevel() ? 1 : (1 << (l - getMinLevel()));
        int w = l < getMinLevel() ? tSize >> (getMinLevel() - l) : tSize;

        // updates the modified residual tile content
        if (level == 0) {
            for (int y = -2; y <= w + 2; ++y) {
                for (int x = -2; x <= w + 2; ++x) {
                    modifiedTile0[x + y * tWidth] += getDeltaElevation(level, w, n, tx, ty, x, y);
                }
            }
        } else {
            /*
            // unoptimized version
            for (int y = -2; y <= w + 2; ++y) {
                for (int x = -2; x <= w + 2; ++x) {
                    float z = getDeltaElevation(level, w, n, tx, ty, x, y);
                    if (y%2 == 0) {
                        if (x % 2 == 0) {
                            z = 0.0f;
                        } else {
                            float z0 = getDeltaElevation(level, w, n, tx, ty, x - 3, y);
                            float z1 = getDeltaElevation(level, w, n, tx, ty, x - 1, y);
                            float z2 = getDeltaElevation(level, w, n, tx, ty, x + 1, y);
                            float z3 = getDeltaElevation(level, w, n, tx, ty, x + 3, y);
                            z -= ((z1 + z2) * 9.0f - (z0 + z3)) / 16.0f;
                        }
                    } else {
                        if (x % 2 == 0) {
                            float z0 = getDeltaElevation(level, w, n, tx, ty, x, y - 3);
                            float z1 = getDeltaElevation(level, w, n, tx, ty, x, y - 1);
                            float z2 = getDeltaElevation(level, w, n, tx, ty, x, y + 1);
                            float z3 = getDeltaElevation(level, w, n,tx, ty, x, y + 3);
                            z -= ((z1 + z2) * 9.0f - (z0 + z3)) / 16.0f;
                        } else {
                            for (int dy = -3; dy <= 3; dy += 2) {
                                float f = dy == -3 || dy == 3 ? -1.0f / 16.0f : 9.0f / 16.0f;
                                for (int dx = -3; dx <= 3; dx += 2) {
                                    float g = dx == -3 || dx == 3 ? -1.0f / 16.0f : 9.0f / 16.0f;
                                    z -= f * g * getDeltaElevation(level, w, n, tx, ty, x + dx, y + dy);
                                }
                            }
                        }
                    }

                    modifiedTile[(x + 2) + (y + 2) * tWidth] += z;
                }
            }
            */

            // optimized version
            for (int y = -4; y <= w + 4; y += 2) {
                for (int x = -2; x <= w + 2; x += 2) {
                    tmp0[x + (y / 2) * tWidth] = getDeltaElevation(level, w, n, tx, ty, x, y);
                }
                for (int x = -1; x <= w + 2; x += 2) {
                    float z0 = getDeltaElevation(level, w, n, tx, ty, x - 3, y);
                    float z1 = getDeltaElevation(level, w, n, tx, ty, x - 1, y);
                    float z2 = getDeltaElevation(level, w, n, tx, ty, x + 1, y);
                    float z3 = getDeltaElevation(level, w,n, tx, ty, x + 3, y);
                    float z = (z1 + z2) * (9.0f / 16.0f) - (z0 + z3) * (1.0f / 16.0f);
                    tmp0[x + (y / 2) * tWidth] = z;
                }
            }
            for (int y = -2; y <= w + 2; y += 2) {
                for (int x = -1; x <= w + 2; x += 2) {
                    float z = tmp0[x + (y / 2) * tWidth];
                    modifiedTile0[x + y * tWidth] += getDeltaElevation(level, w, n, tx, ty, x, y) - z;
                }
            }
            for (int y = -1; y <= w + 2; y += 2) {
                for (int x = -2; x <= w + 2; ++x) {
                    float z0 = tmp0[x + (y - 3) / 2 * tWidth];
                    float z1 = tmp0[x + (y - 1) / 2 * tWidth];
                    float z2 = tmp0[x + (y + 1) / 2 * tWidth];
                    float z3 = tmp0[x + (y + 3) / 2 * tWidth];
                    float z = (z1 + z2) * (9.0f / 16.0f) - (z0 + z3) * (1.0f / 16.0f);
                    modifiedTile0[x + y * tWidth] += getDeltaElevation(level, w, n, tx, ty, x, y) - z;
                }
            }
        }

        j++;
    }

    delete[] tmp;

    i = deltaElevations.begin();
    while (i != deltaElevations.end()) {
        delete[] i->second;
        i++;
    }
    deltaElevations.clear();
}

void EditResidualProducer::reset()
{
    map<TileCache::Tile::Id, float*>::iterator i = modifiedTiles.begin();
    while (i != modifiedTiles.end()) {
        delete[] i->second;
        i++;
    }
    modifiedTiles.clear();
    invalidateTiles();
}

float EditResidualProducer::getDeltaElevation(int level, int w, int n, int tx, int ty, int x, int y)
{
    x += tx * w;
    y += ty * w;
    int nw = n * w;
    if (x >= 0 && x <= nw && y >= 0 && y <= nw) {
        tx = min(x, nw - 1) / w;
        ty = min(y, nw - 1) / w;
        x = (x == nw ? w : x % w);
        y = (y == nw ? w : y % w);

        float *deltaElevation = NULL;
        TileCache::Tile::Id id = TileCache::Tile::getId(level, tx, ty);
        if (id == curId) {
            deltaElevation = curDeltaElevation;
        } else {
            map<TileCache::Tile::Id, float*>::iterator i = deltaElevations.find(id);
            if (i != deltaElevations.end()) {
                deltaElevation = i->second;
            }
            curId = id;
            curDeltaElevation = deltaElevation;
        }

        return deltaElevation == NULL ? 0.0f : deltaElevation[x + y * (tSize + 1)];
    } else {
        int x0 = clamp(x, 0, nw);
        int y0 = clamp(y, 0, nw);
        int x1 = 2 * x0 - x;
        int y1 = 2 * y0 - y;
        return 2.0f * getDeltaElevation(level, w, n, 0, 0, x0, y0) - getDeltaElevation(level, w, n, 0, 0, x1, y1);
    }
}

bool EditResidualProducer::doCreateTile(int level, int tx, int ty, TileStorage::Slot *data)
{
    TileCache::Tile::Id id = TileCache::Tile::getId(level, tx, ty);
    map<TileCache::Tile::Id, float*>::iterator it = modifiedTiles.find(id);
    if (it != modifiedTiles.end()) {
        float *src = it->second;
        float *dst = dynamic_cast<CPUTileStorage<float>::CPUSlot*>(data)->data;
        int tileWidth = getCache()->getStorage()->getTileSize();
        for (int i = 0; i < tileWidth * tileWidth; ++i) {
            dst[i] = src[i];
        }
        return true;
    } else {
        return ResidualProducer::doCreateTile(level, tx, ty, data);
    }
}

void EditResidualProducer::swap(ptr<EditResidualProducer> p)
{
    ResidualProducer::swap(p);
    std::swap(tWidth, p->tWidth);
    std::swap(tSize, p->tSize);
    std::swap(curId, p->curId);
    std::swap(curDeltaElevation, p->curDeltaElevation);
    std::swap(modifiedTiles, p->modifiedTiles);
    std::swap(deltaElevations, p->deltaElevations);
}

class EditResidualProducerResource : public ResourceTemplate<2, EditResidualProducer>
{
public:
    EditResidualProducerResource(ptr<ResourceManager> manager, const string &name, ptr<ResourceDescriptor> desc, const TiXmlElement *e = NULL) :
        ResourceTemplate<2, EditResidualProducer>(manager, name, desc)
    {
        e = e == NULL ? desc->descriptor : e;
        checkParameters(desc, e, "name,cache,file,delta,scale,");
        ResidualProducer::init(manager, this, name, desc, e);
        init();
    }
};

extern const char editResidualProducer[] = "editResidualProducer";

static ResourceFactory::Type<editResidualProducer, EditResidualProducerResource> EditResidualProducerType;

}
