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
    float maxDist = getMaxTreeDistance() * (0.8 + 0.2 * seed);

    float size = unpackUnorm4x8(floatBitsToUint(gParams.z)).w;
#ifdef MONO_SPECIES
    size = size.w * 4.0 - (size.w > 0.5 ? 2.0 : 0.0);
#else
    int species = size > 0.5 ? 1 : 0;
    size = size * 4.0 - species * 2.0;
#endif
    float r = getTreeHeight(species) * size * plantRadius * 0.5;

    generate = length(vec3(gPos.xy,gPos.z-focalPos.z)) < maxDist && isVisible(vec4(gPos + vec3(0.0, 0.0, r), 1.0), 2.0 * r) ? 1.0 : 0.0;
}

#endif

#ifdef _GEOMETRY_

#include "atmosphereShader.glhl"

layout(points) in;
layout(triangle_strip,max_vertices=4) out;

in vec3 gPos[];
in vec3 gParams[];
in float generate[];

out vec2 uv;
out vec3 lightColor;
out vec3 inColor;
#ifndef MONO_SPECIES
flat out int species;
#endif

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

#ifdef MONO_SPECIES
        colorAndSize.w = colorAndSize.w * 4.0 - (colorAndSize.w > 0.5 ? 2.0 : 0.0);
#else
        species = colorAndSize.w > 0.5 ? 1 : 0;
        colorAndSize.w = colorAndSize.w * 4.0 - species * 2.0;
#endif

        vec3 C = vec3(0.5, 1.0, 0.5) * 0.2;
        lightColor = C * colorAndSize.rgb * extinction * (sunL * max(dot(wN, WSD), 0.0) + skyE) / 3.14159265;
        inColor = inscatter;

        vec3 gDir1 = normalize(gPos[0] - vec3(0.0, 0.0, focalPos.z - 30.0));
        vec3 gDir2 = normalize(gPos[0] - focalPos);
        vec3 gDir = vec3(gDir2.xy, clamp(gDir1.z, -0.7, 0.0));

        vec3 tSize = vec3(1.0, getTreeHeight(species), getTreeBase(species)) * colorAndSize.w * plantRadius;

        vec3 gLeft = normalize(cross(vec3(0.0, 0.0, 1.0), gDir));
        vec3 gUp = normalize(cross(gDir, gLeft));
        gLeft *= tSize.x;
        gUp *= tSize.y;
        vec3 gBottom = gPos[0] + vec3(0.0, 0.0, tSize.z / tSize.y);
        gl_Position = tangentFrameToScreen * vec4(gBottom + gLeft, 1.0);
        uv = vec2(0.0, 0.0);
        EmitVertex();
        gl_Position = tangentFrameToScreen * vec4(gBottom - gLeft, 1.0);
        uv = vec2(1.0, 0.0);
        EmitVertex();
        gl_Position = tangentFrameToScreen * vec4(gBottom + gLeft + gUp, 1.0);
        uv = vec2(0.0, 1.0);
        EmitVertex();
        gl_Position = tangentFrameToScreen * vec4(gBottom - gLeft + gUp, 1.0);
        uv = vec2(1.0, 1.0);
        EmitVertex();
        EndPrimitive();
    }
}

#endif

#ifdef _FRAGMENT_

uniform sampler2DArray treeSampler;

in vec2 uv;
in vec3 lightColor;
in vec3 inColor;
#ifndef MONO_SPECIES
flat in int species;
#endif
layout(location=0) out vec4 color;

vec3 hdr(vec3 L);

void main() {
    vec4 reflectance = texture(treeSampler, vec3(uv, species));
    color.rgb = hdr(reflectance.rgb * uv.y * uv.y * lightColor + inColor);
    if (reflectance.a < 0.3) {
        discard;
    }
}

#endif
