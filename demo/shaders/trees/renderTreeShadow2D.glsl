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

#extension GL_EXT_gpu_shader4 : enable

#include "globalsShader.glhl"

#define ALPHASHADOWMAP

uniform vec3 cameraRefPos;
uniform mat4 localToTangentFrame;
uniform mat4 tangentFrameToScreen;
uniform vec3 tangentSunDir; // OPPOSITE to sun direction
uniform vec3 focalPos; // cameraPos is (0,0,focalPos.z) in tangent frame
uniform float plantRadius;
uniform int shadowLayers;
uniform vec4 shadowCuts;

#ifdef _VERTEX_

layout(location=0) in vec3 lPos;
layout(location=1) in vec3 lParams;
out vec3 gPos;
out vec3 gParams;
out float generate;

void main()
{
    gPos = (localToTangentFrame * vec4(lPos - vec3(cameraRefPos.xy, 0.0), 1.0)).xyz;
    gParams = lParams;
    float seed = unpackUnorm4x8(floatBitsToUint(lParams.y)).w;
    float maxDist = getMaxTreeDistance() * (0.8 + 0.2 * seed);
    generate = length(vec3(gPos.xy,gPos.z-focalPos.z)) < maxDist ? 1.0 : 0.0;
}

#endif

#ifdef _GEOMETRY_

layout(points) in;
layout(triangle_strip,max_vertices=8) out;

in vec3 gPos[];
in vec3 gParams[];
in float generate[];

#ifndef MONO_SPECIES
flat out int species;
#endif
out vec2 uv;

int getSlice(float z)
{
    int slice = -1;
    slice += z < shadowCuts.x ? 1 : 0;
    slice += z < shadowCuts.y ? 1 : 0;
    slice += z < shadowCuts.z ? 1 : 0;
    slice += z < shadowCuts.w ? 1 : 0;
    return slice;
}
void main()
{
    if (generate[0] > 0.5) {

        vec4 colorAndSize = unpackUnorm4x8(floatBitsToUint(gParams[0].z));

#ifdef MONO_SPECIES
        colorAndSize.w = colorAndSize.w * 4.0 - (colorAndSize.w > 0.5 ? 2.0 : 0.0);
#else
        species = colorAndSize.w > 0.5 ? 1 : 0;
        colorAndSize.w = colorAndSize.w * 4.0 - species * 2.0;
#endif

        vec3 tSize = vec3(1.0, getTreeHeight(species), getTreeBase(species)) * colorAndSize.w * plantRadius;

        vec3 gDir = normalize(vec3(tangentSunDir.xy, 0.0));
        vec3 gLeft = vec3(-gDir.y, gDir.x, 0.0);
        vec3 gUp = normalize(cross(gDir, gLeft));
        gLeft *= tSize.x;
        gDir *= tSize.x;
        gUp *= tSize.y;

        vec3 O = gPos[0] + vec3(0.0, 0.0, tSize.z / tSize.y);
        vec3 A = +gLeft;
        vec3 B = -gLeft;
        vec3 C = +gLeft + gUp;
        vec3 D = -gLeft + gUp;
        vec3 S = O - (2.0 * tSize.y + tSize.z) / tangentSunDir.z * tangentSunDir;
        vec3 T = O + 2.0 * gUp;

        float zI = (tangentFrameToScreen * vec4(gPos[0], 1.0)).w;
        float zT = (tangentFrameToScreen * vec4(T, 1.0)).w;
        float zS = (tangentFrameToScreen * vec4(S, 1.0)).w;
        int slice1 = getSlice(max(zI + tSize.x, zT));
        int slice2 = getSlice(min(zI - tSize.x, zT));
        int sliceS = getSlice(zS);
        int sliceMin = clamp(min(slice1, sliceS), slice2 - 1, slice1 + 1);
        int sliceMax = clamp(max(slice2, sliceS), slice2 - 1, slice1 + 1);

        for (int slice = sliceMin; slice <= sliceMax; ++slice) {
            gl_Layer = slice;
            mat4 m = getTangentFrameToShadow(slice);

            gl_Position = vec4((m * vec4(O + A, 1.0)).xyz, 1.0);
            uv = vec2(0.0, 0.0);
            EmitVertex();
            gl_Position = vec4((m * vec4(O + B, 1.0)).xyz, 1.0);
            uv = vec2(1.0, 0.0);
            EmitVertex();
            gl_Position = vec4((m * vec4(O + C, 1.0)).xyz, 1.0);
            uv = vec2(0.0, 1.0);
            EmitVertex();
            gl_Position = vec4((m * vec4(O + D, 1.0)).xyz, 1.0);
            uv = vec2(1.0, 1.0);
            EmitVertex();
            EndPrimitive();
        }
    }
}

#endif

#ifdef _FRAGMENT_

uniform sampler2DArray treeSampler;

#ifndef MONO_SPECIES
flat in int species;
#endif
in vec2 uv;

#ifdef ALPHASHADOWMAP
out vec4 color;
#endif

void main() {
    float alpha = texture(treeSampler, vec3(uv, species)).a;
    if (alpha < 0.3) {
        discard;
    }
#ifdef ALPHASHADOWMAP
    color = vec4(gl_FragCoord.z, 1.0 - alpha, 1.0, 1.0);
#endif
}

#endif
