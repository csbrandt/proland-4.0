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
    float radius;
    mat3 localToWorld;
    mat4 screenQuadCorners;
    mat4 screenQuadVerticals;
    vec4 screenQuadCornerNorms;
    mat3 tangentFrameToWorld;
} deformation;

uniform samplerTile elevationSampler;
uniform samplerTile fragmentNormalSampler;

#ifdef _VERTEX_

layout(location=0) in vec3 vertex;
out vec3 p;
out vec2 uv;

void main() {
    vec4 zfc = textureTile(elevationSampler, vertex.xy);

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

    gl_Position = (C + hPrime * N) * alphaPrime;
    p = (deformation.radius + h) * normalize(deformation.localToWorld * P);
    uv = vertex.xy;
}

#endif

#ifdef _FRAGMENT_

in vec3 p;
in vec2 uv;
layout(location=0) out vec4 data;

void main() {
    float h = textureTile(elevationSampler, uv).x;
    if (h < 0.1) {
        data = vec4(0.0, 0.0, 0.5, 1.0);
    } else {
        data = mix(vec4(0.2,0.5,0.0,1.0), vec4(0.1,0.1,0.0,1.0), smoothstep(0.0, 4000.0, h));
        data = mix(data, vec4(1.0), smoothstep(5000.0, 6000.0, h));
    }

    vec3 n = vec3(textureTile(fragmentNormalSampler, uv).xy * 2.0 - 1.0, 0.0);
    n.z = sqrt(max(0.0, 1.0 - dot(n.xy, n.xy)));

    float rocks = deformation.offset.z > 500000.0 ? 0.0 : smoothstep(-0.92, -0.85, -n.z);
    data.rgb = rocks * vec3(0.8) + (1.0 - rocks) * data.rgb;

    float light = max(-(deformation.tangentFrameToWorld * n).y, 0.0);
    data.rgb *= light;

    data.r += mod(dot(floor(deformation.offset.xy / deformation.offset.z + 0.5), vec2(1.0)), 2.0) * 0.5;
}

#endif
