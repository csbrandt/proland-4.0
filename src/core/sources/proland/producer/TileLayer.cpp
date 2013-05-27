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

#include "proland/producer/TileLayer.h"

using namespace std;

namespace proland
{

TileLayer::TileLayer(const char *type, bool deform) :
    Object(type), tileSize(0), tileBorder(0), rootQuadSize(0.0f), deform(deform), enabled(true)
{
}

TileLayer::~TileLayer()
{
}

void TileLayer::init(bool deform)
{
    this->deform = deform;
    this->enabled = true;
}

ptr<TileCache> TileLayer::getCache()
{
    return cache;
}

int TileLayer::getProducerId()
{
    return producerId;
}

int TileLayer::getTileSize()
{
    return tileSize;
}

int TileLayer::getTileBorder()
{
    return tileBorder;
}

float TileLayer::getRootQuadSize()
{
    return rootQuadSize;
}

vec3d TileLayer::getTileCoords(int level, int tx, int ty)
{
    double ox = rootQuadSize * (double(tx) / (1 << level) - 0.5f);
    double oy = rootQuadSize * (double(ty) / (1 << level) - 0.5f);
    double l = rootQuadSize / (1 << level);
    return vec3d(ox, oy, l);
}

bool TileLayer::isDeformed()
{
    return deform;
}

void TileLayer::getDeformParameters(vec3d tileCoords, vec2d &nx, vec2d &ny, vec2d &lx, vec2d &ly)
{
    if (isDeformed()) {
        double x = tileCoords.x + tileCoords.z / 2.0f;
        double y = tileCoords.y + tileCoords.z / 2.0f;
        double R = getRootQuadSize() / 2.0f;
        double yR = y*y + R*R;
        double xyR = x*x + yR;
        double d = R * sqrt(xyR);
        double e = R / (sqrt(yR) * xyR);
        nx = vec2d(x*y / d, yR / d);
        ny = vec2d(-((x*x + R*R) / d), -(x*y / d));
        lx = vec2d(e * yR, 0.0);
        ly = vec2d(-(e * x * y), e * d);
    }
}

bool TileLayer::isEnabled()
{
    return enabled;
}

void TileLayer::setIsEnabled(bool enabled)
{
    this->enabled = enabled;
    invalidateTiles();
}

void TileLayer::setCache(ptr<TileCache> cache, int producerId)
{
    this->cache = cache;
    this->producerId = producerId;
}

void TileLayer::getReferencedProducers(vector< ptr<TileProducer> > &producers) const
{
}

void TileLayer::setTileSize(int tileSize, int tileBorder, float rootQuadSize)
{
    this->tileSize = tileSize;
    this->tileBorder = tileBorder;
    this->rootQuadSize = rootQuadSize;
}

void TileLayer::useTile(int level, int tx, int ty, unsigned int deadline)
{
}

void TileLayer::unuseTile(int level, int tx, int ty)
{
}

void TileLayer::prefetchTile(int level, int tx, int ty)
{
}

void TileLayer::startCreateTile(int level, int tx, int ty, unsigned int deadline, ptr<Task> task, ptr<TaskGraph> owner)
{
}

void TileLayer::beginCreateTile()
{
}

void TileLayer::endCreateTile()
{
}

void TileLayer::stopCreateTile(int level, int tx, int ty)
{
}

void TileLayer::swap(ptr<TileLayer> p)
{
    cache->invalidateTiles(producerId);
    p->cache->invalidateTiles(p->producerId);
    std::swap(cache, p->cache);
    std::swap(producerId, p->producerId);
    std::swap(tileSize, p->tileSize);
    std::swap(tileBorder, p->tileBorder);
    std::swap(rootQuadSize, p->rootQuadSize);
    std::swap(deform, p->deform);
}

void TileLayer::invalidateTiles()
{
    getCache()->invalidateTiles(getProducerId());
}

}
