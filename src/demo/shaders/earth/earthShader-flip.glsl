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

#define FLIP
#include "earthShader.glsl"

#ifdef _GEOMETRY_
#extension GL_EXT_geometry_shader4 : enable

layout (lines_adjacency) in;
layout (triangle_strip,max_vertices=4) out;

in float hIn[];
in float h0In[];
in vec3 pIn[];
in vec3 nIn[];
in vec2 uvIn[];

out float h;
out vec3 p;
out vec3 n;
out vec2 uv;

void emit(int i) {
    gl_Position = gl_PositionIn[i];
    h = hIn[i];
    p = pIn[i];
    n = nIn[i];
    uv = uvIn[i];
    EmitVertex();
}

void main() {
    int a = h0In[3] + h0In[1] >= h0In[0] + h0In[2] ? 0 : 5;
    emit(a % 4);
    emit((a + 1) % 4);
    emit((a + 3) % 4);
    emit((a + 2) % 4);
    EndPrimitive();
}

#endif
