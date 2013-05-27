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

#include "proland/graph/LineCurvePart.h"

namespace proland
{

LineCurvePart::LineCurvePart(vec2d start, vec2d end) :
    start(start), end(end)
{
}

int LineCurvePart::getEnd() const
{
    return 1;
}

vec2d LineCurvePart::getXY(int i) const
{
    return i == 0 ? start : end;
}

bool LineCurvePart::getIsControl(int i) const
{
    return false;
}

float LineCurvePart::getS(int i) const
{
    return (float) i;
}

box2d LineCurvePart::getBounds() const
{
    return box2d(start, end);
}

CurvePart *LineCurvePart::clip(int start, int end) const
{
    assert(start == 0 && end == 1);
    return new LineCurvePart(this->start, this->end);
}

}
