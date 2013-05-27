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
out vec2 uvIn;

void main() {
    gl_Position = vertex;
    uvIn = vertex.xy * 0.5 + vec2(0.5);
}

#endif

#ifdef _GEOMETRY_
#extension GL_EXT_gpu_shader4 : enable
#extension GL_EXT_geometry_shader4 : enable

uniform int nLayers;

layout(triangles) in;
layout(triangle_strip, max_vertices = 15) out;

in vec2 uvIn[];
out vec2 uv;

void main() {
    for (int i = 0; i < nLayers; ++i) {
        gl_Layer = i;
        gl_PrimitiveID = i;
        gl_Position = gl_PositionIn[0];
        uv = uvIn[0];
        EmitVertex();
        gl_Position = gl_PositionIn[1];
        uv = uvIn[1];
        EmitVertex();
        gl_Position = gl_PositionIn[2];
        uv = uvIn[2];
        EmitVertex();
        EndPrimitive();
    }
}

#endif

#ifdef _FRAGMENT_
#extension GL_EXT_gpu_shader4 : enable

uniform sampler2D butterflySampler;
uniform sampler2DArray imgSampler; // 2 complex inputs (= 4 values) per layer

uniform float pass;

in vec2 uv;
layout(location=0) out vec4 color;

// performs two FFTs on two inputs packed in a single texture
// returns two results packed in a single vec4
vec4 fft2(int layer, vec2 i, vec2 w) {
    vec4 input1 = textureLod(imgSampler, vec3(uv.x, i.x, layer), 0.0);
    vec4 input2 = textureLod(imgSampler, vec3(uv.x, i.y, layer), 0.0);
    float res1x = w.x * input2.x - w.y * input2.y;
    float res1y = w.y * input2.x + w.x * input2.y;
    float res2x = w.x * input2.z - w.y * input2.w;
    float res2y = w.y * input2.z + w.x * input2.w;
    return input1 + vec4(res1x, res1y, res2x, res2y);
}

void main() {
    vec4 data = textureLod(butterflySampler, vec2(uv.y, pass), 0.0);
    vec2 i = data.xy;
    vec2 w = data.zw;

    color = fft2(gl_PrimitiveID, i, w);
}

#endif
