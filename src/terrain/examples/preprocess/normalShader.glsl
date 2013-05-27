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

uniform vec3 tileSDF; // size in pixels of one tile, tileSize / grid mesh size for display, output format

uniform sampler2DArray elevationSampler; // elevation texture
uniform vec4 elevationOSL; // lower left corner of tile containing patch elevation, one over size in pixels of tiles texture, layer id

uniform mat4 patchCorners;
uniform mat4 patchVerticals;
uniform vec4 patchCornerNorms;
uniform mat3 worldToTangentFrame;
uniform vec4 deform;

#ifdef _VERTEX_

layout(location=0) in vec4 vertex;
out vec2 st;

void main() {
    st = (vertex.xy * 0.5 + 0.5) * tileSDF.x;
    gl_Position = vertex;
}

#endif

#ifdef _FRAGMENT_

in vec2 st;
layout(location=0) out vec4 data;

vec3 getWorldPosition(vec2 uv, float h) {
    uv = uv / (tileSDF.x - 1.0);

    float R = deform.w;
    mat4 C = patchCorners;
    mat4 N = patchVerticals;
    vec4 L = patchCornerNorms;

    vec4 uvUV = vec4(uv, vec2(1.0) - uv);
    vec4 alpha = uvUV.zxzx * uvUV.wwyy;
    vec4 alphaPrime = alpha * L / dot(alpha, L);

    vec4 up = N * alphaPrime;
    float k = mix(length(up.xyz), 1.0, smoothstep(R / 32.0, R / 64.0, deform.z));
    float hPrime = (h + R * (1.0 - k)) / k;

    return (C * alphaPrime + hPrime * up).xyz;
}

void main() {
    vec2 p_uv = floor(st);

    vec4 uv0 = floor(p_uv.xyxy + vec4(-1.0,0.0,1.0,0.0)) * elevationOSL.z + elevationOSL.xyxy;
    vec4 uv1 = floor(p_uv.xyxy + vec4(0.0,-1.0,0.0,1.0)) * elevationOSL.z + elevationOSL.xyxy;
    float z0 = textureLod(elevationSampler, vec3(uv0.xy, elevationOSL.w), 0.0).z;
    float z1 = textureLod(elevationSampler, vec3(uv0.zw, elevationOSL.w), 0.0).z;
    float z2 = textureLod(elevationSampler, vec3(uv1.xy, elevationOSL.w), 0.0).z;
    float z3 = textureLod(elevationSampler, vec3(uv1.zw, elevationOSL.w), 0.0).z;

    vec3 p0 = getWorldPosition(p_uv + vec2(-1.0, 0.0), z0).xyz;
    vec3 p1 = getWorldPosition(p_uv + vec2(+1.0, 0.0), z1).xyz;
    vec3 p2 = getWorldPosition(p_uv + vec2(0.0, -1.0), z2).xyz;
    vec3 p3 = getWorldPosition(p_uv + vec2(0.0, +1.0), z3).xyz;
    vec2 nf = (worldToTangentFrame * normalize(cross(p1 - p0, p3 - p2))).xy;

    data = vec4(nf.xy * 0.5, 0.0, 0.0) + vec4(0.5);
}

#endif
