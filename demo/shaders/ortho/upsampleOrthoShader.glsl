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
uniform vec4 coarseLevelOSL; // lower left corner of patch to upsample, one over size in pixels of coarse level texture

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

vec3 rgb_to_hsv(vec3 RGB)
{
    vec3 HSV = vec3(0.0);
    float minVal = min(RGB.x, min(RGB.y, RGB.z));
    float maxVal = max(RGB.x, max(RGB.y, RGB.z));
    float delta = maxVal - minVal;
    HSV.z = maxVal;
    if (delta != 0) {
       HSV.y = delta / maxVal;
       vec3 delRGB;
       delRGB = (((vec3(maxVal) - RGB) / 6.0) + delta / 2.0) / delta;
       if (RGB.x == maxVal) {
           HSV.x = delRGB.z - delRGB.y;
       } else if (RGB.y == maxVal) {
           HSV.x = ( 1.0/3.0) + delRGB.x - delRGB.z;
       } else if (RGB.z == maxVal) {
           HSV.x = ( 2.0/3.0) + delRGB.y - delRGB.x;
       }
       if (HSV.x < 0.0) {
           HSV.x += 1.0;
       }
       if (HSV.x > 1.0) {
           HSV.x -= 1.0;
       }
    }
    return HSV;
}

vec3 hsv_to_rgb(vec3 HSV)
{
    vec3 RGB = HSV.zzz;
    if (HSV.y != 0) {
       float var_h = HSV.x * 6;
       float var_i = floor(var_h);
       float var_1 = HSV.z * (1.0 - HSV.y);
       float var_2 = HSV.z * (1.0 - HSV.y * (var_h-var_i));
       float var_3 = HSV.z * (1.0 - HSV.y * (1-(var_h-var_i)));
       if (var_i == 0) {
           RGB = vec3(HSV.z, var_3, var_1);
       } else if (var_i == 1) {
           RGB = vec3(var_2, HSV.z, var_1);
       } else if (var_i == 2) {
           RGB = vec3(var_1, HSV.z, var_3);
       } else if (var_i == 3) {
           RGB = vec3(var_1, var_2, HSV.z);
       } else if (var_i == 4) {
           RGB = vec3(var_3, var_1, HSV.z);
       } else {
           RGB = vec3(HSV.z, var_1, var_2);
       }
   }
   return RGB;
}

void main() {
    vec4 result = vec4(128.0);
    vec2 uv = floor(st);

    if (residualOSH.x != -1.0) {
        result = textureLod(residualSampler, uv * residualOSH.z + residualOSH.xy, 0.0) * 255.0;
    } else if (coarseLevelOSL.x == -1.0) {
        result = rootNoiseColor * 255.0;
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
    vec4 noise = textureLod(noiseSampler, vec3(uvs[noiseUVLH.x], uvs[noiseUVLH.y], noiseUVLH.z), 0.0) * 255.0;
    if (noiseUVLH.w == 1.0) {
        vec3 hsv = rgb_to_hsv(result.rgb / 255.0);
        hsv *= vec3(1.0) + (1.0 - smoothstep(0.4, 0.8, hsv.z)) * noiseColor.rgb * (noise.rgb - vec3(128.0)) / 255.0;
        hsv.x = fract(hsv.x);
        hsv.y = clamp(hsv.y, 0.0, 1.0);
        hsv.z = clamp(hsv.z, 0.0, 1.0);
        result = vec4(hsv_to_rgb(hsv) * 255.0, noiseColor.a * (noise.a - 128.0) + result.a);
    } else {
        result = noiseColor * (noise - vec4(128.0)) + result;
    }

    data = result / 255.0;
}

#endif
