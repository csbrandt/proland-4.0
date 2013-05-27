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

#ifdef _VERTEX_

layout(location=0) in vec4 vertex;
out vec2 uv;

void main() {
    gl_Position = vertex;
    uv = vertex.xy * 0.5 + vec2(0.5);
}

#endif

#ifdef _FRAGMENT_

#include "globalsShader.glhl"

uniform sampler2D colorSampler;
uniform sampler2D depthSampler;

in vec2 uv;
layout(location=0) out vec4 data;

vec3 hdr1(vec3 L) {
    L = L * getHdrExposure();
    L.r = L.r < 1.413 ? pow(L.r * 0.38317, 1.0 / 2.2) : 1.0 - exp(-L.r);
    L.g = L.g < 1.413 ? pow(L.g * 0.38317, 1.0 / 2.2) : 1.0 - exp(-L.g);
    L.b = L.b < 1.413 ? pow(L.b * 0.38317, 1.0 / 2.2) : 1.0 - exp(-L.b);
    return L;
}

void main() {
    vec3 l0 = textureLod(colorSampler, uv, 0.0).rgb;
    vec3 l1 = textureLod(colorSampler, uv, 1.0).rgb;
    vec3 l2 = textureLod(colorSampler, uv, 2.0).rgb;
    vec3 l3 = textureLod(colorSampler, uv, 3.0).rgb;
    vec3 l4 = textureLod(colorSampler, uv, 4.0).rgb;
    vec3 l5 = textureLod(colorSampler, uv, 5.0).rgb;
    data.rgb = hdr1(l0 + max((l1 + l2 + l3 + l4 + l5) / 5.0 - 2.0 * getHdrExposure(), 0.0));
    gl_FragDepth = texture(depthSampler, uv).x;
}

#endif
