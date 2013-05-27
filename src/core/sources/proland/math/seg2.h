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

#ifndef _PROLAND_SEG2_H_
#define _PROLAND_SEG2_H_

#include "pmath.h"

#include "ork/math/vec2.h"
#include "ork/math/box2.h"
#include "proland/math/geometry.h"

using namespace ork;

namespace proland
{

/**
 * A 2D segment.
 * @ingroup proland_math
 * @authors Eric Bruneton, Antoine Begault
 */
template<typename type>
struct seg2
{
    /**
     * One of the segment extremity.
     */
    vec2<type> a;

    /**
     * The vector joining #a to the other segment extremity.
     */
    vec2<type> ab;

    /**
     * Creates a new segment with the given extremities.
     *
     * @param a a segment extremity.
     * @param b the other segment extremity.
     */
    seg2(const vec2<type> &a, const vec2<type> &b);

    /**
     * Returns the square distance between the given point and the line
     * defined by this segment.
     *
     * @param p a point.
     */
    type lineDistSq(const vec2<type> &p) const;

    /**
     * Returns the square distance between the given point and this segment.
     *
     * @param p a point.
     */
    type segmentDistSq(const vec2<type> &p) const;

    /**
     * Returns true if this segment intersects the given segment.
     *
     * @param s a segment.
     */
    bool intersects(const seg2 &s) const;

    bool intersects(const seg2 &s, type &t0) const;

    /**
     * Returns true if this segment intersects the given segment. If there
     * is an intersection it is returned in the vector.
     *
     * @param s a segment.
     * @param i where to store the intersection point, if any.
     */
    bool intersects(const seg2 &s, vec2<type> *i) const;

    /**
     * Returns true if this segment is inside or intersects the given
     * bounding box.
     *
     * @param r a bounding box.
     */
    bool intersects(const box2<type> &r) const;

    /**
     * Returns true if this segment, with the given width, contains the given
     * point. More precisely this method returns true if the stroked path
     * defined from this segment, with a cap "butt" style, contains the given
     * point.
     *
     * @param p a point.
     * @param w width of this segment.
     */
    bool contains(const vec2<type> &p, type w) const;
};

typedef seg2<float> seg2f;

typedef seg2<double> seg2d;

template<typename type>
inline seg2<type>::seg2(const vec2<type> &a, const vec2<type> &b) : a(a), ab(b - a)
{
}

template<typename type>
inline type seg2<type>::lineDistSq(const vec2<type> &p) const
{
    vec2<type> ap = p - a;
    type dotprod = ab.dot(ap);
    type projLenSq = dotprod * dotprod / ab.squaredLength();
    return ap.squaredLength() - projLenSq;
}

template<typename type>
inline type seg2<type>::segmentDistSq(const vec2<type> &p) const
{
    vec2<type> ap = p - a;
    type dotprod = ab.dot(ap);
    type projlenSq;

    if (dotprod <= 0.0) {
        projlenSq = 0.0;
    } else {
        ap = ab - ap;
        dotprod = ab.dot(ap);

        if (dotprod <= 0.0) {
            projlenSq = 0.0;
        } else {
            projlenSq = dotprod * dotprod / ab.squaredLength();
        }
    }

    return ap.squaredLength() - projlenSq;
}

template<typename type>
inline bool seg2<type>::intersects(const seg2 &s) const
{
    vec2<type> aa = s.a - a;
    type det = cross(ab, s.ab);
    type t0 = cross(aa, s.ab) / det;

    if (t0 > 0 && t0 < 1) {
        type t1 = cross(aa, ab) / det;
        return t1 > 0 && t1 < 1;
    }

    return false;
}

template<typename type>
inline bool seg2<type>::intersects(const seg2 &s, type &t0) const
{
    vec2<type> aa = s.a - a;
    type det = cross(ab, s.ab);
    t0 = cross(aa, s.ab) / det;

    if (t0 > 0 && t0 < 1) {
        type t1 = cross(aa, ab) / det;
        return t1 > 0 && t1 < 1;
    }

    return false;
}

template<typename type>
inline bool seg2<type>::intersects(const seg2 &s, vec2<type> *i) const
{
    vec2<type> aa = s.a - a;
    type det = cross(ab, s.ab);
    type t0 = cross(aa, s.ab) / det;

    if (t0 > 0 && t0 < 1) {
        *i = a + ab * t0;
        type t1 = cross(aa, ab) / det;
        return t1 > 0 && t1 < 1;
    }

    return false;
}

template<typename type>
inline bool seg2<type>::intersects(const box2<type> &r) const
{
    vec2<type> b = a + ab;
    if (r.contains(a) || r.contains(b)) {
        return true;
    }

    box2<type> t = box2<type>(a, b);
    if (t.xmin > r.xmax || t.xmax < r.xmin || t.ymin > r.ymax || t.ymax < r.ymin) {
        return false;
    }

    type p0 = cross(ab, vec2<type>(r.xmin, r.ymin) - a);
    type p1 = cross(ab, vec2<type>(r.xmax, r.ymin) - a);
    if (p1 * p0 <= 0) {
        return true;
    }
    type p2 = cross(ab, vec2<type>(r.xmin, r.ymax) - a);
    if (p2 * p0 <= 0) {
        return true;
    }
    type p3 = cross(ab, vec2<type>(r.xmax, r.ymax) - a);
    if (p3 * p0 <= 0) {
        return true;
    }

    return false;
}

template<typename type>
inline bool seg2<type>::contains(const vec2<type> &p, type w) const
{
    vec2<type> ap = p - a;
    type dotprod = ab.dot(ap);
    type projlenSq;

    if (dotprod <= 0.0) {
        return false;
    } else {
        ap = ab - ap;
        dotprod = ab.dot(ap);
        if (dotprod <= 0.0) {
            return false;
        } else {
            projlenSq = dotprod * dotprod / ab.squaredLength();
            return ap.squaredLength() - projlenSq < w*w;
        }
    }
}

}

#endif
