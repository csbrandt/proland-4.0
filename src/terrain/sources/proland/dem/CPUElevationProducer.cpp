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

#include "proland/dem/CPUElevationProducer.h"

#include <sstream>

#include "ork/core/Logger.h"
#include "ork/resource/ResourceTemplate.h"
#include "ork/taskgraph/TaskGraph.h"
#include "proland/producer/CPUTileStorage.h"

using namespace std;
using namespace ork;

namespace proland
{

CPUElevationProducer::CPUElevationProducer(ptr<TileCache> cache, ptr<TileProducer> residualTiles) : TileProducer("CPUElevationProducer", "CreateCPUElevationTile")
{
    init(cache, residualTiles);
}

CPUElevationProducer::CPUElevationProducer() : TileProducer("CPUElevationProducer", "CreateCPUElevationTile")
{
}

void CPUElevationProducer::init(ptr<TileCache> cache, ptr<TileProducer> residualTiles)
{
    TileProducer::init(cache, true);
    this->residualTiles = residualTiles;
}

CPUElevationProducer::~CPUElevationProducer()
{
}

void CPUElevationProducer::getReferencedProducers(vector< ptr<TileProducer> > &producers) const
{
    producers.push_back(residualTiles);
}

void CPUElevationProducer::setRootQuadSize(float size)
{
    TileProducer::setRootQuadSize(size);
    residualTiles->setRootQuadSize(size);
}

int CPUElevationProducer::getBorder()
{
    assert(residualTiles->getBorder() == 2);
    return 2;
}

bool CPUElevationProducer::prefetchTile(int level, int tx, int ty)
{
    bool b = TileProducer::prefetchTile(level, tx, ty);
    if (!b) {
        int tileSize = getCache()->getStorage()->getTileSize() - 5;
        int residualTileSize = residualTiles->getCache()->getStorage()->getTileSize() - 5;
        int mod = residualTileSize / tileSize;
        if (residualTiles->hasTile(level, tx / mod, ty / mod)) {
            residualTiles->prefetchTile(level, tx / mod, ty / mod);
        }
    }
    return b;
}

float CPUElevationProducer::getHeight(ptr<TileProducer> producer, int level, float x, float y)
{
    float levelTileSize = producer->getRootQuadSize() / (1 << level);
    float s = producer->getRootQuadSize() / 2.0f;
    if (x <= -s || x >= s || y <= -s || y >= s) {
        return 0;
    }
    x += s;
    y += s;

    int tx = (int) floor(x / levelTileSize);
    int ty = (int) floor(y / levelTileSize);

    int tileWidth = producer->getCache()->getStorage()->getTileSize();
    int tileSize = tileWidth - 5;

    TileCache::Tile *t = producer->findTile(level, tx, ty);;

    if (t == NULL) {
        if (Logger::INFO_LOGGER != NULL) {
            Logger::INFO_LOGGER->logf("DEM", "Missing CPUElevation tile [%d:%d:%d] (coord %f:%f)", level, tx, ty, x, y);
        }
        return 0;
    }
    assert(t != NULL);
    CPUTileStorage<float>::CPUSlot *slot = dynamic_cast<CPUTileStorage<float>::CPUSlot*>(t->getData());
    assert(slot != NULL);
    float *tile = slot->data;
    x = 2.0f + (fmod(x, levelTileSize) / levelTileSize) * tileSize;
    y = 2.0f + (fmod(y, levelTileSize) / levelTileSize) * tileSize;
    int sx = (int) floor(x);
    int sy = (int) floor(y);

    return tile[sx + sy * tileWidth];
}

ptr<Task> CPUElevationProducer::startCreateTile(int level, int tx, int ty, unsigned int deadline, ptr<Task> task, ptr<TaskGraph> owner)
{
    ptr<TaskGraph> result = owner == NULL ? createTaskGraph(task) : owner;
    if (level > 0) {
        TileCache::Tile *t = getTile(level - 1, tx / 2, ty / 2, deadline);
        assert(t != NULL);
        result->addTask(t->task);
        result->addDependency(task, t->task);
    }

    int tileSize = getCache()->getStorage()->getTileSize() - 5;
    int residualTileSize = residualTiles->getCache()->getStorage()->getTileSize() - 5;
    int mod = residualTileSize / tileSize;
    if (residualTiles->hasTile(level, tx / mod, ty / mod)) {
        TileCache::Tile *t = residualTiles->getTile(level, tx / mod, ty / mod, deadline);
        assert(t != NULL);
        result->addTask(t->task);
        result->addDependency(task, t->task);
    }

    return result;
}

void CPUElevationProducer::beginCreateTile()
{
}

bool CPUElevationProducer::doCreateTile(int level, int tx, int ty, TileStorage::Slot *data)
{
    if (Logger::DEBUG_LOGGER != NULL) {
        ostringstream oss;
        oss << "CPUElevation tile " << getId() << " " << level << " " << tx << " " << ty;
        Logger::DEBUG_LOGGER->log("DEM", oss.str());
    }

    CPUTileStorage<float>::CPUSlot *cpuData = dynamic_cast<CPUTileStorage<float>::CPUSlot*>(data);
    assert(cpuData != NULL);

    int tileWidth = data->getOwner()->getTileSize();
    int tileSize = tileWidth - 5;

    CPUTileStorage<float>::CPUSlot *parentCpuData = NULL;
    if (level > 0) {
        TileCache::Tile *t = findTile(level - 1, tx / 2, ty / 2);
        assert(t != NULL);
        parentCpuData = dynamic_cast<CPUTileStorage<float>::CPUSlot*>(t->getData());
        assert(parentCpuData != NULL);
    }

    int residualTileWidth = residualTiles->getCache()->getStorage()->getTileSize();
    int mod = (residualTileWidth - 2 * residualTiles->getBorder() - 1) / tileSize;

    int rx = (tx % mod) * tileSize; // select the xy coord in residual
    int ry = (ty % mod) * tileSize;
    TileCache::Tile *t = residualTiles->findTile(level, tx / mod, ty / mod);

    CPUTileStorage<float>::CPUSlot *cpuTile = NULL;

    bool hasResidual = residualTiles->hasTile(level, tx / mod, ty / mod);
    if (hasResidual) {
        assert(t != NULL);
        cpuTile = dynamic_cast<CPUTileStorage<float>::CPUSlot*>(t->getData());
        assert(cpuTile != NULL);
    }

    int px = 1 + (tx % 2) * tileSize / 2; //select the xy coord in the parent tile
    int py = 1 + (ty % 2) * tileSize / 2;

    float *parentTile = NULL;
    if (level > 0) {
        parentTile = parentCpuData->data;
    }

    for (int j = 0; j < tileWidth; ++j) {
        for (int i = 0; i < tileWidth; ++i) {
            float z;
            float r = 0.0f;

            if (level == 0) {
                z = 0.0f;
            } else {
                if (j % 2 == 0) {
                    if (i % 2 == 0) {
                        z = parentTile[i / 2 + px + (j / 2 + py) * tileWidth];
                    } else {
                        float z0 = parentTile[i / 2 + px - 1 + (j / 2 + py) * tileWidth];
                        float z1 = parentTile[i / 2 + px + (j / 2 + py) * tileWidth];
                        float z2 = parentTile[i / 2 + px + 1 + (j / 2 + py) * tileWidth];
                        float z3 = parentTile[i / 2 + px + 2 + (j / 2 + py) * tileWidth];
                        z = ((z1 + z2) * 9 - (z0 + z3)) / 16;
                    }
                } else {
                    if (i % 2 == 0) {
                        float z0 = parentTile[i / 2  + px + (j / 2 - 1 + py) * tileWidth];
                        float z1 = parentTile[i / 2 + px + (j / 2 + py) * tileWidth];
                        float z2 = parentTile[i / 2 + px + (j / 2 + 1 + py) * tileWidth];
                        float z3 = parentTile[i / 2 + px + (j / 2 + 2 + py) * tileWidth];
                        z = ((z1 + z2) * 9 - (z0 + z3)) / 16;
                    } else {
                        int di, dj;
                        z = 0;
                        for (dj = -1; dj <= 2; ++dj) {
                            float f = dj == -1 || dj == 2 ? -1/16.0f : 9/16.0f;
                            for (di = -1; di <= 2; ++di) {
                                float g = di == -1 || di == 2 ? -1/16.0f : 9/16.0f;
                                z += f * g * parentTile[i / 2 + di + px + (j / 2 + dj + py) * tileWidth];
                            }
                        }
                    }
                }
            }
            if (hasResidual) {
                r = cpuTile->data[(int)(i + rx + (j + ry) * residualTileWidth)];
            }
            cpuData->data[i + j * tileWidth] = z + r;
        }
    }

    return true;
}

void CPUElevationProducer::endCreateTile()
{
}

void CPUElevationProducer::stopCreateTile(int level, int tx, int ty)
{
    if (level > 0) {
        TileCache::Tile *t = findTile(level - 1, tx / 2, ty / 2);
        assert(t != NULL);
        putTile(t);
    }

    int tileSize = getCache()->getStorage()->getTileSize() - 5;
    int residualTileSize = residualTiles->getCache()->getStorage()->getTileSize() - 5;
    int mod = residualTileSize / tileSize;
    if (residualTiles->hasTile(level, tx / mod, ty / mod)) {
        TileCache::Tile *t = residualTiles->findTile(level, tx / mod, ty / mod);
        assert(t != NULL);
        residualTiles->putTile(t);
    }
}

void CPUElevationProducer::swap(ptr<CPUElevationProducer> p)
{
    TileProducer::swap(p);
    std::swap(residualTiles, p->residualTiles);
}

class CPUElevationProducerResource : public ResourceTemplate<3, CPUElevationProducer>
{
public:
    CPUElevationProducerResource(ptr<ResourceManager> manager, const string &name, ptr<ResourceDescriptor> desc, const TiXmlElement *e = NULL) :
        ResourceTemplate<3, CPUElevationProducer>(manager, name, desc)
    {
        e = e == NULL ? desc->descriptor : e;
        ptr<TileCache> cache;
        ptr<TileProducer> residuals;
        checkParameters(desc, e, "name,cache,residuals,");
        cache = manager->loadResource(getParameter(desc, e, "cache")).cast<TileCache>();
        residuals = manager->loadResource(getParameter(desc, e, "residuals")).cast<TileProducer>();
        init(cache, residuals);
    }
};

extern const char cpuElevationProducer[] = "cpuElevationProducer";

static ResourceFactory::Type<cpuElevationProducer, CPUElevationProducerResource> CPUElevationProducerType;

}
