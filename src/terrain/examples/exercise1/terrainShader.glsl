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

struct samplerTile {
    sampler2DArray tilePool; // tile cache
    vec3 tileCoords; // coords of currently selected tile in tile cache (u,v,layer; u,v in [0,1]^2)
    vec3 tileSize; // size of currently selected tile in tile cache (du,dv,d; du,dv in [0,1]^2, d in pixels)
};

// returns content of currently selected tile, at uv coordinates (in [0,1]^2; relatively to this tile)
vec4 textureTile(samplerTile tex, vec2 uv) {
    vec3 uvl = tex.tileCoords + vec3(uv * tex.tileSize.xy, 0.0);
    return texture(tex.tilePool, uvl);
}

uniform struct {
    vec4 offset;
    vec4 camera;
    vec2 blending;
    mat4 localToScreen;
} deformation;

uniform samplerTile elevationSampler;
uniform samplerTile fragmentNormalSampler;
uniform samplerTile orthoSampler;

#ifdef _VERTEX_

layout(location=0) in vec3 vertex;
out vec2 uv;
out vec3 p;

void main() {
    vec4 zfc = textureTile(elevationSampler, vertex.xy);

    vec2 v = abs(deformation.camera.xy - vertex.xy);
    float d = max(max(v.x, v.y), deformation.camera.z);
    float blend = clamp((d - deformation.blending.x) / deformation.blending.y, 0.0, 1.0);

    float h = zfc.z * (1.0 - blend) + zfc.y * blend;
    p = vec3(vertex.xy * deformation.offset.z + deformation.offset.xy, h);

    gl_Position = deformation.localToScreen * vec4(p, 1.0);
    uv = vertex.xy;
}

#endif

#ifdef _FRAGMENT_

#include "atmosphereShader.glhl"

in vec2 uv;
in vec3 p;
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

void main() {
    data = textureTile(orthoSampler, uv);

    float h = textureTile(elevationSampler, uv).x;
    if (h < 0.1) {
        data = vec4(0.0, 0.0, 0.5, 1.0);
    }

    vec3 n = vec3(textureTile(fragmentNormalSampler, uv).xy * 2.0 - 1.0, 0.0);
    n.z = sqrt(max(0.0, 1.0 - dot(n.xy, n.xy)));

    vec3 WSD = getWorldSunDir();
    vec3 WCP = getWorldCameraPos();

    vec3 sunL;
    vec3 skyE;
    vec3 extinction;
    sunRadianceAndSkyIrradiance(p, n, WSD, sunL, skyE);

    //float light = dot(n, normalize(vec3(1.0)));
    //data.rgb *= 0.5 + 0.75 * light;
    float cosTheta = dot(n, WSD);
    vec3 lightColor = (sunL * max(cosTheta, 0.0) + skyE) / 3.14159265;
    data.rgb = 0.1 * data.rgb * lightColor;

    // aerial perspective
    vec3 inscatter = inScattering(WCP, p, WSD, extinction, 0.0);
    data.rgb = data.rgb * extinction + inscatter;

    data.rgb = hdr(data.rgb);

    //data.r += mod(dot(floor(deformation.offset.xy / deformation.offset.z + 0.5), vec2(1.0)), 2.0);
}

#endif
