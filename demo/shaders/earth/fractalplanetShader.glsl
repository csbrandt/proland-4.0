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
} deformation;

#ifdef _VERTEX_

layout(location=0) in vec3 vertex;
out vec3 p;
out vec3 n;
out vec2 uv;

void main() {
    //vec4 zfc = vec4(0.0, 0.0, 0.0, 1.0);
    //vec4 nfc = vec4(0.0);
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

    float h = zfc.z * (1.0 - blend) + zfc.y * blend;
    float k = min(length(P) / dot(alpha, L) * 1.0000003, 1.0);
    float hPrime = (h + R * (1.0 - k)) / k;

#ifdef CUBE_PROJECTION
    gl_Position = deformation.localToScreen * vec4(P + vec3(0.0, 0.0, h), 1.0);
#else
    gl_Position = (C + hPrime * N) * alphaPrime;
#endif

    p = (deformation.radius + h) * normalize(deformation.localToWorld * P);

#ifdef VERTEX_NORMALS
    vec3 nf = vec3(nfc.xy, sqrt(1.0 - dot(nfc.xy, nfc.xy)));
    vec3 nc = vec3(nfc.zw, sqrt(1.0 - dot(nfc.zw, nfc.zw)));
    n = deformation.tangentFrameToWorld * (nf * (1.0 - blend) + nc * blend);
#endif

    uv = vertex.xy;
}

#endif

#ifdef _FRAGMENT_

#include "atmosphereShader.glhl"
#include "oceanBrdf.glhl"

in float h;
in vec3 p;
in vec3 n;
in vec2 uv;
layout(location=0) out vec4 data;

void main() {
    vec3 WCP = getWorldCameraPos();
    vec3 WSD = getWorldSunDir();

    vec3 V = normalize(p);
    vec3 P = V * max(length(p), deformation.radius + 10.0);
    vec3 v = normalize(P - WCP);

#ifdef VERTEX_NORMALS
    vec3 fn = normalize(n);
    float nz = fn.z;
#else
    vec3 fn = vec3(textureTile(fragmentNormalSampler, uv).xy * 2.0 - 1.0, 0.0);
    fn.z = sqrt(max(0.0, 1.0 - dot(fn.xy, fn.xy)));
    float nz = fn.z;
    fn = deformation.tangentFrameToWorld * fn;
#endif
    float cTheta = dot(fn, WSD);
    float vSun = dot(V, WSD);

    float h = textureTile(elevationSampler, uv).x;
    vec4 reflectance;

    if (h <= 0.0) {
        reflectance = vec4(0.0,0.0,0.0,0.0);
    } else {
        reflectance = mix(vec4(0.02,0.05,0.0,0.0), vec4(0.01,0.01,0.0,0.0), smoothstep(0.0, 4000.0, h));
        reflectance = mix(reflectance, vec4(vec3(0.5),0.0), smoothstep(5000.0, 6000.0, h));
    }

    float rocks = deformation.offset.z > 500000.0 ? 0.0 : smoothstep(-0.92, -0.85, -nz);
    reflectance.rgb = rocks * vec3(0.08) + (1.0 - rocks) * reflectance.rgb;

    vec3 sunL;
    vec3 skyE;
    sunRadianceAndSkyIrradiance(P, fn, WSD, sunL, skyE);

    // diffuse ground color
    vec3 groundColor = 1.5 * reflectance.rgb * (sunL * max(cTheta, 0.0) + skyE) / 3.14159265;

    if (h <= 0.0) {
        groundColor = oceanRadiance(-v, V, WSD, getSeaRoughness(), sunL, skyE);
    }

    vec3 extinction;
    vec3 inscatter = inScattering(WCP, P, WSD, extinction, 0.0);

    vec3 finalColor = groundColor * extinction + inscatter;

    data.rgb = hdr(finalColor);
    data.a = 1.0;

#ifdef QUADTREE_ON
    data.rgb += mod(dot(floor(deformation.offset.xy / deformation.offset.z + 0.5), vec2(1.0)), 2.0) * vec3(0.25, 0.0, 0.0);
#endif
}

#endif
