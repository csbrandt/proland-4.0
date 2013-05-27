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

const char *renderTreeSource = "\
#extension GL_EXT_gpu_shader4 : enable\n\
\n\
#include \"treeInfo3D.glsl\"\n\
\n\
uniform vec3 cameraRefPos; // xy: ref pos in horizontal plane, z: (1+(zFar+zNear)/(zFar-zNear))/2\n\
uniform vec4 clip[4];\n\
uniform mat4 localToTangentFrame;\n\
uniform mat4 tangentFrameToScreen;\n\
uniform mat4 tangentFrameToWorld;\n\
uniform mat3 tangentSpaceToWorld;\n\
uniform vec3 tangentSunDir;\n\
uniform vec3 focalPos; // cameraPos is (0,0,focalPos.z) in tangent frame\n\
uniform float plantRadius;\n\
\n\
uniform float pass;\n\
\n\
#ifdef _VERTEX_\n\
\n\
bool isVisible(vec4 c, float r) {\n\
    return dot(c, clip[0]) > - r && dot(c, clip[1]) > - r && dot(c, clip[2]) > - r && dot(c, clip[3]) > - r;\n\
}\n\
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
    float r = gParams.x * plantRadius;\n\
    generate = length(vec3(gPos.xy,gPos.z-focalPos.z)) < maxDist && isVisible(vec4(gPos + vec3(0.0, 0.0, r), 1.0), r) ? 1.0 : 0.0;\n\
}\n\
\n\
#endif\n\
\n\
#ifdef _GEOMETRY_\n\
\n\
#include \"globalsShader.glhl\"\n\
\n\
layout(points) in;\n\
layout(triangle_strip,max_vertices=4) out;\n\
\n\
in vec3 gPos[];\n\
in vec3 gParams[];\n\
in float generate[];\n\
\n\
flat out vec3 cPos; // camera position in local frame of cone (where cone center = 0,0,0, cone height = cone radius = 1)\n\
flat out vec4 lDIR;\n\
out vec4 cDir; // camera direction in local frame of cone (where cone center = 0,0,0, cone height = cone radius = 1)\n\
out vec3 tDir;\n\
flat out ivec3 vId;\n\
flat out vec3 vW;\n\
flat out ivec3 lId;\n\
flat out vec3 lW;\n\
\n\
void main()\n\
{\n\
    if (selectTree(gPos[0] + cameraRefPos, gParams[0].y) && generate[0] > 0.5 && pass < 2.0) {\n\
        vec3 WCP = getWorldCameraPos();\n\
        vec3 WSD = getWorldSunDir();\n\
\n\
        vec3 wPos = (tangentFrameToWorld * vec4(gPos[0], 1.0)).xyz; // world pos\n\
\n\
        float ca = gParams[0].y * 2.0 - 1.0;\n\
        float sa = sqrt(1.0 - ca * ca);\n\
        mat2 rotation = mat2(ca, sa, -sa, ca);\n\
\n\
        vec3 tSize = vec3(1.0, 1.0, 0.0) * gParams[0].x * plantRadius;\n\
\n\
        vec3 gDir = normalize(vec3(gPos[0].xy, 0.0));\n\
        vec3 gLeft = vec3(-gDir.y, gDir.x, 0.0);\n\
        vec3 gUp = vec3(0.0, 0.0, 1.0);\n\
        gLeft *= tSize.x;\n\
        gDir *= tSize.x;\n\
        gUp *= tSize.y;\n\
\n\
        vec3 O = gPos[0] + gUp * tSize.z / tSize.y + gUp; // TODO tree base expressed in function of tree radius or tree height, or tree half height? must be consistent with usage in brdf model\n\
        bool below = focalPos.z < O.z + gUp.z;\n\
        vec3 A = +gLeft - gDir - gUp;\n\
        vec3 B = -gLeft - gDir - gUp;\n\
        vec3 C = +gLeft + (below ? -gDir : gDir) + gUp;\n\
        vec3 D = -gLeft + (below ? -gDir : gDir) + gUp;\n\
        cPos = (vec3(0.0, 0.0, focalPos.z) - O) / tSize.xxy;\n\
\n\
        cPos = vec3(rotation * cPos.xy, cPos.z);\n\
        vec3 cDIR = normalize(cPos);\n\
        lDIR.xyz = normalize(vec3(rotation * tangentSunDir.xy, tangentSunDir.z) / tSize.xxy);\n\
        lDIR.w = length(vec3(rotation * tangentSunDir.xy, tangentSunDir.z) / tSize.xxy);\n\
\n\
        ivec3 vv;\n\
        vec3 rr;\n\
        findViews(cDIR, vv, rr);\n\
\n\
        findViews(lDIR.xyz, lId, lW);\n\
\n\
        vId = vv;\n\
        vW = rr;\n\
        gl_Position = tangentFrameToScreen * vec4(O + A, 1.0);\n\
        tDir = O + A - vec3(0.0, 0.0, focalPos.z);\n\
        cDir.xyz = vec3(rotation * A.xy, A.z) / tSize.xxy - cPos;\n\
        cDir.w = dot(cPos + cDir.xyz, cDIR);\n\
        EmitVertex();\n\
        gl_Position = tangentFrameToScreen * vec4(O + B, 1.0);\n\
        tDir = O + B - vec3(0.0, 0.0, focalPos.z);\n\
        cDir.xyz = vec3(rotation * B.xy, B.z) / tSize.xxy - cPos;\n\
        cDir.w = dot(cPos + cDir.xyz, cDIR);\n\
        EmitVertex();\n\
        gl_Position = tangentFrameToScreen * vec4(O + C, 1.0);\n\
        tDir = O + C - vec3(0.0, 0.0, focalPos.z);\n\
        cDir.xyz = vec3(rotation * C.xy, C.z) / tSize.xxy - cPos;\n\
        cDir.w = dot(cPos + cDir.xyz, cDIR);\n\
        EmitVertex();\n\
        gl_Position = tangentFrameToScreen * vec4(O + D, 1.0);\n\
        tDir = O + D - vec3(0.0, 0.0, focalPos.z);\n\
        cDir.xyz = vec3(rotation * D.xy, D.z) / tSize.xxy - cPos;\n\
        cDir.w = dot(cPos + cDir.xyz, cDIR);\n\
        EmitVertex();\n\
        EndPrimitive();\n\
    }\n\
}\n\
\n\
#endif\n\
\n\
#ifdef _FRAGMENT_\n\
\n\
uniform sampler2DArray treeSampler;\n\
\n\
uniform Views {\n\
    mat3 views[MAXNT];\n\
};\n\
\n\
flat in vec3 cPos;\n\
flat in vec4 lDIR;\n\
in vec4 cDir;\n\
in vec3 tDir;\n\
flat in ivec3 vId;\n\
flat in vec3 vW;\n\
flat in ivec3 lId;\n\
flat in vec3 lW;\n\
layout(location=0) out vec4 color;\n\
\n\
void main() {\n\
    vec3 p0 = cPos + cDir.xyz * (1.0 + cDir.w / length(cDir.xyz));\n\
    vec2 uv0 = (views[vId.x] * p0).xy * 0.5 + vec2(0.5);\n\
    vec2 uv1 = (views[vId.y] * p0).xy * 0.5 + vec2(0.5);\n\
    vec2 uv2 = (views[vId.z] * p0).xy * 0.5 + vec2(0.5);\n\
    vec4 c0 = texture(treeSampler, vec3(uv0, vId.x));\n\
    vec4 c1 = texture(treeSampler, vec3(uv1, vId.y));\n\
    vec4 c2 = texture(treeSampler, vec3(uv2, vId.z));\n\
\n\
    // linear blend\n\
    color = c0 * vW.x + c1 * vW.y + c2 * vW.z;\n\
    color.rgb /= (color.a == 0.0 ? 1.0 : color.a);\n\
\n\
    float t = (color.r * (2.0 * sqrt(2.0)) - sqrt(2.0)) - cDir.w;\n\
    t = 1.0 - t / length(cDir.xyz);\n\
    vec3 p = cPos + cDir.xyz * t;\n\
\n\
    // iterative refinement\n\
    uv0 = (views[vId.x] * p).xy * 0.5 + vec2(0.5);\n\
    uv1 = (views[vId.y] * p).xy * 0.5 + vec2(0.5);\n\
    uv2 = (views[vId.z] * p).xy * 0.5 + vec2(0.5);\n\
    c0 = texture(treeSampler, vec3(uv0, vId.x));\n\
    c1 = texture(treeSampler, vec3(uv1, vId.y));\n\
    c2 = texture(treeSampler, vec3(uv2, vId.z));\n\
    vec4 color2 = c0 * vW.x + c1 * vW.y + c2 * vW.z;\n\
    color2.rgb /= (color2.a == 0.0 ? 1.0 : color2.a);\n\
    t = (color2.r * (2.0 * sqrt(2.0)) - sqrt(2.0)) - cDir.w;\n\
    t = 1.0 - t / length(cDir.xyz);\n\
    p = cPos + cDir.xyz * t;\n\
\n\
    float fragZ = (gl_FragCoord.z - cameraRefPos.z) / t + cameraRefPos.z;\n\
    gl_FragDepth = fragZ;\n\
\n\
    if (color2.a > 0.0) {\n\
        uv0 = (views[lId.x] * p).xy * 0.5 + vec2(0.5);\n\
        uv1 = (views[lId.y] * p).xy * 0.5 + vec2(0.5);\n\
        uv2 = (views[lId.z] * p).xy * 0.5 + vec2(0.5);\n\
        c0 = texture(treeSampler, vec3(uv0, lId.x));\n\
        c1 = texture(treeSampler, vec3(uv1, lId.y));\n\
        c2 = texture(treeSampler, vec3(uv2, lId.z));\n\
        vec4 c = c0 * lW.x + c1 * lW.y + c2 * lW.z;\n\
        c.r /= (c.a == 0.0 ? 1.0 : c.a);\n\
        float d = (c.r * (2.0 * sqrt(2.0)) - sqrt(2.0)) - dot(p, lDIR.xyz);\n\
\n\
        float kc = exp(-getTreeTau() * max(d, 0.0));\n\
\n\
        vec3 P = vec3(0.0, 0.0, focalPos.z) + tDir * t; // position in tangent frame\n\
\n\
        int slice = -1;\n\
        slice += fragZ < getShadowLimit(0) ? 1 : 0;\n\
        slice += fragZ < getShadowLimit(1) ? 1 : 0;\n\
        slice += fragZ < getShadowLimit(2) ? 1 : 0;\n\
        slice += fragZ < getShadowLimit(3) ? 1 : 0;\n\
        if (slice >= 0) {\n\
            mat4 t2s = getTangentFrameToShadow(slice);\n\
            vec3 qs = (t2s * vec4(P, 1.0)).xyz * 0.5 + vec3(0.5);\n\
            float ts = shadow(vec4(qs.xy, slice, qs.z), t2s[3][3]);\n\
            kc *= ts;\n\
        }\n\
\n\
        float tz = 0.5 - p.z * 0.5;\n\
        float shear = 2.0 / getTreeHeight();\n\
        float skykc = color2.b / (1.0 + tz * tz * getTreeDensity() / (shear * shear));\n\
\n\
        color.rgb = vec3(1.0, pass == 0.0 ? kc : skykc, 0.0);\n\
    }\n\
}\n\
\n\
#endif\n\
";
