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

#include "proland/math/color.h"

using namespace std;

namespace proland
{

vec3f rgb2hsl(const vec3f &rgb)
{
    float vmin = min(rgb.x, min(rgb.y, rgb.z));
    float vmax = max(rgb.x, max(rgb.y, rgb.z));
    float dmax = vmax - vmin;
    float h, s, l;
    l = (vmax + vmin) / 2;
    if (dmax == 0) {
        h = 0;
        s = 0;
    } else {
        if (l < 0.5) {
            s = dmax / (vmax + vmin);
        } else {
            s = dmax / (2 - vmax - vmin);
        }
        float d[3];
        d[0] = (((vmax - rgb.x) / 6) + (dmax / 2)) / dmax;
        d[1] = (((vmax - rgb.y) / 6) + (dmax / 2)) / dmax;
        d[2] = (((vmax - rgb.z) / 6) + (dmax / 2)) / dmax;
        if (rgb.x == vmax) {
            h = d[2] - d[1];
        } else if (rgb.y == vmax) {
            h = (1.0f / 3) + d[0] - d[2];
        } else /*if (rgb.z == vmax)*/ {
            h = (2.0f / 3) + d[1] - d[0];
        }
        if (h < 0) {
            h += 1;
        }
        if (h > 1) {
            h -= 1;
        }
    }
    return vec3f(h, s, l);
}

static float h2rgb(float v1, float v2, float vH)
{
    float r;
    if (vH < 0) vH += 1;
    if (vH > 1) vH -= 1;
    if (vH < 1.0 / 6) {
        r = v1 + (v2 - v1) * 6 * vH;
    } else if (vH < 1.0 / 2) {
        r = v2;
    } else if (vH < 2.0 / 3) {
        r = v1 + (v2 - v1) * ((2.0f / 3) - vH) * 6;
    } else {
        r = v1;
    }
    return r;
}

vec3f hsl2rgb(const vec3f &hsl)
{
    float s = hsl.y;
    float l = hsl.z;
    if (s == 0) {
        return vec3f(l, l, l);
    } else {
       float v1, v2;
       if (l < 0.5) {
           v2 = l * (1 + s);
       } else {
           v2 = l + s - s * l;
       }
       v1 = 2 * l - v2;
       float r = h2rgb(v1, v2, hsl.x + 1.0f / 3);
       float g = h2rgb(v1, v2, hsl.x);
       float b = h2rgb(v1, v2, hsl.x - 1.0f / 3);
       return vec3f(r, g, b);
    }
}

mat3f dcolor(const vec3f &rgb, const vec3f &amp)
{
    vec3f hsl = rgb2hsl(rgb);
    vec3f RGB;
    mat3f m;
    RGB = hsl2rgb(hsl + vec3f(0.01f, 0, 0));
    m.setColumn(0, (RGB - rgb) / 0.01f * amp.x);
    RGB = hsl2rgb(hsl + vec3f(0, 0.01f, 0));
    m.setColumn(1, (RGB - rgb) / 0.01f * amp.y);
    RGB = hsl2rgb(hsl + vec3f(0, 0, 0.01f));
    m.setColumn(2, (RGB - rgb) / 0.01f * amp.z);
    return m;
}

}
