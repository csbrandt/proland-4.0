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

#include "globalsShader.glhl"
#include "textureTile.glsl"

const float Z0 = 1.0;

uniform float radius;
uniform mat4 cameraToOcean;
uniform mat4 screenToCamera;
uniform mat4 cameraToScreen;
uniform mat3 oceanToCamera;
uniform mat4 oceanToWorld;
uniform vec3 oceanCameraPos;
uniform vec3 oceanSunDir;
uniform vec3 horizon1;
uniform vec3 horizon2;

uniform float nbWaves; // number of waves
uniform sampler1D wavesSampler; // waves parameters (h, omega, kx, ky) in wind space
uniform float heightOffset; // so that surface height is centered around z = 0
uniform float time; // current time

// grid cell size in pixels, angle under which a grid cell is seen,
// and parameters of the geometric series used for wavelengths
uniform vec4 lods;

#define NYQUIST_MIN 0.5 // wavelengths below NYQUIST_MIN * sampling period are fully attenuated
#define NYQUIST_MAX 1.25 // wavelengths above NYQUIST_MAX * sampling period are not attenuated at all

const float g = 9.81;
const float PI = 3.141592657;

#ifdef _VERTEX_

layout(location=0) in vec3 vertex;
out float oceanLod;
out vec2 oceanUv; // coordinates in wind space used to compute P(u)
out vec3 oceanP; // wave point P(u) in ocean space
out vec3 oceanDPdu; // dPdu in wind space, used to compute N
out vec3 oceanDPdv; // dPdv in wind space, used to compute N
out float oceanRoughness; // variance of unresolved waves in wind space

vec2 oceanPos(vec3 vertex, out float t, out vec3 cameraDir, out vec3 oceanDir) {
    float horizon = horizon1.x + horizon1.y * vertex.x - sqrt(horizon2.x + (horizon2.y + horizon2.z * vertex.x) * vertex.x);
    cameraDir = normalize((screenToCamera * vec4(vertex.x, min(vertex.y, horizon), 0.0, 1.0)).xyz);
    oceanDir = (cameraToOcean * vec4(cameraDir, 0.0)).xyz;
    float cz = oceanCameraPos.z;
    float dz = oceanDir.z;
    if (radius == 0.0) {
        t = (heightOffset + Z0 - cz) / dz;
    } else {
        float b = dz * (cz + radius);
        float c = cz * (cz + 2.0 * radius);
        float tSphere = - b - sqrt(max(b * b - c, 0.0));
        float tApprox = - cz / dz * (1.0 + cz / (2.0 * radius) * (1.0 - dz * dz));
        t = abs((tApprox - tSphere) * dz) < 1.0 ? tApprox : tSphere;
    }
    return oceanCameraPos.xy + t * oceanDir.xy;
}

vec2 oceanPos(vec3 vertex) {
    float t;
    vec3 cameraDir;
    vec3 oceanDir;
    return oceanPos(vertex, t, cameraDir, oceanDir);
}

void main() {
    float t;
    vec3 cameraDir;
    vec3 oceanDir;
    vec2 uv = oceanPos(vertex, t, cameraDir, oceanDir);
    float lod = - t / oceanDir.z * lods.y; // size in meters of one grid cell, projected on the sea surface
    vec2 duv = oceanPos(vertex + vec3(0.0, 0.01, 0.0)) - uv;
    vec3 dP = vec3(0.0, 0.0, heightOffset + (radius > 0.0 ? 0.0 : Z0));
    vec3 dPdu = vec3(1.0, 0.0, 0.0);
    vec3 dPdv = vec3(0.0, 1.0, 0.0);
    float roughness = getSeaRoughness();

    if (duv.x != 0.0 || duv.y != 0.0) {
        float iMin = max(floor((log2(NYQUIST_MIN * lod) - lods.z) * lods.w), 0.0);
        for (float i = iMin; i < nbWaves; ++i) {
            vec4 wt = textureLod(wavesSampler, (i + 0.5) / nbWaves, 0.0);
            float phase = wt.y * time - dot(wt.zw, uv);
            float s = sin(phase);
            float c = cos(phase);
            float overk = g / (wt.y * wt.y);

            float wm = smoothstep(NYQUIST_MIN, NYQUIST_MAX, (2.0 * PI) * overk / lod);
            vec3 factor = wm * wt.x * vec3(wt.zw * overk, 1.0);

            dP += factor * vec3(s, s, c);

            vec3 dPd = factor * vec3(c, c, -s);
            dPdu -= dPd * wt.z;
            dPdv -= dPd * wt.w;

            wt.zw *= overk;
            float kh = wt.x / overk;
            roughness -= wt.z * wt.z * (1.0 - sqrt(1.0 - kh * kh));
        }
    }
    vec3 p = t * oceanDir + dP + vec3(0.0, 0.0, oceanCameraPos.z);
    if (radius > 0.0) {
        dPdu += vec3(0.0, 0.0, -p.x / (radius + p.z));
        dPdv += vec3(0.0, 0.0, -p.y / (radius + p.z));
    }
    gl_Position = cameraToScreen * vec4(t * cameraDir + oceanToCamera * dP, 1.0);
    oceanLod = lod;
    oceanUv = uv;
    oceanP = p;
    oceanDPdu = dPdu;
    oceanDPdv = dPdv;
    oceanRoughness = roughness;
}

