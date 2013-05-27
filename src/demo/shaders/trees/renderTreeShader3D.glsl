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

uniform vec3 cameraRefPos; // xy: ref pos in horizontal plane, z: (1+(zFar+zNear)/(zFar-zNear))/2
uniform vec4 clip[4];
uniform mat4 localToTangentFrame;
uniform mat4 tangentFrameToScreen;
uniform mat4 tangentFrameToWorld;
uniform mat3 tangentSpaceToWorld;
uniform vec3 tangentSunDir;
uniform vec3 focalPos; // cameraPos is (0,0,focalPos.z) in tangent frame
uniform float plantRadius;

#ifdef _VERTEX_

bool isVisible(vec4 c, float r) {
    return dot(c, clip[0]) > - r && dot(c, clip[1]) > - r && dot(c, clip[2]) > - r && dot(c, clip[3]) > - r;
    //return dot(c, -clip[0]+clip[1]) > - r && dot(c, clip[1]) > - r && dot(c, clip[2]) > - r && dot(c, clip[3]) > - r;
}

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

    float size = unpackUnorm4x8(floatBitsToUint(gParams.z)).w;
#ifdef MONO_SPECIES
    size = size * 4.0 - (size > 0.5 ? 2.0 : 0.0);
#else
    int species = size > 0.5 ? 1 : 0;
    size = size * 4.0 - species * 2.0;
#endif
    float r = getTreeHeight(species) * size * plantRadius * 0.5;

    generate = length(vec3(gPos.xy,gPos.z-focalPos.z)) < maxDist && isVisible(vec4(gPos + vec3(0.0, 0.0, r), 1.0), r) ? 1.0 : 0.0;
}

#endif

#ifdef _GEOMETRY_

#include "globalsShader.glhl"
#include "atmosphereShader.glhl"

layout(points) in;
layout(triangle_strip,max_vertices=4) out;

in vec3 gPos[];
in vec3 gParams[];
in float generate[];

flat out vec3 sunColor;
flat out vec3 skyColor;
flat out vec3 inColor;
flat out vec3 cPos; // camera position in local frame of cone (where cone center = 0,0,0, cone height = cone radius = 1)
flat out vec4 lDIR;
flat out vec4 nkcs; // limit proprotion of lit tree surface, blending factor, seed
out vec4 cDir; // camera direction in local frame of cone (where cone center = 0,0,0, cone height = cone radius = 1)
out vec3 tDir;

flat out ivec3 vId;
flat out vec3 vW;
flat out ivec3 lId;
flat out vec3 lW;

flat out vec2 fadingAndAO;

