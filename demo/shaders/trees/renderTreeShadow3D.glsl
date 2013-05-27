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

#include "treeInfo3D.glsl"

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
    float maxDist = getMaxTreeDistance();// * (0.8 + 0.2 * seed);
    generate = length(vec3(gPos.xy,gPos.z-focalPos.z)) < maxDist ? 1.0 : 0.0;
}

#endif

#ifdef _GEOMETRY_

layout(points) in;
layout(triangle_strip,max_vertices=8) out;
//layout(triangle_strip,max_vertices=16) out;

in vec3 gPos[];
in vec3 gParams[];
in float generate[];

#ifndef MONO_SPECIES
flat out int species;
#endif
flat out float zscale;
//flat out vec3 sDir;
out vec3 sPos;
out float lDist;

flat out ivec3 vId;
flat out vec3 vW;

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

        vec4 apertureAndSeed = unpackUnorm4x8(floatBitsToUint(gParams[0].y));
        vec4 colorAndSize = unpackUnorm4x8(floatBitsToUint(gParams[0].z));

#ifdef MONO_SPECIES
        colorAndSize.w = colorAndSize.w * 4.0 - (colorAndSize.w > 0.5 ? 2.0 : 0.0);
#else
        species = colorAndSize.w > 0.5 ? 1 : 0;
        colorAndSize.w = colorAndSize.w * 4.0 - species * 2.0;
#endif

        float ca = apertureAndSeed.w * 2.0 - 1.0;
        float sa = sqrt(1.0 - ca * ca);
        mat2 rotation = mat2(ca, sa, -sa, ca);

        vec3 tSize = getTreeSize(species) * colorAndSize.w * plantRadius;

        vec3 gDir = normalize(vec3(tangentSunDir.xy, 0.0));
        vec3 gLeft = vec3(-gDir.y, gDir.x, 0.0);
        vec3 gUp = vec3(0.0, 0.0, 1.0);
        gLeft *= tSize.x;
        gDir *= tSize.x;
        gUp *= tSize.y;

        vec3 O = gPos[0] + gUp * tSize.z / tSize.y + gUp; // TODO tree base expressed in function of tree radius or tree height, or tree half height? must be consistent with usage in brdf model
        vec3 A = +gLeft - gDir - gUp;
        vec3 B = -gLeft - gDir - gUp;
        vec3 C = +gLeft + gDir + gUp;
        vec3 D = -gLeft + gDir + gUp;

        findViews(species, normalize(-vec3(rotation * tangentSunDir.xy, tangentSunDir.z) / tSize.xxy), vId, vW);

        //sDir = VDIR;
        vec3 lDir = normalize(vec3(rotation * tangentSunDir.xy, tangentSunDir.z) / tSize.xxy);

        vec3 T = O + gUp;
        vec3 S = gPos[0] + gDir * (tSize.x+(2.0*tSize.y + tSize.z) * sqrt(1.0-tangentSunDir.z*tangentSunDir.z) / -tangentSunDir.z);

        float zI = (tangentFrameToScreen * vec4(gPos[0], 1.0)).w;
        float zT = (tangentFrameToScreen * vec4(T, 1.0)).w;
        float zS = (tangentFrameToScreen * vec4(S, 1.0)).w;
        int slice1 = getSlice(max(zI + tSize.x, zT));
        int slice2 = getSlice(min(zI - tSize.x, zT));
        int sliceS = getSlice(zS);
        int sliceMin = clamp(min(slice1, sliceS), slice2 - 1, slice1 + 1);
        int sliceMax = clamp(max(slice2, sliceS), slice2 - 1, slice1 + 1);

        vec3 AA = vec3(rotation * A.xy, A.z) / tSize.xxy;
        vec3 BB = vec3(rotation * B.xy, B.z) / tSize.xxy;
        vec3 CC = vec3(rotation * C.xy, C.z) / tSize.xxy;
        vec3 DD = vec3(rotation * D.xy, D.z) / tSize.xxy;
        float lA = dot(AA, lDir);
        float lB = dot(BB, lDir);
        float lC = dot(CC, lDir);
        float lD = dot(DD, lDir);

        for (int slice = sliceMin; slice <= sliceMax; ++slice) {
            gl_Layer = slice;
            mat4 m = getTangentFrameToShadow(slice);
            zscale = m[3][3] / length(tangentSunDir / tSize.xxy);

            gl_Position = vec4((m * vec4(O + A, 1.0)).xyz, 1.0);
            sPos = AA;
            lDist = lA;
            EmitVertex();
            gl_Position = vec4((m * vec4(O + B, 1.0)).xyz, 1.0);
            sPos = BB;
            lDist = lB;
            EmitVertex();
            gl_Position = vec4((m * vec4(O + C, 1.0)).xyz, 1.0);
            sPos = CC;
            lDist = lC;
            EmitVertex();
            gl_Position = vec4((m * vec4(O + D, 1.0)).xyz, 1.0);
            sPos = DD;
            lDist = lD;
            EmitVertex();
            EndPrimitive();
        }
    }
}

#endif

#ifdef _FRAGMENT_

uniform Views {
    mat3 views[NT];
};

#ifndef MONO_SPECIES
flat in int species;
#endif
flat in float zscale;
//flat in vec3 sDir;
in vec3 sPos;
in float lDist;

uniform sampler2DArray treeSampler;

flat in ivec3 vId;
flat in vec3 vW;

#ifdef ALPHASHADOWMAP
out vec4 color;
#endif

void main() {
    /*vec3 cPos[3];
    vec3 cDir[3];
    cPos[0] = views[vId.x%NT] * sPos;
    cPos[1] = views[vId.y%NT] * sPos;
    cPos[2] = views[vId.z%NT] * sPos;
    cDir[0] = views[vId.x%NT] * sDir;
    cDir[1] = views[vId.y%NT] * sDir;
    cDir[2] = views[vId.z%NT] * sDir;

    float t0 = -cPos[0].z / cDir[0].z;
    float t1 = -cPos[1].z / cDir[1].z;
    float t2 = -cPos[2].z / cDir[2].z;
    vec2 uv0 = (cPos[0].xy + cDir[0].xy * t0) * 0.5 + vec2(0.5);
    vec2 uv1 = (cPos[1].xy + cDir[1].xy * t1) * 0.5 + vec2(0.5);
    vec2 uv2 = (cPos[2].xy + cDir[2].xy * t2) * 0.5 + vec2(0.5);*/

    vec2 uv0 = (views[vId.x%NT] * sPos).xy * 0.5 + vec2(0.5);
    vec2 uv1 = (views[vId.y%NT] * sPos).xy * 0.5 + vec2(0.5);
    vec2 uv2 = (views[vId.z%NT] * sPos).xy * 0.5 + vec2(0.5);

    vec4 c0 = texture(treeSampler, vec3(uv0, vId.x));
    vec4 c1 = texture(treeSampler, vec3(uv1, vId.y));
    vec4 c2 = texture(treeSampler, vec3(uv2, vId.z));

    // linear blend
    float a = c0.a * vW.x + c1.a * vW.y + c2.a * vW.z;

    if (a < 0.3) discard;

    float r = (c0.g * vW.x + c1.g * vW.y + c2.g * vW.z) / a;
    float t = (sqrt(2.0) - r * (2.0 * sqrt(2.0))) - lDist;

#ifdef ALPHASHADOWMAP
    color = vec4(gl_FragCoord.z + t * zscale, 1.0 - a, 1.0, 1.0);
#else
    gl_FragDepth = gl_FragCoord.z + t * zscale;
#endif
}

#endif
