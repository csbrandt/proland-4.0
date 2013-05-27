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

uniform sampler2D spectrum_1_2_Sampler;
uniform sampler2D spectrum_3_4_Sampler;

uniform float FFT_SIZE;

uniform vec4 INVERSE_GRID_SIZES;

uniform float t;

#ifdef _VERTEX_

layout(location=0) in vec4 vertex;
out vec2 uv;

void main() {
    gl_Position = vertex;
    uv = vertex.xy * 0.5 + vec2(0.5);
}

#endif

#ifdef _FRAGMENT_

in vec2 uv;
layout(location=0) out vec4 color0;
layout(location=1) out vec4 color1;
layout(location=2) out vec4 color2;
layout(location=3) out vec4 color3;
layout(location=4) out vec4 color4;

vec2 getSpectrum(float k, vec2 s0, vec2 s0c) {
    float w = sqrt(9.81 * k * (1.0 + k * k / (370.0 * 370.0)));
    float c = cos(w * t);
    float s = sin(w * t);
    return vec2((s0.x + s0c.x) * c - (s0.y + s0c.y) * s, (s0.x - s0c.x) * s + (s0.y - s0c.y) * c);
}

vec2 i(vec2 z) {
    return vec2(-z.y, z.x); // returns i times z (complex number)
}

void main() {
    vec2 st = floor(uv * FFT_SIZE) / FFT_SIZE;
    float x = uv.x > 0.5 ? st.x - 1.0 : st.x;
    float y = uv.y > 0.5 ? st.y - 1.0 : st.y;

    vec4 s12 = textureLod(spectrum_1_2_Sampler, uv, 0.0);
    vec4 s34 = textureLod(spectrum_3_4_Sampler, uv, 0.0);
    vec4 s12c = textureLod(spectrum_1_2_Sampler, vec2(1.0 + 0.5 / FFT_SIZE) - st, 0.0);
    vec4 s34c = textureLod(spectrum_3_4_Sampler, vec2(1.0 + 0.5 / FFT_SIZE) - st, 0.0);

    vec2 k1 = vec2(x, y) * INVERSE_GRID_SIZES.x;
    vec2 k2 = vec2(x, y) * INVERSE_GRID_SIZES.y;
    vec2 k3 = vec2(x, y) * INVERSE_GRID_SIZES.z;
    vec2 k4 = vec2(x, y) * INVERSE_GRID_SIZES.w;

    float K1 = length(k1);
    float K2 = length(k2);
    float K3 = length(k3);
    float K4 = length(k4);

    float IK1 = K1 == 0.0 ? 0.0 : 1.0 / K1;
    float IK2 = K2 == 0.0 ? 0.0 : 1.0 / K2;
    float IK3 = K3 == 0.0 ? 0.0 : 1.0 / K3;
    float IK4 = K4 == 0.0 ? 0.0 : 1.0 / K4;

    vec2 h1 = getSpectrum(K1, s12.xy, s12c.xy);
    vec2 h2 = getSpectrum(K2, s12.zw, s12c.zw);
    vec2 h3 = getSpectrum(K3, s34.xy, s34c.xy);
    vec2 h4 = getSpectrum(K4, s34.zw, s34c.zw);

    color0 = vec4(h1 + i(h2), h3 + i(h4));
    color1 = vec4(i(k1.x * h1) - k1.y * h1, i(k2.x * h2) - k2.y * h2);
    color2 = vec4(i(k3.x * h3) - k3.y * h3, i(k4.x * h4) - k4.y * h4);
    color3 = color1 * vec4(IK1, IK1, IK2, IK2);
    color4 = color2 * vec4(IK3, IK3, IK4, IK4);
}

#endif
