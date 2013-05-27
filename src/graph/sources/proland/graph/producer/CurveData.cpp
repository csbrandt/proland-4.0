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

#include "proland/graph/producer/CurveData.h"

#include "proland/producer/CPUTileStorage.h"
#include "proland/graph/producer/GraphLayer.h"

namespace proland
{

#define UNINITIALIZED -1e9


CurveData::CurveData(CurveId id, CurvePtr flattenCurve) :
    id(id), flattenCurve(flattenCurve)
{
    length = flattenCurve->computeCurvilinearLength();

    if (flattenCurve->getStart()->getCurveCount() > 1) {
        NodePtr n = flattenCurve->getStart();
        startCapLength = getCapLength(n, flattenCurve->getXY(n, 1));
    } else {
        startCapLength = 0.f;
    }
    if (flattenCurve->getEnd()->getCurveCount() > 1) {
        NodePtr n = flattenCurve->getEnd();
        endCapLength = getCapLength(n, flattenCurve->getXY(n, 1));
    } else {
        endCapLength = 0.f;
    }
}

CurveData::~CurveData()
{
    flattenCurve = NULL;
}


CurveId CurveData::getId()
{
    return id;
}

float CurveData::getCapLength(NodePtr p, vec2d q)
{
    vec2d o = p->getPos();
    float capLength = 0;
    for (int i = 0; i < p->getCurveCount(); ++i) {
        CurvePtr ipath = p->getCurve(i);
        if (!(ipath->getId() == getId())) {
            vec2d r = ipath->getXY(p, 1);
            if (abs(angle(q - o, r - o) - M_PI) < 0.01) {
                continue;
            }
            float pw = 2 * flattenCurve->getWidth();
            float ipw = 2 * ipath->getWidth();
            vec2d corner = proland::corner(o, q, r, (double) pw, (double) ipw);
            float dot = (q - o).dot(corner - o);
            capLength = max((double) capLength, dot / (o - q).length());
        }
    }
    return ceil(capLength);
}

float CurveData::getStartCapLength()
{
    return startCapLength / 2;
}

float CurveData::getEndCapLength()
{
    return endCapLength / 2;
}

float CurveData::getCurvilinearLength()
{
    return length;
}

float CurveData::getCurvilinearLength(float s, vec2d *p, vec2d *n)
{
    return flattenCurve->getCurvilinearLength(s, p, n);
}

float CurveData::getCurvilinearCoordinate(float l, vec2d *p, vec2d *n)
{
    return flattenCurve->getCurvilinearCoordinate(l, p, n);
}

void CurveData::getUsedTiles(set<TileCache::Tile::Id> &tiles, float rootSampleLength)
{
    return;
}

float CurveData::getS(int rank)
{
    return flattenCurve->getS(rank);
}

CurvePtr CurveData::getFlattenCurve()
{
    return flattenCurve;
}

}
