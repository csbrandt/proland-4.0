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

#include "textureTile.glsl"

uniform samplerTile orthoSampler;

uniform struct {
    vec4 offset;
    vec4 camera;
    vec2 blending;
    mat4 localToScreen;
} deformation;

#ifdef _VERTEX_

layout(location=0) in vec3 vertex;
out vec3 p;
out vec3 n;
out vec2 uv;

void main() {
    vec4 zfc = vec4(0.0);
    vec4 nfc = vec4(0.0);

    vec2 v = abs(deformation.camera.xy - vertex.xy);
    float d = max(max(v.x, v.y), deformation.camera.z);
    float blend = clamp((d - deformation.blending.x) / deformation.blending.y, 0.0, 1.0);

    float h = zfc.z * (1.0 - blend) + zfc.y * blend;
    p = vec3(vertex.xy * deformation.offset.z + deformation.offset.xy, h);

    vec3 nf = vec3(nfc.xy, sqrt(1.0 - dot(nfc.xy, nfc.xy)));
    vec3 nc = vec3(nfc.zw, sqrt(1.0 - dot(nfc.zw, nfc.zw)));
    n = nf * (1.0 - blend) + nc * blend;

    gl_Position = deformation.localToScreen * vec4(p, 1.0);

    uv = vertex.xy;
}

#endif

#ifdef _FRAGMENT_

in vec3 p;
in vec3 n;
in vec2 uv;
layout(location=0) out vec4 color;

void main() {
    vec3 fn = normalize(n);

    color = textureTile(orthoSampler, uv);

    color.r += mod(dot(floor(deformation.offset.xy / deformation.offset.z + 0.5), vec2(1.0)), 2.0);
}

#endif