void main()
{
    if (generate[0] > 0.5) {
        vec3 WCP = getWorldCameraPos();
        vec3 WSD = getWorldSunDir();

        vec3 wPos = (tangentFrameToWorld * vec4(gPos[0], 1.0)).xyz; // world pos
        vec2 lN = unpackUnorm2x16(floatBitsToUint(gParams[0].x)) * 2.0 - vec2(1.0); // normal in tangent frame
        vec3 wN = (tangentFrameToWorld * vec4(lN, sqrt(max(0.0, 1.0 - dot(lN, lN))), 0.0)).xyz; // world normal

        vec4 apertureAndSeed = unpackUnorm4x8(floatBitsToUint(gParams[0].y));
        vec4 colorAndSize = unpackUnorm4x8(floatBitsToUint(gParams[0].z));

#ifdef MONO_SPECIES
        colorAndSize.w = colorAndSize.w * 4.0 - (colorAndSize.w > 0.5 ? 2.0 : 0.0);
#else
        int species = colorAndSize.w > 0.5 ? 1 : 0;
        colorAndSize.w = colorAndSize.w * 4.0 - species * 2.0;
#endif

        vec3 sunL; // sun radiance
        vec3 skyE; // sky irradiance
        vec3 extinction; // atmospheric extinction
        vec3 inscatter; // atmospheric inscattering
#ifdef SHADOWS
        float ambient = sqrt(sqrt(apertureAndSeed.x));
        vec2 bentn = tan(apertureAndSeed.yz * 2.9 - 1.45) / 8.0;
        vec4 ambientAperture = vec4(ambient, tangentSpaceToWorld * vec3(bentn, sqrt(1.0 - dot(bentn, bentn))));
        sunRadianceAndSkyIrradiance(wPos, wN, WSD, 1.0, ambientAperture, sunL, skyE);
#else
        sunRadianceAndSkyIrradiance(wPos, wN, WSD, sunL, skyE);
#endif
        inscatter = inScattering(WCP, wPos, WSD, extinction, 0.0);

        vec3 reflectance = species == 0 ? treeReflectance0 : treeReflectance1;

        sunColor = reflectance * colorAndSize.rgb * extinction * sunL / (4.0 * 3.14159265);
        skyColor = reflectance * colorAndSize.rgb * extinction * skyE / (2.0 * 3.14159265); // 2 or 4 pi?
        inColor = inscatter;

        float ca = apertureAndSeed.w * 2.0 - 1.0;
        float sa = sqrt(1.0 - ca * ca);
        mat2 rotation = mat2(ca, sa, -sa, ca);

        vec3 tSize = getTreeSize(species) * colorAndSize.w * plantRadius;

        vec3 gDir = normalize(vec3(gPos[0].xy, 0.0));
        vec3 gLeft = vec3(-gDir.y, gDir.x, 0.0);
        vec3 gUp = vec3(0.0, 0.0, 1.0);
        gLeft *= tSize.x;
        gDir *= tSize.x;
        gUp *= tSize.y;

        vec3 O = gPos[0] + gUp * tSize.z / tSize.y + gUp; // TODO tree base expressed in function of tree radius or tree height, or tree half height? must be consistent with usage in brdf model
        bool below = focalPos.z < O.z + gUp.z;
        vec3 A = +gLeft - gDir - gUp;
        vec3 B = -gLeft - gDir - gUp;
        vec3 C = +gLeft + (below ? -gDir : gDir) + gUp;
        vec3 D = -gLeft + (below ? -gDir : gDir) + gUp;
        cPos = (vec3(0.0, 0.0, focalPos.z) - O) / tSize.xxy;

        // computation of nkc
        vec3 tNorm = vec3(lN, sqrt(1.0 - dot(lN, lN)));
        float treeDensity = getTreeDensityFactor();
        float shear = 2.0 / getTreeHeight(species);
        vec3 v = O - vec3(0.0, 0.0, focalPos.z); // view vector in tangent space
        vec3 sv = -normalize(v * vec3(1.0, 1.0, shear));
        vec3 sn = normalize(tNorm * vec3(1.0, 1.0, 1.0 / shear));
        vec3 sl = normalize(tangentSunDir * vec3(1.0, 1.0, shear));
        treeDensity *= sn.z;
        float cThetaV = dot(sv, sn);
        float cThetaL = dot(sl, sn);
        float cPhi = clamp((dot(sv, sl) - cThetaV * cThetaL) / (sqrt(1.0 - cThetaV * cThetaV) * sqrt(1.0 - cThetaL * cThetaL)), -1.0, 1.0);
        nkcs.x = treeKc(species, treeDensity, acos(cThetaV), acos(cThetaL), acos(cPhi));
        nkcs.y = smoothstep(0.0, 0.8, length(vec3(gPos[0].xy,gPos[0].z-focalPos.z)) / getMaxTreeDistance());
        nkcs.z = apertureAndSeed.w;
        nkcs.w = treeAO(species, treeDensity, acos(cThetaV));

cPos = vec3(rotation * cPos.xy, cPos.z);
vec3 cDIR = normalize(cPos);
lDIR.xyz = normalize(vec3(rotation * tangentSunDir.xy, tangentSunDir.z) / tSize.xxy);
lDIR.w = length(vec3(rotation * tangentSunDir.xy, tangentSunDir.z) / tSize.xxy);

        float dist = length(vec3(gPos[0].xy,gPos[0].z-focalPos.z)) * (0.9 + 0.1 * apertureAndSeed.w) / getMaxTreeDistance();
        fadingAndAO.x = 1.0 - smoothstep(0.7, 1.0, dist);
        fadingAndAO.y = treeDensity / (shear * shear);

        ivec3 vv;
        vec3 rr;
        findViews(species, cDIR, vv, rr);

        findViews(species, lDIR.xyz, lId, lW);

        vId = vv;
        vW = rr;
        gl_Position = tangentFrameToScreen * vec4(O + A, 1.0);
        tDir = O + A - vec3(0.0, 0.0, focalPos.z);
        cDir.xyz = vec3(rotation * A.xy, A.z) / tSize.xxy - cPos;
        cDir.w = dot(cPos + cDir.xyz, cDIR);
        EmitVertex();
        gl_Position = tangentFrameToScreen * vec4(O + B, 1.0);
        tDir = O + B - vec3(0.0, 0.0, focalPos.z);
        cDir.xyz = vec3(rotation * B.xy, B.z) / tSize.xxy - cPos;
        cDir.w = dot(cPos + cDir.xyz, cDIR);
        EmitVertex();
        gl_Position = tangentFrameToScreen * vec4(O + C, 1.0);
        tDir = O + C - vec3(0.0, 0.0, focalPos.z);
        cDir.xyz = vec3(rotation * C.xy, C.z) / tSize.xxy - cPos;
        cDir.w = dot(cPos + cDir.xyz, cDIR);
        EmitVertex();
        gl_Position = tangentFrameToScreen * vec4(O + D, 1.0);
        tDir = O + D - vec3(0.0, 0.0, focalPos.z);
        cDir.xyz = vec3(rotation * D.xy, D.z) / tSize.xxy - cPos;
        cDir.w = dot(cPos + cDir.xyz, cDIR);
        EmitVertex();
        EndPrimitive();
    }
}

#endif

#ifdef _FRAGMENT_

uniform sampler2DArray treeSampler;
uniform isampler2D multiSampler;

