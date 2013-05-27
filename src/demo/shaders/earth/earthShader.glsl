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

#define SPHERE_PROJECTION

#if !defined(VERTEX_NORMALS) && !defined(FRAGMENT_NORMALS)
#define VERTEX_NORMALS
#endif

uniform samplerTile elevationSampler;
uniform samplerTile vertexNormalSampler;
uniform samplerTile fragmentNormalSampler;
uniform samplerTile orthoSampler;
uniform samplerTile ambientApertureSampler;
uniform samplerTile lccSampler;

uniform struct {
    vec4 offset;
    vec4 camera;
    vec2 blending;
    mat4 localToScreen;
    float radius;
    mat3 localToWorld;
    mat4 screenQuadCorners;
    mat4 screenQuadVerticals;
    vec4 screenQuadCornerNorms;
    mat3 tangentFrameToWorld;
    mat3 tileToTangent;
} deformation;

#ifdef _VERTEX_

layout(location=0) in vec3 vertex;
#ifdef FLIP
float h;
vec3 p;
vec3 q;
vec3 n;
vec2 uv;
out float hIn;
out float h0In;
out vec3 pIn;
out vec3 qIn;
out vec3 nIn;
out vec2 uvIn;
#else
out float h;
out vec3 p;
out vec3 q;
out vec3 n;
out vec2 uv;
#endif

void main() {
    vec4 zfc = textureTile(elevationSampler, vertex.xy);
    vec4 nfc = textureTile(vertexNormalSampler, vertex.xy) * 2.0 - vec4(1.0);

    vec2 v = abs(deformation.camera.xy - vertex.xy);
    float d = max(max(v.x, v.y), deformation.camera.z);
    float blend = clamp((d - deformation.blending.x) / deformation.blending.y, 0.0, 1.0);

    float R = deformation.radius;
    mat4 C = deformation.screenQuadCorners;
    mat4 N = deformation.screenQuadVerticals;
    vec4 L = deformation.screenQuadCornerNorms;
    vec3 P = vec3(vertex.xy * deformation.offset.z + deformation.offset.xy, R);

    vec4 uvUV = vec4(vertex.xy, vec2(1.0) - vertex.xy);
    vec4 alpha = uvUV.zxzx * uvUV.wwyy;
    vec4 alphaPrime = alpha * L / dot(alpha, L);

    h = zfc.z * (1.0 - blend) + zfc.y * blend;
    float k = min(length(P) / dot(alpha, L) * 1.0000003, 1.0);
    float hPrime = (h + R * (1.0 - k)) / k;

#ifdef CUBE_PROJECTION
    gl_Position = deformation.localToScreen * vec4(P + vec3(0.0, 0.0, h), 1.0);
#else
    gl_Position = (C + hPrime * N) * alphaPrime;
#endif
    p = (deformation.radius + h) * normalize(deformation.localToWorld * P);
    q = vec3((deformation.tileToTangent * vec3(vertex.xy, 1.0)).xy, h);

#ifdef VERTEX_NORMALS
    vec3 nf = vec3(nfc.xy, sqrt(1.0 - dot(nfc.xy, nfc.xy)));
    vec3 nc = vec3(nfc.zw, sqrt(1.0 - dot(nfc.zw, nfc.zw)));
    n = deformation.tangentFrameToWorld * (nf * (1.0 - blend) + nc * blend);
#endif

    uv = vertex.xy;
#ifdef FLIP
    hIn = h;
    h0In = zfc.z;
    pIn = p;
    qIn = q;
    nIn = n;
    uvIn = uv;
#endif
}

#endif

#ifdef _FRAGMENT_

#include "atmosphereShader.glhl"
#include "oceanBrdf.glhl"
#include "treeBrdf.glhl"
#include "clouds.glhl"

in float h;
in vec3 p;
in vec3 q;
in vec3 n;
in vec2 uv;
layout(location=0) out vec4 data;

