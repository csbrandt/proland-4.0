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

/**
 * Real-time Realistic Ocean Lighting using Seamless Transitions from Geometry to BRDF
 * Copyright (c) 2009 INRIA
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * Author: Eric Bruneton
 */

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
uniform float seaRoughness; // total variance
uniform float time; // current time

// grid cell size in pixels, angle under which a grid cell is seen,
// and parameters of the geometric series used for wavelengths
uniform vec4 lods;

#define NYQUIST_MIN 0.5 // wavelengths below NYQUIST_MIN * sampling period are fully attenuated
#define NYQUIST_MAX 1.25 // wavelengths above NYQUIST_MAX * sampling period are not attenuated at all

uniform vec3 seaColor; // sea bottom color

const float PI = 3.141592657;
const float g = 9.81;

#ifdef _VERTEX_

layout(location=0) in vec3 vertex;
out float oceanLod;
out vec2 oceanUv; // coordinates in wind space used to compute P(u)
out vec3 oceanP; // wave point P(u) in ocean space
out vec3 oceanDPdu; // dPdu in wind space, used to compute N
out vec3 oceanDPdv; // dPdv in wind space, used to compute N
out float oceanSigmaSq; // variance of unresolved waves in wind space

vec2 oceanPos(vec3 vertex, out float t, out vec3 cameraDir, out vec3 oceanDir) {
    float horizon = horizon1.x + horizon1.y * vertex.x - sqrt(horizon2.x + (horizon2.y + horizon2.z * vertex.x) * vertex.x);
    cameraDir = normalize((screenToCamera * vec4(vertex.x, min(vertex.y, horizon), 0.0, 1.0)).xyz);
    oceanDir = (cameraToOcean * vec4(cameraDir, 0.0)).xyz;
    float cz = oceanCameraPos.z;
    float dz = oceanDir.z;
    if (radius == 0.0) {
        t = (heightOffset + 5.0 - cz) / dz;
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
    vec3 dP = vec3(0.0, 0.0, heightOffset + (radius > 0.0 ? 0.0 : 5.0));
    vec3 dPdu = vec3(1.0, 0.0, 0.0);
    vec3 dPdv = vec3(0.0, 1.0, 0.0);
    float sigmaSq = seaRoughness;
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
            sigmaSq -= wt.z * wt.z * (1.0 - sqrt(1.0 - kh * kh));
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
    oceanSigmaSq = sigmaSq;
}

#endif

#ifdef _FRAGMENT_

#extension GL_EXT_gpu_shader4 : enable

#include "atmosphereShader.glhl"

in float oceanLod;
in vec2 oceanUv;
in vec3 oceanP;
in vec3 oceanDPdu;
in vec3 oceanDPdv;
in float oceanSigmaSq;
layout(location=0) out vec4 data;

vec3 getWorldCameraPos();
vec3 getWorldSunDir();
vec3 hdr(vec3 L);

void sunRadianceAndSkyIrradiance(vec3 worldP, vec3 worldN, vec3 worldS, out vec3 sunL, out vec3 skyE)
{
    float r = length(worldP);
    if (r < 0.9 * PLANET_RADIUS) {
        worldP.z += PLANET_RADIUS;
        r = length(worldP);
    }
    vec3 worldV = worldP / r; // vertical vector
    float muS = dot(worldV, worldS);
    sunL = sunRadiance(r, muS);
    skyE = skyIrradiance(r, muS) * (1.0 + dot(worldV, worldN));
}

// ---------------------------------------------------------------------------
// REFLECTED SUN RADIANCE
// ---------------------------------------------------------------------------

// assumes x>0
float erfc(float x) {
    return 2.0 * exp(-x * x) / (2.319 * x + sqrt(4.0 + 1.52 * x * x));
}

float Lambda(float cosTheta, float sigmaSq) {
    float v = cosTheta / sqrt((1.0 - cosTheta * cosTheta) * (2.0 * sigmaSq));
    return max(0.0, (exp(-v * v) - v * sqrt(PI) * erfc(v)) / (2.0 * v * sqrt(PI)));
    //return (exp(-v * v)) / (2.0 * v * sqrt(PI));
}

// L, V, N in world space
float reflectedSunRadiance(vec3 L, vec3 V, vec3 N, float sigmaSq) {
    vec3 H = normalize(L + V);
    vec3 Ty = normalize(cross(N, vec3(1.0, 0.0, 0.0)));
    vec3 Tx = cross(Ty, N);
    float zetax = dot(H, Tx) / dot(H, N);
    float zetay = dot(H, Ty) / dot(H, N);

    float zL = dot(L, N); // cos of source zenith angle
    float zV = dot(V, N); // cos of receiver zenith angle
    float zH = dot(H, N); // cos of facet normal zenith angle
    float zH2 = zH * zH;

    float p = exp(-0.5 * (zetax * zetax + zetay * zetay) / sigmaSq) / (2.0 * PI * sigmaSq);

    float fresnel = 0.02 + 0.98 * pow(1.0 - dot(V, H), 5.0);

    zL = max(zL,0.01);
    zV = max(zV,0.01);

    return fresnel * p / ((1.0 + Lambda(zL, sigmaSq) + Lambda(zV, sigmaSq)) * zV * zH2 * zH2 * 4.0);
}

// L, V, N in world space
float wardReflectedSunRadiance(vec3 L, vec3 V, vec3 N, float sigmaSq) {
    vec3 H = normalize(L + V);

    float hn = dot(H, N);
    float p = exp(-2.0 * ((1.0 - hn * hn) / sigmaSq) / (1.0 + hn)) / (4.0 * PI * sigmaSq);

    float c = 1.0 - dot(V, H);
    float c2 = c * c;
    float fresnel = 0.02 + 0.98 * c2 * c2 * c;

    float zL = dot(L, N);
    float zV = dot(V, N);
    zL = max(zL,0.01);
    zV = max(zV,0.01);

    // brdf times cos(thetaL)
    return zL <= 0.0 ? 0.0 : max(fresnel * p * sqrt(abs(zL / zV)), 0.0);
}

// ---------------------------------------------------------------------------
// REFLECTED SKY RADIANCE
// ---------------------------------------------------------------------------

uniform sampler2DArray skymapSampler;

vec3 skymapRadiance(vec2 tau, vec2 dtaudx, vec2 dtaudy, vec3 sunDir)
{
    vec2 sun = normalize(sunDir.xy);
    mat2 rot = mat2(sun.x, -sun.y, sun.y, sun.x);
    vec2 uv = vec2(0.5 / 255.0)+ ((rot * tau) * 0.5 / 1.1 + 0.5) * (255.0 / 256.0);
    vec2 duvdx = rot * dtaudx;
    vec2 duvdy = rot * dtaudy;

    float l = acos(sunDir.z) * (28.0 / (PI / 2.0));
    float layer = floor(l);
    float a = l - layer;
    vec3 l0 = texture2DArrayGrad(skymapSampler, vec3(uv, layer), duvdx, duvdy).rgb;
    vec3 l1 = texture2DArrayGrad(skymapSampler, vec3(uv, layer + 1.0), duvdx, duvdy).rgb;
    return (l0 * (1.0 -  a) + l1 * a) * 100.0;
}

float meanFresnel(float cosThetaV, float sigmaV) {
    return pow(1.0 - cosThetaV, 5.0 * exp(-2.69 * sigmaV)) / (1.0 + 22.7 * pow(sigmaV, 1.5));
}

float meanFresnel(vec3 V, vec3 N, float sigmaSq) {
    return meanFresnel(dot(V, N), sqrt(sigmaSq));
}

vec2 U(vec2 zeta, vec3 V) {
    vec3 F = normalize(vec3(-zeta, 1.0));
    vec3 R = 2.0 * dot(F, V) * F - V;
    return -R.xy / (1.0 + R.z);
}

// V, N, sunDir in world space
vec3 reflectedSkyRadiance(vec3 V, vec3 N, float sigmaSq, vec3 sunDir) {
    vec3 result = vec3(0.0);

    vec2 zeta0 = - N.xy / N.z;
    vec2 tau0 = U(zeta0, V);

    const float n = 1.0 / 1.1;
    vec2 JX = (U(zeta0 + vec2(0.01, 0.0), V) - tau0) / 0.01 * n * sqrt(sigmaSq);
    vec2 JY = (U(zeta0 + vec2(0.0, 0.01), V) - tau0) / 0.01 * n * sqrt(sigmaSq);

    result = skymapRadiance(tau0, JX, JY, sunDir);

    result *= 0.02 + 0.98 * meanFresnel(V, N, sigmaSq);
    return result;
}

vec3 reflectedSkyRadianceW(vec3 V, vec3 N, float sigmaSq, vec3 sunDir) {
    vec3 NX = normalize(vec3(-N.y, N.x, 0.0));
    vec3 NY = cross(N, NX);
    vec3 tV = vec3(dot(V, NX), dot(V, NY), dot(V, N));
    vec3 tSun = vec3(dot(sunDir, NX), dot(sunDir, NY), dot(sunDir, N));
    return reflectedSkyRadiance(tV, vec3(0.0, 0.0, 1.0), sigmaSq, tSun);
}

// ---------------------------------------------------------------------------
// REFRACTED SEA RADIANCE
// ---------------------------------------------------------------------------

float refractedSeaRadiance(vec3 V, vec3 N, float sigmaSq) {
    return 0.98 * (1.0 - meanFresnel(V, N, sigmaSq));
}

void main() {
    vec3 WSD = getWorldSunDir();
    vec3 WCP = getWorldCameraPos();
    float lod = oceanLod;
    vec2 uv = oceanUv;
    vec3 dPdu = oceanDPdu;
    vec3 dPdv = oceanDPdv;
    float sigmaSq = oceanSigmaSq;

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
        sigmaSq -= wt.z * wt.z * (sqrt(1.0 - wkh * wkh) - sqrt(1.0 - kh * kh));
    }

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

    //vec3 Lsun = reflectedSunRadiance(oceanSunDir, V, N, sigmaSq) * sunL;
    //vec3 Lsky = reflectedSkyRadiance(V, N, sigmaSq, oceanSunDir);
    vec3 Lsun = wardReflectedSunRadiance(oceanSunDir, V, N, sigmaSq) * sunL;
    vec3 Lsky = meanFresnel(V, N, sigmaSq) * skyE / PI;
    vec3 Lsea = refractedSeaRadiance(V, N, sigmaSq) * seaColor * skyE / PI;

    vec3 surfaceColor = Lsun + Lsky + Lsea;

    // aerial perspective
    vec3 inscatter = inScattering(earthCamera, earthP, oceanSunDir, extinction, 0.0);
    vec3 finalColor = surfaceColor * extinction + inscatter;

    data.rgb = hdr(finalColor);
    data.a = 1.0;
}

#endif
