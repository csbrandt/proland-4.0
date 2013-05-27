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

#include "proland/rivers/graph/HydroCurve.h"

#include "proland/math/seg2.h"
#include "proland/rivers/graph/HydroGraph.h"

using namespace ork;

namespace proland
{

HydroCurve::HydroCurve(Graph *owner) : Curve(owner), potential(-1.f)
{
    river.id = NULL_ID;
}

HydroCurve::HydroCurve(Graph *owner, CurvePtr c, NodePtr s, NodePtr e) :
    Curve(owner, c, s ,e)
{
    if (c != NULL) {
        ptr<HydroCurve> h = c.cast<HydroCurve>();
        assert(h != NULL);
        setRiver(h->getRiver());
        potential = h->getPotential();
    } else {
        river.id = NULL_ID;
        potential = -1.f;
    }
}

HydroCurve::~HydroCurve()
{
}

float HydroCurve::getWidth() const
{
    if (river.id != NULL_ID) {
        return getRiverPtr()->getWidth();
    }

    return width;
}

float HydroCurve::getPotential() const
{
    return potential;
}

void HydroCurve::setPotential(float potential)
{
    this->potential = potential;
}

CurveId HydroCurve::getRiver() const
{
    return river;
}

CurvePtr HydroCurve::getRiverPtr() const
{
    return getOwner()->getAncestor()->getCurve(river);
}

void HydroCurve::setRiver(CurveId river)
{
    this->river = river;
    if (river.id != NULL_ID) {
        this->width = getRiverPtr()->getWidth();
    }
}

void HydroCurve::print() const
{
    printf("%d-> %d %f %d %f %d\n", getId().id, getSize(), getWidth(), getType(), getPotential(), getRiver().id);
    vec2d v;
    for (int i = 0; i < getSize(); i++) {
        v = getXY(i);
        printf("%f %f %d %f %f\n", v.x, v.y, getIsControl(i), getS(i), getL(i));
    }
}

}
