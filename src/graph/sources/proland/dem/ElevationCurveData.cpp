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

#include "proland/dem/ElevationCurveData.h"

#include "proland/dem/CPUElevationProducer.h"

namespace proland
{

#define UNINITIALIZED -1e9

ElevationCurveData::ElevationCurveData(CurveId id, CurvePtr flattenCurve, ptr<TileProducer> elevations, bool monotonic) :
    CurveData(id, flattenCurve), elevations(elevations), monotonic(monotonic)
{
    sampleCount = max(2, (int) ceil(length / getSampleLength(flattenCurve)));
    sampleLength = length / (sampleCount - 1);
    samples = new float[sampleCount];
    if (monotonic) {
        monotonicSamples = new float[sampleCount];
        for (int i = 0; i < sampleCount; ++i) {
            monotonicSamples[i] = UNINITIALIZED;
        }
    } else {
        monotonicSamples = NULL;
    }
    smoothFactor = getSmoothFactor(flattenCurve);
    smoothedSamples = new float[sampleCount];
    for (int i = 0; i < sampleCount; ++i) {
        samples[i] = UNINITIALIZED;
        smoothedSamples[i] = UNINITIALIZED;
    }
}

ElevationCurveData::~ElevationCurveData()
{
    delete[] samples;
    if (monotonicSamples != NULL) {
        delete[] monotonicSamples;
    }
    delete[] smoothedSamples;
}

float ElevationCurveData::getSample(const vec2d &p)
{
    int level = 0;
    float sampleLength = getSampleLength(flattenCurve);
    float rootQuadSize = elevations->getRootQuadSize();
    float l = rootQuadSize / (elevations->getCache()->getStorage()->getTileSize() - elevations->getBorder() * 2 - 1.0f);
    while (l > sampleLength) {
        l = l/2;
        level += 1;
    }
    return CPUElevationProducer::getHeight(elevations, level, p.x, p.y);
}

float ElevationCurveData::getSample(int i)
{
    i = max(0, min(sampleCount - 1, i));
    float f = samples[i];
    if (f == UNINITIALIZED) {
        int level = 0;
        float maxSampleLength = getSampleLength(flattenCurve);

        NodePtr pt = NULL;
        if (i == 0) {
            pt = flattenCurve->getStart();
        } else if (i == sampleCount - 1) {
            pt = flattenCurve->getEnd();
        }
        if (pt != NULL) {
            for (int j = 0; j < pt->getCurveCount(); ++j) {
                float l = getSampleLength(pt->getCurve(j));
                maxSampleLength = max(maxSampleLength, l);
            }
        }
        float rootQuadSize = elevations->getRootQuadSize();
        float rootSampleLength = rootQuadSize / (elevations->getCache()->getStorage()->getTileSize() - elevations->getBorder() * 2 - 1.0f);
        float l = rootSampleLength;

        while (l > maxSampleLength) {
            l = l/2;
            level += 1;
        }
        vec2d p;
        flattenCurve->getCurvilinearCoordinate(sampleLength * i, &p, NULL);
        f = samples[i] = CPUElevationProducer::getHeight(elevations, level, p.x, p.y);
    }

    return f;
}

float ElevationCurveData::getMonotonicSample(int i)
{
    if (monotonic) {
        i = max(0, min(sampleCount - 1, i));
        float f = monotonicSamples[i];
        if (f == UNINITIALIZED) {
            float h0 = getSample(0);
            float h1 = getSample(sampleCount - 1);
            if (h0 < h1) {
                f = h1;
                for (int j = sampleCount - 1; j >= i; --j) {
                    f = std::max(std::min(f, getSample(j)), h0);
                }
            } else {
                f = h0;
                for (int j = 0; j <= i; ++j) {
                    f = std::max(std::min(f, getSample(j)), h1);
                }
            }
            monotonicSamples[i] = f;
        }
        return f;
    } else {
        return getSample(i);
    }
}

float ElevationCurveData::getSymetricSample(int i)
{
    int n = sampleCount - 1;
    if (i > n) {
        return 2*getMonotonicSample(n) - getMonotonicSample(2*n - i);
    } else if (i < 0) {
        return 2*getMonotonicSample(0) - getMonotonicSample(-i);
    } else {
        return getMonotonicSample(i);
    }
}

float ElevationCurveData::getSmoothedSample(int i)
{
    i = max(0, min(sampleCount - 1, i));
    float f = smoothedSamples[i];
    if (f == UNINITIALIZED) {
        f = 0;
        float n = 0;
        int w = max(1, smoothFactor);
        for (int j = i - w; j <= i + w; ++j) {
            // box filter
            float filter = 1.0;
            // gaussian filter
            //float x = ((float) j - i) / w;
            //float filter = exp(-2.0 * x * x);
            f += getSymetricSample(j) * filter;
            n += filter;
        }
        f = f / n;
        smoothedSamples[i] = f;
    }
    return f;
}

float ElevationCurveData::getStartHeight()
{
    return getSample(0);
}

float ElevationCurveData::getEndHeight()
{
    return getSample(sampleCount - 1);
}

float ElevationCurveData::getAltitude(float s)
{
    float L = getCurvilinearLength();
    float l = flattenCurve->getCurvilinearLength(s, NULL, NULL);

    if (l < startCapLength) {
        return getStartHeight();
    }
    if (l > L - endCapLength) {
        return getEndHeight();
    }

    float flat0 = startCapLength;
    float flat1 = endCapLength;

    if (flat0 + flat1 > L) {
        float h0 = getStartHeight();
        float h1 = getEndHeight();
        float c = (l - startCapLength) / (L - startCapLength - endCapLength);
        c = (float) (0.5f + 0.5f * sin((c - 0.5f) * M_PI_F)); // ??????
        return h0 * (1 - c) + h1 * c;
    }

    int i = (int) floor(l / sampleLength);
    float t = l / sampleLength - i;

    float h0 = getSmoothedSample(i);
    float h1 = getSmoothedSample(i + 1);

    float hp0 = (h1 - getSmoothedSample(i - 1))/2;
    float hp1 = (getSmoothedSample(i + 2) - h0)/2;
    float dhp = hp1 - hp0;
    float dh = h1 - h0 - hp0;
    float z = (((dhp - 2*dh)*t + (3*dh - dhp))*t + hp0)*t + h0; // ??????

    if (l < flat0) {
        float z0 = getStartHeight();
        float c = (l - startCapLength) / (flat0 - startCapLength);
        c = (float) (0.5f + 0.5f * sin((c - 0.5f) * M_PI_F)); // ??????
        return z0 * (1 - c) + z * c;
    }
    if (l > L - flat1) {
        l = L - l;
        float z1 = getEndHeight();
        float c = (l - endCapLength) / (flat1 - endCapLength);
        c = (float) (0.5f + 0.5f * sin((c - 0.5f) * M_PI_F)); // ??????
        return z1 * (1 - c) + z * c;
    }
    return z;
}

float ElevationCurveData::getSampleLength(CurvePtr c)
{
    float width = min(c->getWidth(), 20.0f);
    return 20.0f * width / 6.0f;
}

int ElevationCurveData::getSmoothFactor(CurvePtr c)
{
    float width = min(c->getWidth(), 20.0f);
    return (int) (width / 3);
}

void ElevationCurveData::getUsedTiles(set<TileCache::Tile::Id> &tiles, float rootSampleLength)
{
    if ((int) usedTiles.size() == 0) {
        float sampleLength = getSampleLength(flattenCurve);
        int level = 0;
        float l = rootSampleLength;
        while (l > sampleLength) {
            l = l/2;
            level += 1;
        }
        float rootQuadSize = elevations->getRootQuadSize();
        float levelTileSize = rootQuadSize / (1 << level);

        int tx;
        int ty;
        for(int i = 1; i < sampleCount - 1; ++i) {
            vec2d p;
            flattenCurve->getCurvilinearCoordinate(sampleLength * i, &p, NULL);
            tx = (int) floor((p.x + rootQuadSize / 2.0f) / levelTileSize);
            ty = (int) floor((p.y + rootQuadSize / 2.0f) / levelTileSize);
            int nTiles = 1 << level;
            if (tx < nTiles && tx >= 0 && ty < nTiles && ty >= 0) {
                usedTiles.insert(make_pair(level, make_pair(tx, ty)));
            }
        }

        for (int i = 0; i < 2; ++i) {
            NodePtr pt = i == 0 ? flattenCurve->getStart() : flattenCurve->getEnd();
            float maxSampleLength = sampleLength;
            for (int j = 0; j < pt->getCurveCount(); ++j) {
                float l = getSampleLength(pt->getCurve(j));
                maxSampleLength = max(maxSampleLength, l);
            }
            level = 0;
            l = rootSampleLength;
            while (l > maxSampleLength) {
                l = l / 2;
                level += 1;
            }
            int nTiles = 1 << level;
            levelTileSize = rootQuadSize / nTiles;
            tx = (int) floor((pt->getPos().x + rootQuadSize / 2.0f) / levelTileSize);
            ty = (int) floor((pt->getPos().y + rootQuadSize / 2.0f) / levelTileSize);
            if (tx < nTiles && tx >= 0 && ty < nTiles && ty >= 0) {
                usedTiles.insert(make_pair(level, make_pair(tx, ty)));
            }
        }
    }

    tiles.insert(usedTiles.begin(), usedTiles.end());
}

}
