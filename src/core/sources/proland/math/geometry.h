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

#ifndef _PROLAND_GEOMETRY_H_
#define _PROLAND_GEOMETRY_H_

#include "pmath.h"

#include "ork/math/vec2.h"
#include "ork/math/box2.h"

using namespace ork;

namespace proland
{

/**
 * Returns the z coordinate of the cross product of this vector and of the
 * given vector.
 * @ingroup proland_math
 */
template <typename type>
inline type cross(const vec2<type> u, const vec2<type> &v)
{
    return u.x * v.y - u.y * v.x;
}

/**
 * Returns the angle between this vector and the given vector. The
 * returned angle is in the 0 2.PI interval.
 * @ingroup proland_math
 */
template<typename type>
static type angle(const vec2<type> &u, const vec2<type> &v)
{
    type t = atan2(u.x * v.y - u.y * v.x, u.x * v.x + u.y * v.y);
    return t > 0 ? t : t + 2 * (type)(M_PI_L);
}

/**
 * Returns the intersection point between two half lines with widths.
 * The two half lines are defined by ab and ac, with widths width1 and
 * width2 respectively. The returned point is the intersection between
 * the lines parallel to ab and ac and at a distance width1/2 and
 * width2/2 from these lines. There are four such intersections: the
 * returned one is the one inside the sector defined by the ab and ac
 * half lines.
 * @ingroup proland_math
 *
 * @param a extremity of ab and ac half lines.
 * @param b defines the ab half line.
 * @param c defines the ac half line.
 * @param width1 width of the ab half line.
 * @param width2 width of the ac half line.
 */
template<typename type>
static vec2<type> corner(const vec2<type> &a, const vec2<type> &b, const vec2<type> &c, type width1, type width2)
{
    type dx0 = b.x - a.x;
    type dy0 = b.y - a.y;
    type dx1 = c.x - a.x;
    type dy1 = c.y - a.y;
    type k0 = width1 / (2 * sqrt(dx0*dx0 + dy0*dy0));
    type k1 = width2 / (2 * sqrt(dx1*dx1 + dy1*dy1));
    type x0, y0, x1, y1;
    if (dx0 * dy1 - dy0 * dx1 > 0) {
        x0 = a.x - k0*dy0;
        y0 = a.y + k0*dx0;
        x1 = a.x + k1*dy1;
        y1 = a.y - k1*dx1;
    } else {
        x0 = a.x + k0*dy0;
        y0 = a.y - k0*dx0;
        x1 = a.x - k1*dy1;
        y1 = a.y + k1*dx1;
    }
    type det = dy0*dx1 - dx0*dy1;
    type t;
    if (det == 0) {
        t = 0;
    } else {
        t = (dy1*(x0-x1) - dx1*(y0-y1)) / det;
    }
    return vec2<type>(x0 + t*dx0, y0 + t*dy0);
}

/**
 * Returns true if the given point is inside this bounding box.
 * @ingroup proland_math
 *
 * @param a an arbitrary point.
 */
template<typename type>
bool clipPoint(const box2<type> &box, const vec2<type> &a)
{
    return box.contains(a);
}

/**
 * Returns true if the given box is inside or intersects this bounding box.
 * @ingroup proland_math
 *
 * @param a an arbitrary bounding box.
 */
template<typename type>
bool clipRectangle(const box2<type> &box, const box2<type> &a)
{
    return a.xmax >= box.xmin && a.xmin <= box.xmax && a.ymax >= box.ymin && a.ymin <= box.ymax;
}

/**
 * Alias for clipRectangle.
 * @ingroup proland_math
 */
template<typename type>
bool intersects(const box2<type> &box, const box2<type> &a)
{
    return a.xmax >= box.xmin && a.xmin <= box.xmax && a.ymax >= box.ymin && a.ymin <= box.ymax;
}

/**
 * Returns true if the given segment is inside or may intersect this
 * bounding box.
 * @ingroup proland_math
 *
 * @param a a segment extremity.
 * @param b the other segment extremity.
 */
template<typename type>
bool clipSegment(const box2<type> &box, const vec2<type> &a, const vec2<type> &b)
{
    if (box.contains(a) || box.contains(b)) {
        return true;
    }

    box2<type> ab = box2<type>(a, b);

    if (ab.xmax < box.xmin || ab.xmin > box.xmax || ab.ymax < box.ymin || ab.ymin > box.ymax) {
        return false;
    }

    // conservative result: potential intersection
    return true;
}

/**
 * Returns true if the given quadratic Bezier arc is inside or may
 * intersect this bounding box.
 * @ingroup proland_math
 *
 * @param a Bezier arc extremity.
 * @param b Bezier arc control point.
 * @param c the other Bezier arc extremity.
 */
template<typename type>
bool clipQuad(const box2<type> &box, const vec2<type> &a, const vec2<type> &b, const vec2<type> &c)
{
    if (box.contains(a) || box.contains(b) || box.contains(c)) {
        return true;
    }

    box2<type> abc = box2<type>(a, b).enlarge(c);

    if (abc.xmax < box.xmin || abc.xmin > box.xmax || abc.ymax < box.ymin || abc.ymin > box.ymax) {
        return false;
    }

    // conservative result: potential intersection
    return true;
}

/**
 * Returns true if the given cubic Bezier arc is inside or may
 * intersect this bounding box.
 * @ingroup proland_math
 *
 * @param a Bezier arc extremity.
 * @param b first Bezier arc control point.
 * @param c second Bezier arc control point.
 * @param d the other Bezier arc extremity.
 */
template<typename type>
bool clipCubic(const box2<type> &box, const vec2<type> &a, const vec2<type> &b, const vec2<type> &c, const vec2<type> &d)
{
    if (box.contains(a) || box.contains(b) || box.contains(c) || box.contains(d)) {
        return true;
    }

    box2<type> abcd = box2<type>(a, b).enlarge(box2<type>(c, d));

    if (abcd.xmax < box.xmin || abcd.xmin > box.xmax || abcd.ymax < box.ymin || abcd.ymin > box.ymax) {
        return false;
    }

    // conservative result: potential intersection
    return true;
}

}

#endif
