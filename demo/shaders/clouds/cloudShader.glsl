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

#include "textureTile.glsl"

uniform samplerTile elevationSampler;
uniform samplerTile normalSampler;
uniform samplerTile orthoSampler;

uniform struct {
    vec4 offset;
    vec4 camera;
    vec2 blending;
    mat4 worldToScreen;
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
out float z;

void main() {
    //vec4 zfc = vec4(0.0, 0.0, 0.0, 1.0);
    //vec4 nfc = vec4(0.0);
    vec4 zfc = textureTile(elevationSampler, vertex.xy);
    vec4 nfc = textureTile(normalSampler, vertex.xy) * 2.0 - vec4(1.0);

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

    float h = (zfc.z * (1.0 - blend) + zfc.y * blend) * vertex.z;
    float k = min(length(P) / dot(alpha, L) * 1.0000003, 1.0);
    float hPrime = (h + R * (1.0 - k)) / k;

    gl_Position = (C + hPrime * N) * alphaPrime;

    p = vec3(vertex.xy * deformation.offset.z + deformation.offset.xy, deformation.radius);
    p = (deformation.radius + h) * normalize(deformation.localToWorld * p);

    vec3 nf = vec3(nfc.xy, sqrt(1.0 - dot(nfc.xy, nfc.xy)));
    vec3 nc = vec3(nfc.zw, sqrt(1.0 - dot(nfc.zw, nfc.zw)));
    n = deformation.tangentFrameToWorld * (nf * (1.0 - blend) + nc * blend);

    uv = vertex.xy;
    z = vertex.z;
}

#endif

#ifdef _FRAGMENT_

#include "globalsShader.glhl"
#include "atmosphereShader.glhl"

uniform float cloudReflectanceFactor;

in vec3 p;
in vec3 n;
in vec2 uv;
in float z;
layout(location=0) out vec4 data;

void main() {
    vec3 WCP = getWorldCameraPos();
    vec3 WSD = getWorldSunDir();

    vec4 ortho = textureTile(orthoSampler, uv);
    if (ortho.r == 0.0) {
        discard;
    }

    vec3 V = normalize(p);
    vec3 P = V * max(length(p), deformation.radius);
    vec3 v = normalize(P - WCP);
    vec3 N = n;

    vec3 reflectance;

    if (z < 0.5) {
        N = V;
        reflectance = vec3(1.0 - ortho.r * cloudReflectanceFactor);
    } else {
        reflectance = vec3(ortho.r * cloudReflectanceFactor);
    }

    vec3 sunL;
    vec3 skyE;
    sunRadianceAndSkyIrradiance(P, N, WSD, sunL, skyE);

    // diffuse ground color
    float cTheta = 0.25 + 0.75 * max(dot(n, WSD), 0.0);
    vec3 cloudColor = reflectance.rgb * (sunL * cTheta + skyE) / 3.14159265;

    vec3 extinction;
    vec3 inscatter = inScattering(WCP, P, WSD, extinction, 0.0);

    vec3 finalColor = cloudColor * extinction + inscatter;

    // cloud texture color is seen here as an extinction coefficient, for a
    // vertical view dir, depending on cloud density, cloud width, etc...
    // we cannot deduce these cloud parameters, but can modulate this
    // coefficient for a non vertical view dir (apparent width multiplied by
    // abs(1.0/dot(N,viewdir)))
    float verticalExtinction = 1.0 - ortho.r;
    float cloudExtinction = pow(verticalExtinction, 1.0 / abs(dot(V, v)));

    data = vec4(hdr(finalColor), 1.0 - cloudExtinction);

#ifdef QUADTREE_ON
    data.rgb += mod(dot(floor(deformation.offset.xy / deformation.offset.z + 0.5), vec2(1.0)), 2.0) * vec3(0.25, 0.0, 0.0);
#endif
}

#endif
