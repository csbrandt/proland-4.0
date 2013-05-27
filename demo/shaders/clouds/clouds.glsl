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

#include "clouds.glhl"
#include "textureQuadtree.glsl"

uniform samplerTile cloudsSampler;

float cloudsShadow(vec3 P, vec3 WSD, float h, float vSun, float radius)
{
    float result = 1.0;
#ifdef CLOUDS_SHADOWS
    if (h < CLOUD_HEIGHT && vSun > 0.001) {
        vec3 xyp;
        float b = vSun * (radius + h);
        float c = (h - CLOUD_HEIGHT) * (2.0 * radius + h + CLOUD_HEIGHT);
        vec3 pc = P + WSD * (sqrt(b*b - c) - b);

        float maxpc = max(abs(pc.x), max(abs(pc.y), abs(pc.z)));
        if (maxpc == pc.z) {
            xyp = vec3(vec2(pc.x / pc.z, pc.y / pc.z) * (radius + CLOUD_HEIGHT), 0.0);
        } else if (maxpc == -pc.y) {
            xyp = vec3(-vec2(pc.x / pc.y, pc.z / pc.y) * (radius + CLOUD_HEIGHT), 1.0);
        } else if (maxpc == pc.x) {
            xyp = vec3(vec2(pc.y / pc.x, pc.z / pc.x) * (radius + CLOUD_HEIGHT), 2.0);
        } else if (maxpc == pc.y) {
            xyp = vec3(vec2(-pc.x / pc.y, pc.z / pc.y) * (radius + CLOUD_HEIGHT), 3.0);
        } else if (maxpc == -pc.x) {
            xyp = vec3(vec2(pc.y / pc.x, -pc.z / pc.x) * (radius + CLOUD_HEIGHT), 4.0);
        } else {
            xyp = vec3(vec2(-pc.x / pc.z, pc.y / pc.z) * (radius + CLOUD_HEIGHT), 5.0);
        }
        result = textureQuadtree(cloudsSampler, xyp.xy, xyp.z).r;
        result = max((result - 0.1) / 0.9, 0.0);
        result = max(1.0 - result * 5.0, 0.1);
    }
#endif
    return result;
}

#ifdef _FRAGMENT_
#endif