#endif

#ifdef _FRAGMENT_

#include "atmosphereShader.glhl"
#include "oceanBrdf.glhl"
#include "clouds.glhl"

in float oceanLod;
in vec2 oceanUv;
in vec3 oceanP;
in vec3 oceanDPdu;
in vec3 oceanDPdv;
in float oceanRoughness;
layout(location=0) out vec4 data;

void main() {
    vec3 WSD = getWorldSunDir();
    vec3 WCP = getWorldCameraPos();
    float lod = oceanLod;
    vec2 uv = oceanUv;
    vec3 dPdu = oceanDPdu;
    vec3 dPdv = oceanDPdv;
    float roughness = oceanRoughness;

    float iMAX = min(ceil((log2(NYQUIST_MAX * lod) - lods.z) * lods.w), nbWaves - 1.0);
    float iMax = floor((log2(NYQUIST_MIN * lod) - lods.z) * lods.w);
    float iMin = max(0.0, floor((log2(NYQUIST_MIN * lod / lods.x) - lods.z) * lods.w));
    for (float i = iMin; i <= iMAX; i += 1.0) {
        vec4 wt = textureLod(wavesSampler, (i + 0.5) / nbWaves, 0.0);
        float phase = wt.y * time - dot(wt.zw, uv);
        float s = sin(phase);
        float c = cos(phase);
        float overk = g / (wt.y * wt.y);

        float wm = smoothstep(NYQUIST_MIN, NYQUIST_MAX, (2.0 * PI) * overk / lod);
        float wn = smoothstep(NYQUIST_MIN, NYQUIST_MAX, (2.0 * PI) * overk / lod * lods.x);
        vec3 factor = (1.0 - wm) * wn * wt.x * vec3(wt.zw * overk, 1.0);

        vec3 dPd = factor * vec3(c, c, -s);
        dPdu -= dPd * wt.z;
        dPdv -= dPd * wt.w;

        wt.zw *= overk;
        float kh = i < iMax ? wt.x / overk : 0.0;
        float wkh = (1.0 - wn) * kh;
        roughness -= wt.z * wt.z * (sqrt(1.0 - wkh * wkh) - sqrt(1.0 - kh * kh));
    }

    roughness = max(roughness, 1e-5);

    vec3 earthCamera = vec3(0.0, 0.0, oceanCameraPos.z + radius);
    vec3 earthP = radius > 0.0 ? normalize(oceanP + vec3(0.0, 0.0, radius)) * (radius + 10.0) : oceanP;

    vec3 oceanCamera = vec3(0.0, 0.0, oceanCameraPos.z);
    vec3 V = normalize(oceanCamera - oceanP);
    vec3 N = normalize(cross(dPdu, dPdv));
    if (dot(V, N) < 0.0) {
        N = reflect(N, V); // reflects backfacing normals
    }

    vec3 sunL;
    vec3 skyE;
    vec3 extinction;
    sunRadianceAndSkyIrradiance(earthP, N, oceanSunDir, sunL, skyE);

    vec3 worldP = (oceanToWorld * vec4(oceanP, 1.0)).xyz;
    sunL *= cloudsShadow(worldP, WSD, 0.0, dot(normalize(worldP), WSD), radius);

    vec3 surfaceColor = oceanRadiance(V, N, oceanSunDir, roughness, sunL, skyE);

    // aerial perspective
    vec3 inscatter = inScattering(earthCamera, earthP, oceanSunDir, extinction, 0.0);
    vec3 finalColor = surfaceColor * extinction + inscatter;

    data.rgb = hdr(finalColor);
    data.a = 1.0;
}

#endif
