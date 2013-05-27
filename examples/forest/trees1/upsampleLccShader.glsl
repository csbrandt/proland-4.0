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

uniform float tileWidth; // size in pixels of one tile (including borders)

uniform sampler2DArray coarseLevelSampler; // coarse level texture
uniform vec4 coarseLevelOSL; // lower left corner of patch to upsample, one over size in pixels of coarse level texture, layer id

uniform sampler2D residualSampler; // residual texture
uniform vec4 residualOSH; // lower left corner of residual patch to use, scaling factor of residual texture coordinates, scaling factor of residual values

uniform sampler2DArray noiseSampler;
uniform ivec4 noiseUVLH;
uniform vec4 noiseColor;
uniform vec4 rootNoiseColor;

#ifdef _VERTEX_

layout(location=0) in vec4 vertex;
out vec2 st;

void main() {
    st = (vertex.xy * 0.5 + 0.5) * tileWidth;
    gl_Position = vertex;
}

#endif

#ifdef _FRAGMENT_

in vec2 st;
layout(location=0) out vec4 data;

vec4 masks[4] = vec4[4] (
    vec4(1.0, 3.0, 3.0, 9.0),
    vec4(3.0, 1.0, 9.0, 3.0),
    vec4(3.0, 9.0, 1.0, 3.0),
    vec4(9.0, 3.0, 3.0, 1.0)
);

void main() {
    vec4 result = vec4(128.0);
    vec2 uv = floor(st);

    if (residualOSH.x != -1.0) {
    	vec4 s = textureLod(residualSampler, uv * residualOSH.z + residualOSH.xy, 0.0);
        result.r = s.r * 255.0;
    }

    if (coarseLevelOSL.x != -1.0) {
        vec4 mask = masks[int(mod(uv.x, 2.0) + 2.0 * mod(uv.y, 2.0))];
        vec3 offset = vec3(floor((uv + vec2(1.0)) * 0.5) * coarseLevelOSL.z + coarseLevelOSL.xy, coarseLevelOSL.w);
        vec4 c0 = textureLod(coarseLevelSampler, offset, 0.0) * 255.0;
        vec4 c1 = textureLod(coarseLevelSampler, offset + vec3(coarseLevelOSL.z, 0.0, 0.0), 0.0) * 255.0;
        vec4 c2 = textureLod(coarseLevelSampler, offset + vec3(0.0, coarseLevelOSL.z, 0.0), 0.0) * 255.0;
        vec4 c3 = textureLod(coarseLevelSampler, offset + vec3(coarseLevelOSL.zz, 0.0), 0.0) * 255.0;
        vec4 c = floor((mask.x * c0 + mask.y * c1 + mask.z * c2 + mask.w * c3) / 16.0);
        result = (result - vec4(128.0)) * residualOSH.w + c;
    }

    vec2 nuv = (uv + vec2(0.5)) / tileWidth;
    vec4 uvs = vec4(nuv, vec2(1.0) - nuv);
    if (noiseUVLH.w == 1) {
        result.r = 255.0;
    } else {
        vec4 noise = textureLod(noiseSampler,  vec3(uvs[noiseUVLH.x], uvs[noiseUVLH.y], noiseUVLH.z), 0.0) * 255.0;
        result.r = noiseColor.r * (noise.r - 128.0) + result.r;
    }

    data = vec4(result.r / 255.0, 0.0, 0.0, 0.0);
}

#endif
