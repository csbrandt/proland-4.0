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

uniform float N_SLOPE_VARIANCE;

uniform sampler2D spectrum_1_2_Sampler;
uniform sampler2D spectrum_3_4_Sampler;
uniform int FFT_SIZE;

uniform vec4 GRID_SIZES;
uniform float slopeVarianceDelta;

uniform float c;

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
layout(location=0) out vec4 color;

#define M_PI 3.14159265

float getSlopeVariance(vec2 k, float A, float B, float C, vec2 spectrumSample) {
    float w = 1.0 - exp(A * k.x * k.x + B * k.x * k.y + C * k.y * k.y);
    vec2 kw = k * w;
    return dot(kw, kw) * dot(spectrumSample, spectrumSample) * 2.0;
}

void main() {
    const float SCALE = 10.0;
    float a = floor(uv.x * N_SLOPE_VARIANCE);
    float b = floor(uv.y * N_SLOPE_VARIANCE);
    float A = pow(a / (N_SLOPE_VARIANCE - 1.0), 4.0) * SCALE;
    float C = pow(c / (N_SLOPE_VARIANCE - 1.0), 4.0) * SCALE;
    float B = (2.0 * b / (N_SLOPE_VARIANCE - 1.0) - 1.0) * sqrt(A * C);
    A = -0.5 * A;
    B = - B;
    C = -0.5 * C;

    float slopeVariance = slopeVarianceDelta;
    for (int y = 0; y < FFT_SIZE; ++y) {
        for (int x = 0; x < FFT_SIZE; ++x) {
            int i = x >= FFT_SIZE / 2 ? x - FFT_SIZE : x;
            int j = y >= FFT_SIZE / 2 ? y - FFT_SIZE : y;
            vec2 k = 2.0 * M_PI * vec2(i, j);

            vec4 spectrum12 = texture(spectrum_1_2_Sampler, vec2(float(x) + 0.5, float(y) + 0.5) / float(FFT_SIZE));
            vec4 spectrum34 = texture(spectrum_3_4_Sampler, vec2(float(x) + 0.5, float(y) + 0.5) / float(FFT_SIZE));

            slopeVariance += getSlopeVariance(k / GRID_SIZES.x, A, B, C, spectrum12.xy);
            slopeVariance += getSlopeVariance(k / GRID_SIZES.y, A, B, C, spectrum12.zw);
            slopeVariance += getSlopeVariance(k / GRID_SIZES.z, A, B, C, spectrum34.xy);
            slopeVariance += getSlopeVariance(k / GRID_SIZES.w, A, B, C, spectrum34.zw);
        }
    }
    color = vec4(slopeVariance);
}

#endif
