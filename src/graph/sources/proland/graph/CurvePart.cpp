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

#include "proland/graph/CurvePart.h"

#include "proland/math/geometry.h"

namespace proland
{

CurvePart::CurvePart()
{
}

CurvePart::~CurvePart()
{
}

CurveId CurvePart::getId() const
{
    CurveId i;
    i.id = NULL_ID;
    return i;
}

CurveId CurvePart::getParentId() const
{
    CurveId i;
    i.id = NULL_ID;
    return i;
}

int CurvePart::getType() const
{
    return 0;
}

float CurvePart::getWidth() const
{
    return -1;
}

CurvePtr CurvePart::getCurve() const
{
    return NULL;
}

vec2d CurvePart::getXY(const vec2d &start, int offset) const
{
    assert(start == getXY(0) || start == getXY(getEnd()));
    return getXY(start == getXY(0) ? offset : getEnd() - offset);
}

bool CurvePart::getIsControl(int i) const
{
    return false;
}

bool CurvePart::canClip(int i) const
{
    return true;
}

void CurvePart::clip(const box2d &clip, vector<CurvePart*> &result) const
{
    int start = -1;
    int cur = 0;
    while (cur < getEnd()) {
        int c = cur;
        vec2d p0 = getXY(cur);
        vec2d p1 = getXY(++cur);
        vec2d p2;
        vec2d p3;
        bool intersect;
        assert(canClip(cur - 1));
        if (!canClip(cur)) {
            p2 = getXY(++cur);
            if (!canClip(cur)) {
                p3 = getXY(++cur);
                assert(canClip(cur));
                intersect = clipCubic(clip, p0, p1, p2, p3);
            } else {
                intersect = clipQuad(clip, p0, p1, p2);
            }
        } else {
            intersect = clipSegment(clip, p0, p1);
        }

        if (intersect) {
            if (start == -1) {
                start = c;
            }
        } else {
            if (start != -1) {
                result.push_back(this->clip(start, c));
            }
            start = -1;
        }
    }
    if (start != -1) {
        result.push_back(this->clip(start, getEnd()));
    }
}

void CurvePart::clip(const vector<CurvePart*> &paths, const box2d &clip, vector<CurvePart*> &result)
{
    for (int i = 0; i < (int) paths.size(); ++i) {
        paths[i]->clip(clip, result);
    }
}

bool CurvePart::equals(CurvePtr c) const
{
    int n = getEnd();
    if (n != c->getSize() - 1) {
        return false;
    }
    for (int i = 0; i <= n; ++i) {
        if (getXY(i) != c->getXY(i) || getIsControl(i) != c->getIsControl(i)) {
            for (int j = 0; j <= n; ++j) {
                if (getXY(n - j) != c->getXY(j) || getIsControl(n - j) != c->getIsControl(j)) {
                    return false;
                }
            }
            return true;
        }
    }
    return true;
}

}