uniform Views {
    mat3 views[NT];
};

flat in vec3 sunColor;
flat in vec3 skyColor;
flat in vec3 inColor;
flat in vec3 cPos;
flat in vec4 lDIR;
flat in vec4 nkcs;
in vec4 cDir;
in vec3 tDir;
flat in ivec3 vId;
flat in vec3 vW;
flat in ivec3 lId;
flat in vec3 lW;
layout(location=0) out vec4 color;

flat in vec2 fadingAndAO;

vec3 hdr(vec3 L);

void main() {
    vec3 p0 = cPos + cDir.xyz * (1.0 + cDir.w / length(cDir.xyz));
    vec2 uv0 = (views[vId.x%NT] * p0).xy * 0.5 + vec2(0.5);
    vec2 uv1 = (views[vId.y%NT] * p0).xy * 0.5 + vec2(0.5);
    vec2 uv2 = (views[vId.z%NT] * p0).xy * 0.5 + vec2(0.5);
    vec4 c0 = texture(treeSampler, vec3(uv0, vId.x));
    vec4 c1 = texture(treeSampler, vec3(uv1, vId.y));
    vec4 c2 = texture(treeSampler, vec3(uv2, vId.z));

    // linear blend
    color = c0 * vW.x + c1 * vW.y + c2 * vW.z;
    color.r /= (color.a == 0.0 ? 1.0 : color.a);

    float t = (color.r * (2.0 * sqrt(2.0)) - sqrt(2.0)) - cDir.w;
    t = 1.0 - t / length(cDir.xyz);
    vec3 p = cPos + cDir.xyz * t;

    gl_SampleMask[0] = texture(multiSampler, vec2(color.a * fadingAndAO.x, nkcs.z)).r;

    // iterative refinement
    uv0 = (views[vId.x%NT] * p).xy * 0.5 + vec2(0.5);
    uv1 = (views[vId.y%NT] * p).xy * 0.5 + vec2(0.5);
    uv2 = (views[vId.z%NT] * p).xy * 0.5 + vec2(0.5);
    c0 = texture(treeSampler, vec3(uv0, vId.x));
    c1 = texture(treeSampler, vec3(uv1, vId.y));
    c2 = texture(treeSampler, vec3(uv2, vId.z));
    color = c0 * vW.x + c1 * vW.y + c2 * vW.z;
    color.r /= (color.a == 0.0 ? 1.0 : color.a);
    t = (color.r * (2.0 * sqrt(2.0)) - sqrt(2.0)) - cDir.w;
    t = 1.0 - t / length(cDir.xyz);
    p = cPos + cDir.xyz * t;

    float fragZ = (gl_FragCoord.z - cameraRefPos.z) / t + cameraRefPos.z;
    gl_FragDepth = fragZ;

    if (color.a > 0.0) {
        uv0 = (views[lId.x%NT] * p).xy * 0.5 + vec2(0.5);
        uv1 = (views[lId.y%NT] * p).xy * 0.5 + vec2(0.5);
        uv2 = (views[lId.z%NT] * p).xy * 0.5 + vec2(0.5);
        c0 = texture(treeSampler, vec3(uv0, lId.x));
        c1 = texture(treeSampler, vec3(uv1, lId.y));
        c2 = texture(treeSampler, vec3(uv2, lId.z));
        vec4 c = c0 * lW.x + c1 * lW.y + c2 * lW.z;
        c.r /= (c.a == 0.0 ? 1.0 : c.a);
        float d = (c.r * (2.0 * sqrt(2.0)) - sqrt(2.0)) - dot(p, lDIR.xyz);

        float kc = exp(-10.0*max(d,0.0));

        float tz = 0.5 - p.z * 0.5;
        float skykc = color.b / (color.a == 0.0 ? 1.0 : color.a);
        skykc /= 1.0 + tz * tz * fadingAndAO.y; // (slope factor already taken into account in skyColor)

        vec3 P = vec3(0.0, 0.0, focalPos.z) + tDir * t; // position in tangent frame

        if (nkcs.y < 1.0) {
            int slice = -1;
            slice += fragZ < getShadowLimit(0) ? 1 : 0;
            slice += fragZ < getShadowLimit(1) ? 1 : 0;
            slice += fragZ < getShadowLimit(2) ? 1 : 0;
            slice += fragZ < getShadowLimit(3) ? 1 : 0;
            if (slice >= 0) {
                mat4 t2s = getTangentFrameToShadow(slice);
                vec3 qs = (t2s * vec4(P, 1.0)).xyz * 0.5 + vec3(0.5);
                float ts = shadow(vec4(qs.xy, slice, qs.z), t2s[3][3]);
                kc *= ts;
            }
        }
        kc = mix(kc, nkcs.x, nkcs.y);
        skykc = mix(skykc, nkcs.w, nkcs.y);
        float hs = inTreeHotspot(normalize(lDIR.xyz),normalize(cDir.xyz));

        color.rgb = hdr(sunColor * kc * hs + skyColor * skykc + inColor);
    }
}

#endif