void main() {
    vec3 WCP = getWorldCameraPos();
    vec3 WSD = getWorldSunDir();

    float pixelScale = length(dFdx(p)+dFdy(p));

    vec3 V = normalize(p);
    vec3 P = V * max(length(p), deformation.radius + 10.0);
    vec3 v = normalize(P - WCP);

    float nz = 1.0;

#ifdef VERTEX_NORMALS
    vec3 fn = normalize(n);
#else
    vec3 fn = vec3(textureTile(fragmentNormalSampler, uv).xy * 2.0 - 1.0, 0.0);
    fn.z = sqrt(max(0.0, 1.0 - dot(fn.xy, fn.xy)));
    nz = fn.z;
    fn = deformation.tangentFrameToWorld * fn;
#endif

#ifndef OTHER_PLANET
    if (h < 5.0) {
        fn = V; // hack to fix normals in oceans
    }
#endif

    float cTheta = dot(fn, WSD);
    float vSun = dot(V, WSD);

    vec4 reflectance = textureTile(orthoSampler, uv);
    reflectance.rgb = tan(1.37 * reflectance.rgb) / tan(1.37); //RGB to reflectance

#ifndef OTHER_PLANET
    float rocks = h < 10.0 ? 0.0 : smoothstep(-0.8, -0.6, -nz);
    if (rocks > 0.0) {
        float rockColor = 0.1 - 0.1 * exp(-6.0 * dot(reflectance.rgb, vec3(0.333)));
        reflectance.rgb = rocks * vec3(rockColor) + (1.0 - rocks) * reflectance.rgb;
    }
#endif

    float waterMask = smoothstep(0.3, 0.9, 2.0 * reflectance.w - 1.0);
    float nightMask = smoothstep(0.3, 0.9, 1.0 - 2.0 * reflectance.w);

#ifndef OTHER_PLANET
    /*if (h < 5.0 && waterMask > 0.0) {
        reflectance.rgb = vec3(0.0);
        waterMask = 1.0;
    }*/
#endif

    vec3 sunL;
    vec3 skyE;

#ifdef TERRAIN_SHADOW
    vec3 occ = textureTile(ambientApertureSampler, uv).rgb;
    float ambient = sqrt(sqrt(occ.x));
    vec2 bentn = tan(occ.yz * 2.9 - 1.45) / 8.0;
    vec4 ambientAperture = vec4(ambient, deformation.tangentFrameToWorld * vec3(bentn, sqrt(1.0 - dot(bentn, bentn))));
    sunRadianceAndSkyIrradiance(P, fn, WSD, pixelScale, ambientAperture, sunL, skyE);
#else
    sunRadianceAndSkyIrradiance(P, fn, WSD, sunL, skyE);
#endif

    sunL *= cloudsShadow(P, WSD, h, vSun, deformation.radius);

    // diffuse ground color
    vec3 groundColor = reflectance.rgb * (sunL * max(cTheta, 0.0) + skyE) / 3.14159265;

    vec4 lcc = textureTile(lccSampler, uv);
    if (lcc.r > 0.0) { // TODO need also if lcc.r=0?
    	float d = length(vec3(q.xy, q.z - deformation.camera.w));
        groundColor = treeBrdf(q, d, lcc, v, fn, WSD, V, reflectance.rgb, sunL, skyE);
    }

#ifndef OTHER_PLANET
    // water specular color due to sunLight
    if (waterMask > 0.0) {
        groundColor += waterMask * oceanRadiance(-v, V, WSD, getSeaRoughness(), sunL, skyE);
    }

    // emitted light (at night)
    groundColor += nightMask * smoothstep(0.05, 0.1, -vSun) * vec3(0.0466, 0.0453, 0.0177);
#endif

    vec3 extinction;
    vec3 inscatter = inScattering(WCP, P, WSD, extinction, 0.0);

    vec3 finalColor = groundColor * extinction + inscatter;

    data.rgb = hdr(finalColor);
    data.a = 1.0;

    vec4 PEN = getPencil();
    if (PEN.w > 0.0) {
        vec3 dp = (normalize(p) - normalize(PEN.xyz)) * deformation.radius;
        if (length(dp) < PEN.w) {
            data += getPencilColor();
        }
    }

#ifdef QUADTREE_ON
    data.rgb += mod(dot(floor(deformation.offset.xy / deformation.offset.z + 0.5), vec2(1.0)), 2.0) * vec3(0.25, 0.0, 0.0);
#endif
}

#endif
