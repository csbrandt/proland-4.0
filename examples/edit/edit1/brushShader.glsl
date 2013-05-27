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

uniform vec4 offset; // vertex coord to uv between [-1 1] (excluding tile borders)
uniform vec4 stroke; // stroke center and radius
uniform vec4 strokeEnd;
uniform float brushElevation;

uniform struct {
    vec4 offset;
} deformation;

#ifdef _VERTEX_

layout(location=0) in vec4 vertex;
out vec3 p;

void main() {
    gl_Position = vec4(vertex.xy, 0.0, 1.0);
    vec2 uv = vertex.xy * offset.xy + offset.zw;
    p = vec3(uv * deformation.offset.z + deformation.offset.xy, 0.0);
}

#endif

#ifdef _FRAGMENT_

in vec3 p;
layout(location=0) out vec4 data;

float segDist(vec2 a, vec2 b, vec2 p) {
    vec2 ab = b - a;
    vec2 ap = p - a;
    vec2 q = a + clamp(max(dot(ap, ab) / dot(ab, ab), 0.0), 0.0, 1.0) * ab;
    return length(p - q);
}

void main() {
    float d = segDist(stroke.xy, strokeEnd.xy, p.xy) / stroke.w;
    float value = 1.0 - smoothstep(0.0, 1.0, d);
    data = vec4(0.0, 0.0, 0.0, 0.5 * value * stroke.w * brushElevation);
}

#endif
