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

const char *renderTreeShadowSource = "\
#extension GL_EXT_gpu_shader4 : enable\n\
\n\
#include \"treeInfo3D.glsl\"\n\
\n\
uniform vec3 cameraRefPos;\n\
uniform mat4 localToTangentFrame;\n\
uniform mat4 tangentFrameToScreen;\n\
uniform vec3 tangentSunDir; // OPPOSITE to sun direction\n\
uniform vec3 focalPos; // cameraPos is (0,0,focalPos.z) in tangent frame\n\
uniform float plantRadius;\n\
uniform int shadowLayers;\n\
uniform vec4 shadowCuts;\n\
\n\
#ifdef _VERTEX_\n\
\n\
layout(location=0) in vec3 lPos;\n\
layout(location=1) in vec3 lParams;\n\
out vec3 gPos;\n\
out vec3 gParams;\n\
out float generate;\n\
\n\
void main()\n\
{\n\
    gPos = (localToTangentFrame * vec4(lPos - vec3(cameraRefPos.xy, 0.0), 1.0)).xyz;\n\
    gParams = lParams;\n\
    if (getTreeDensity() == 0.0) gParams.x = 1.0;\n\
    float maxDist = getMaxTreeDistance();\n\
    generate = length(vec3(gPos.xy,gPos.z-focalPos.z)) < maxDist ? 1.0 : 0.0;\n\
}\n\
\n\
#endif\n\
\n\
#ifdef _GEOMETRY_\n\
\n\
layout(points) in;\n\
layout(triangle_strip,max_vertices=8) out;\n\
\n\
in vec3 gPos[];\n\
in vec3 gParams[];\n\
in float generate[];\n\
\n\
flat out float zscale;\n\
out vec3 sPos;\n\
out float lDist;\n\
\n\
flat out ivec3 vId;\n\
flat out vec3 vW;\n\
\n\
int getSlice(float z)\n\
{\n\
    int slice = -1;\n\
    slice += z < shadowCuts.x ? 1 : 0;\n\
    slice += z < shadowCuts.y ? 1 : 0;\n\
    slice += z < shadowCuts.z ? 1 : 0;\n\
    slice += z < shadowCuts.w ? 1 : 0;\n\
    return slice;\n\
}\n\
\n\
void main()\n\
{\n\
    if (selectTree(gPos[0] + cameraRefPos, gParams[0].y) && generate[0] > 0.5) {\n\
\n\
        float ca = gParams[0].y * 2.0 - 1.0;\n\
        float sa = sqrt(1.0 - ca * ca);\n\
        mat2 rotation = mat2(ca, sa, -sa, ca);\n\
\n\
        vec3 tSize = vec3(1.0, 1.0, 0.0) * gParams[0].x * plantRadius;\n\
\n\
        vec3 gDir = normalize(vec3(tangentSunDir.xy, 0.0));\n\
        vec3 gLeft = vec3(-gDir.y, gDir.x, 0.0);\n\
        vec3 gUp = vec3(0.0, 0.0, 1.0);\n\
        gLeft *= tSize.x;\n\
        gDir *= tSize.x;\n\
        gUp *= tSize.y;\n\
\n\
        vec3 O = gPos[0] + gUp * tSize.z / tSize.y + gUp; // TODO tree base expressed in function of tree radius or tree height, or tree half height? must be consistent with usage in brdf model\n\
        vec3 A = +gLeft - gDir - gUp;\n\
        vec3 B = -gLeft - gDir - gUp;\n\
        vec3 C = +gLeft + gDir + gUp;\n\
        vec3 D = -gLeft + gDir + gUp;\n\
\n\
        findViews(normalize(-vec3(rotation * tangentSunDir.xy, tangentSunDir.z) / tSize.xxy), vId, vW);\n\
\n\
        vec3 lDir = normalize(vec3(rotation * tangentSunDir.xy, tangentSunDir.z) / tSize.xxy);\n\
\n\
        vec3 T = O + gUp;\n\
        vec3 S = gPos[0] + gDir * (tSize.x+(2.0*tSize.y + tSize.z) * sqrt(1.0-tangentSunDir.z*tangentSunDir.z) / -tangentSunDir.z);\n\
\n\
        float zI = (tangentFrameToScreen * vec4(gPos[0], 1.0)).w;\n\
        float zT = (tangentFrameToScreen * vec4(T, 1.0)).w;\n\
        float zS = (tangentFrameToScreen * vec4(S, 1.0)).w;\n\
        int slice1 = getSlice(max(zI + tSize.x, zT));\n\
        int slice2 = getSlice(min(zI - tSize.x, zT));\n\
        int sliceS = getSlice(zS);\n\
        int sliceMin = clamp(min(slice1, sliceS), slice2 - 1, slice1 + 1);\n\
        int sliceMax = clamp(max(slice2, sliceS), slice2 - 1, slice1 + 1);\n\
\n\
        vec3 AA = vec3(rotation * A.xy, A.z) / tSize.xxy;\n\
        vec3 BB = vec3(rotation * B.xy, B.z) / tSize.xxy;\n\
        vec3 CC = vec3(rotation * C.xy, C.z) / tSize.xxy;\n\
        vec3 DD = vec3(rotation * D.xy, D.z) / tSize.xxy;\n\
        float lA = dot(AA, lDir);\n\
        float lB = dot(BB, lDir);\n\
        float lC = dot(CC, lDir);\n\
        float lD = dot(DD, lDir);\n\
\n\
        for (int slice = sliceMin; slice <= sliceMax; ++slice) {\n\
            gl_Layer = slice;\n\
            mat4 m = getTangentFrameToShadow(slice);\n\
            zscale = m[3][3] / length(tangentSunDir / tSize.xxy);\n\
\n\
            gl_Position = vec4((m * vec4(O + A, 1.0)).xyz, 1.0);\n\
            sPos = AA;\n\
            lDist = lA;\n\
            EmitVertex();\n\
            gl_Position = vec4((m * vec4(O + B, 1.0)).xyz, 1.0);\n\
            sPos = BB;\n\
            lDist = lB;\n\
            EmitVertex();\n\
            gl_Position = vec4((m * vec4(O + C, 1.0)).xyz, 1.0);\n\
            sPos = CC;\n\
            lDist = lC;\n\
            EmitVertex();\n\
            gl_Position = vec4((m * vec4(O + D, 1.0)).xyz, 1.0);\n\
            sPos = DD;\n\
            lDist = lD;\n\
            EmitVertex();\n\
            EndPrimitive();\n\
        }\n\
    }\n\
}\n\
\n\
#endif\n\
\n\
#ifdef _FRAGMENT_\n\
\n\
uniform Views {\n\
    mat3 views[MAXNT];\n\
};\n\
\n\
flat in float zscale;\n\
in vec3 sPos;\n\
in float lDist;\n\
\n\
uniform sampler2DArray treeSampler;\n\
\n\
flat in ivec3 vId;\n\
flat in vec3 vW;\n\
\n\
out vec4 color;\n\
\n\
void main() {\n\
    vec2 uv0 = (views[vId.x] * sPos).xy * 0.5 + vec2(0.5);\n\
    vec2 uv1 = (views[vId.y] * sPos).xy * 0.5 + vec2(0.5);\n\
    vec2 uv2 = (views[vId.z] * sPos).xy * 0.5 + vec2(0.5);\n\
\n\
    vec4 c0 = texture(treeSampler, vec3(uv0, vId.x));\n\
    vec4 c1 = texture(treeSampler, vec3(uv1, vId.y));\n\
    vec4 c2 = texture(treeSampler, vec3(uv2, vId.z));\n\
\n\
    // linear blend\n\
    float a = c0.a * vW.x + c1.a * vW.y + c2.a * vW.z;\n\
\n\
    if (a < 0.3) discard;\n\
\n\
    float r = (c0.g * vW.x + c1.g * vW.y + c2.g * vW.z) / a;\n\
    float t = (sqrt(2.0) - r * (2.0 * sqrt(2.0))) - lDist;\n\
\n\
    color = vec4(gl_FragCoord.z + t * zscale, 1.0 - a, 1.0, 1.0);\n\
}\n\
\n\
#endif\n\
";
