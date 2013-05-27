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

uniform samplerTile elevationSampler;
uniform samplerTile fragmentNormalSampler;
uniform samplerTile orthoSampler;
uniform samplerTile lccSampler;

uniform struct {
    vec4 offset;
    vec4 camera;
    vec2 blending;
    mat4 localToScreen;
    mat3 tileToTangent;
    mat4 screenQuadCorners;
    mat4 screenQuadVerticals;
} deformation;

#ifdef _VERTEX_

layout(location=0) in vec3 vertex;
out vec3 p;
out vec3 q;
out vec2 uv;

void main() {
    vec4 zfc = textureTile(elevationSampler, vertex.xy);

    vec2 v = abs(deformation.camera.xy - vertex.xy);
    float d = max(max(v.x, v.y), deformation.camera.z);
    float blend = clamp((d - deformation.blending.x) / deformation.blending.y, 0.0, 1.0);
    float h = zfc.z * (1.0 - blend) + zfc.y * blend;

    p = vec3(vertex.xy * deformation.offset.z + deformation.offset.xy, h);
    q = vec3((deformation.tileToTangent * vec3(vertex.xy, 1.0)).xy, h);
    uv = vertex.xy;

    mat4 C = deformation.screenQuadCorners;
    mat4 N = deformation.screenQuadVerticals;

    vec4 uvUV = vec4(vertex.xy, vec2(1.0) - vertex.xy);
    vec4 alpha = uvUV.zxzx * uvUV.wwyy;
    gl_Position = (C + h * N) * alpha;
}

#endif

#ifdef _FRAGMENT_

#include "atmosphereShader.glhl"
#include "oceanBrdf.glhl"
#include "treeBrdf.glhl"

in vec3 p;
in vec3 q;
in vec2 uv;
layout(location=0) out vec4 data;

void main() {
    vec3 WCP = getWorldCameraPos();
    vec3 WSD = getWorldSunDir();
    vec3 v = normalize(p - WCP);

    vec3 fn = vec3(textureTile(fragmentNormalSampler, uv).xy * 2.0 - 1.0, 0.0);
    fn.z = sqrt(max(0.0, 1.0 - dot(fn.xy, fn.xy)));

    vec3 sunL;
    vec3 skyE;
    sunRadianceAndSkyIrradiance(p, fn, WSD, sunL, skyE);

    vec3 reflectance = textureTile(orthoSampler, uv).rgb * 0.1;

    // diffuse ground color
    vec3 lightColor = (sunL * max(dot(fn, WSD), 0.0) + skyE) / 3.14159265;
    vec3 groundColor = reflectance * lightColor;

    if (textureTile(elevationSampler, uv).z <= 0.0) {
        groundColor = oceanRadiance(-v, fn, WSD, getSeaRoughness(), sunL, skyE);
    }

    vec4 lcc = textureTile(lccSampler, uv);
    if (lcc.x > 0.0) {
        float d = length(vec3(q.xy, q.z - deformation.camera.w));
        groundColor = treeBrdf(q, d, lcc, v, fn, WSD, vec3(0.0, 0.0, 1.0), reflectance, sunL, skyE);
    }

    vec3 extinction;
    vec3 inscatter = inScattering(WCP, p, WSD, extinction, 0.0);

    vec3 finalColor = groundColor * extinction + inscatter;

    data.rgb = hdr(finalColor);
    data.a = 1.0;

    vec4 PEN = getPencil();
    if (length(p.xy - PEN.xy) < PEN.w) {
        data += getPencilColor();
    }

#ifdef QUADTREE_ON
    data.r += mod(dot(floor(deformation.offset.xy / deformation.offset.z + 0.5), vec2(1.0)), 2.0);
#endif
}

#endif
