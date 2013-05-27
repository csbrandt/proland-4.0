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

uniform vec3 tileOffset;
uniform vec4 color;
uniform vec3 stripeSize;
uniform vec2 blendSize;

uniform sampler2D asphaltSampler;

#ifdef _VERTEX_

layout(location=0) in vec2 vertex;
layout(location=1) in vec2 uv;
out vec2 st;

void main() {
    gl_Position = vec4((vertex - tileOffset.xy) * tileOffset.z, 0.0, 1.0);
    st = uv;
}

#endif

#ifdef _FRAGMENT_

in vec2 st;
layout(location=0) out vec4 data;

vec2 pulseTrainIntegral2(vec2 nedge, vec2 t) {
    return (1.0 - nedge) * floor(t) + max(fract(t) - nedge, 0.0);
}

vec4 pulseTrainIntegral4(vec4 nedge, vec4 t) {
    return (1.0 - nedge) * floor(t) + max(fract(t) - nedge, 0.0);
}

float filteredPulseTrain(float edge, float x, float dx) {
    vec2 x0x1 = dx * vec2(-0.5, 0.5) + x;
    vec2 pti = pulseTrainIntegral2(vec2(edge, edge), x0x1);
    return (pti.y - pti.x) / dx;
}

vec2 filteredPulseTrain2(vec2 edge, vec2 x, vec2 dx) {
    vec4 x0x1 = dx.xyxy * vec4(-0.5, -0.5, 0.5, 0.5) + x.xyxy;
    vec4 pti = pulseTrainIntegral4(edge.xyxy, x0x1);
    return (pti.zw - pti.xy) / dx;
}

void main() {
    data = color * texture(asphaltSampler, st * 0.5);

    if (stripeSize.z < 0.0) {
        const vec2 CELL_SIZE = vec2(7.0, 3.25); // in meters along and perpendicular to axis
        const vec2 STRIPE_SIZE = vec2(2.5, 0.1); // idem
        vec2 pq = (st + vec2(0.0, STRIPE_SIZE.y * 0.5)) / CELL_SIZE;
        vec2 rs = 1.0 - filteredPulseTrain2(STRIPE_SIZE / CELL_SIZE, pq, fwidth(pq));
        data.rgb = data.rgb + rs.x * rs.y;
    }

    if (blendSize.x != 0.0) {
        data.a = clamp(blendSize.x*abs(st.y) + blendSize.y, 0.0, 1.0);
    }
}

#endif
