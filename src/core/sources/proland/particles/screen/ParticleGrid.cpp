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

#include "proland/particles/screen/ParticleGrid.h"

#include "ork/core/Logger.h"

using namespace std;

namespace proland
{

ParticleGrid::ParticleGrid(float radius, int maxParticlesPerCell, float gridFactor) :
    Object("ParticleGrid"), viewport(0, 0, 0, 0)
{
    this->radius = radius;
    this->maxParticlesPerCell = maxParticlesPerCell;
    this->cellSizes = NULL;
    this->gridFactor = gridFactor;
}

ParticleGrid::~ParticleGrid()
{
    deleteGrid();
}

float ParticleGrid::getParticleRadius() const
{
    return radius;
}

void ParticleGrid::setParticleRadius(float radius)
{
    if (radius != this->radius) {
        this->radius = radius;
        deleteGrid();
    }
}

int ParticleGrid::getMaxParticlesPerCell() const
{
    return maxParticlesPerCell;
}

void ParticleGrid::setMaxParticlesPerCell(int maxParticlesPerCell)
{
    this->maxParticlesPerCell = maxParticlesPerCell;
}

box2i ParticleGrid::getViewport() const
{
    return viewport;
}

void ParticleGrid::setViewport(const box2i &viewport)
{
    if (viewport != this->viewport) {
        this->viewport = viewport;
        deleteGrid();
    }
}

vec2i ParticleGrid::getGridSize() const
{
    return gridSize;
}

vec2i ParticleGrid::getCell(const vec2f &p)
{
    int i = (int) floor((p.x - viewport.xmin) / (viewport.xmax - viewport.xmin) * gridSize.x);
    int j = (int) floor((p.y - viewport.ymin) / (viewport.ymax - viewport.ymin) * gridSize.y);
    return vec2i(i, j);
}

int ParticleGrid::getCellSize(const vec2i &cell)
{
    assert(cellSizes != NULL && cell.x >= 0 && cell.x < gridSize.x && cell.y >= 0 && cell.y < gridSize.y);
    return cellSizes[cell.x + cell.y * gridSize.x];
}

ScreenParticleLayer::ScreenParticle **ParticleGrid::getCellContent(const vec2i &cell)
{
    assert(cellSizes != NULL && cell.x >= 0 && cell.x < gridSize.x && cell.y >= 0 && cell.y < gridSize.y);
    return cellContents + (cell.x + cell.y * gridSize.x) * maxParticlesPerCell;
}

void ParticleGrid::addParticle(ScreenParticleLayer::ScreenParticle *p, float intensity)
{
    assert(cellSizes != NULL);
    vec2i cmin = getCell(p->screenPos - vec2f(radius, radius));
    vec2i cmax = getCell(p->screenPos + vec2f(radius, radius));

    cmin.x = max(0, cmin.x);
    cmin.y = max(0, cmin.y);
    cmax.x = min(gridSize.x - 1, cmax.x);
    cmax.y = min(gridSize.y - 1, cmax.y);
    bool overflow = false;
    int maxN = 0;

    for (int j = cmin.y; j <= cmax.y; ++j) {
        for (int i = cmin.x; i <= cmax.x; ++i) {
            int index = i + j * gridSize.x;
            int size = cellSizes[index];
            if (size < maxParticlesPerCell) {
                cellSizes[index] = size + 1;
                cellContents[size + index * maxParticlesPerCell] = p;
                intensities[size + index * maxParticlesPerCell] = intensity;
            } else {
                maxN = max(maxN, size);
                overflow = true;
                float minI = intensity;
                int kmin = 0;
                for (int k = 0; k < maxParticlesPerCell; k++) {
                    // this should sort out low-intensity particles,
                    // which should not have much impact on visual result,
                    // but prevents cells from being overcrowded.
                    if (intensities[k + index * maxParticlesPerCell] < minI) {
                        minI = intensities[k + index * maxParticlesPerCell];
                        kmin = k;
                    }
                    if (minI == 0.f) {
                        break;
                    }
                }
                if (minI < intensity) {
                    cellContents[kmin + index * maxParticlesPerCell] = p;
                    intensities[kmin + index * maxParticlesPerCell] = intensity;
                }
            }
        }
    }
    if (overflow && Logger::DEBUG_LOGGER != NULL) {
        Logger::DEBUG_LOGGER->logf("PARTICLES", "Too many particles per cell Reached %d. Max allowed %d", maxN, maxParticlesPerCell);
    }
}

void ParticleGrid::clear()
{
    if (cellSizes != NULL) {
        int n = gridSize.x * gridSize.y;
        for (int i = 0; i < n; ++i) {
            cellSizes[i] = 0;
        }
    } else {
        createGrid();
    }
}

ptr<Texture2D> ParticleGrid::copyToTexture(ptr<ScreenParticleLayer> l, ptr<Texture2D> t, int &pixelsPerCell)
{
    assert(cellSizes != NULL);
    pixelsPerCell = (int) ceil(float(maxParticlesPerCell) / 4.0f);

    int width = gridSize.x * pixelsPerCell;
    int height = gridSize.y;
    if (t == NULL || t->getWidth() != width || t->getHeight() != height) {
        t = new Texture2D(width, height, RGBA32F,
            RGBA, FLOAT, Texture::Parameters().wrapS(CLAMP_TO_BORDER).wrapT(CLAMP_TO_BORDER).min(NEAREST).mag(NEAREST), Buffer::Parameters(), CPUBuffer(NULL));
    }

    ptr<ParticleStorage> storage = l->getOwner()->getStorage();
    for (int j = 0; j < gridSize.y; ++j) {
        for (int i = 0; i < gridSize.x; i++) {
            int index = i + j * gridSize.x;
            int pindex = index * maxParticlesPerCell;
            int size = cellSizes[index];
            for (int k = 0; k < size; ++k, ++pindex) {
                cellIndexes[pindex] = storage->getParticleIndex(l->getParticle(cellContents[pindex]));
            }
            if (size < maxParticlesPerCell) {
                cellIndexes[pindex] = -1;
            }
        }
    }
    t->setSubImage(0, 0, 0, width, height, RGBA, FLOAT, Buffer::Parameters(), CPUBuffer(cellIndexes));
    return t;
}

void ParticleGrid::createGrid()
{
    if (cellSizes == NULL) {
        gridSize.x = (int) (gridFactor * (viewport.xmax - viewport.xmin) / (radius));
        gridSize.y = (int) (gridFactor * (viewport.ymax - viewport.ymin) / (radius));

        cellSizes = new int[gridSize.x * gridSize.y];
        cellContents = new ScreenParticleLayer::ScreenParticle*[gridSize.x * gridSize.y * maxParticlesPerCell];
        cellIndexes = new int[gridSize.x * gridSize.y * maxParticlesPerCell];
        intensities = new float[gridSize.x * gridSize.y * maxParticlesPerCell];
        int n = gridSize.x * gridSize.y;
        for (int i = 0; i < n; ++i) {
            cellSizes[i] = 0;
        }
        for (int i = 0; i < n * maxParticlesPerCell; i++) {
            cellIndexes[i] = -1;
        }
    }
}

void ParticleGrid::deleteGrid()
{
    if (cellSizes != NULL) {
        delete[] cellSizes;
        delete[] cellContents;
        delete[] cellIndexes;
        delete[] intensities;
        cellSizes = NULL;
    }
}

}
