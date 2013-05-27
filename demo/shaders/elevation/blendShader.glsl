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

const vec2 dx = vec2(1.0, 0.0);
const vec2 dy = vec2(0.0, 1.0);
const vec3 filter = vec3(1.0, 1.0, 1.0) / (1.0 + 4.0*1.0 + 4.0*1.0);

uniform sampler2D coarseLevelSampler;
uniform float scale;

#ifdef _VERTEX_

layout(location = 0) in vec4 vertex;
out vec2 st;

void main() {
    gl_Position = vertex;
    st = vertex.xy * 0.5 + vec2(0.5);
}

#endif

#ifdef _FRAGMENT_

in vec2 st;
layout(location = 0) out vec4 data;

float blend(vec4 v) {
    float blending = min(v.w, 1.0);
    return v.x * blending + v.z * (1.0 - blending);
}

void main() {
    vec4 v = texture(coarseLevelSampler, st);
    vec3 samples = vec3(blend(v),
    blend(texture(coarseLevelSampler, st + dx * scale)) +
    blend(texture(coarseLevelSampler, st - dx * scale)) +
    blend(texture(coarseLevelSampler, st + dy * scale)) +
    blend(texture(coarseLevelSampler, st - dy * scale)),
    blend(texture(coarseLevelSampler, st + (dx + dy) * scale)) +
    blend(texture(coarseLevelSampler, st + (dx - dy) * scale)) +
    blend(texture(coarseLevelSampler, st - (dx + dy) * scale)) +
    blend(texture(coarseLevelSampler, st - (dx - dy) * scale)));
    v.z = dot(samples, filter);

    data = v;
    //gl_FragDepth = 1.0;
}

#endif
