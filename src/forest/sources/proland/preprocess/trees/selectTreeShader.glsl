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

const char * selectTreeSource = "\
#extension GL_EXT_gpu_shader4 : enable\n\
\n\
#include \"globalsShader.glhl\"\n\
\n\
#ifdef _VERTEX_\n\
\n\
layout(location=0) in vec3 vertex;\n\
out vec3 uvs;\n\
\n\
void main()\n\
{\n\
    uvs = vertex;\n\
}\n\
\n\
#endif\n\
\n\
#ifdef _GEOMETRY_\n\
\n\
layout(points) in;\n\
layout(points,max_vertices=1) out;\n\
\n\
uniform vec3 tileOffset;\n\
\n\
in vec3 uvs[];\n\
out vec3 pos;\n\
out vec3 params;\n\
\n\
void main()\n\
{\n\
    vec4 seeds = unpackUnorm4x8(floatBitsToUint(uvs[0].z));\n\
    float size = mix(getMinRadius(), getMaxRadius(), dot(seeds, vec4(0.25)));\n\
    pos = vec3(uvs[0].xy * tileOffset.z + tileOffset.xy, -0.35);\n\
    params = vec3(size, seeds.w, 0.0);\n\
    EmitVertex();\n\
    EndPrimitive();\n\
}\n\
\n\
#endif\n\
";
