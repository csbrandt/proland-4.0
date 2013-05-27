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

uniform vec3 tileOffset;

vec4 colors[4] = vec4[4] (
   vec4(1.0, 0.0, 0.0, 0.0),
   vec4(1.0, 0.0, 1.0, 0.0),
   vec4(0.0, 1.0, 1.0, 0.0),
   vec4(0.0, 1.0, 0.0, 0.0)
);

layout(location=0) in vec3 vertex;
out vec4 color;

void main() {
    color = colors[int(vertex.z)];
    gl_Position = vec4((vertex.xy - tileOffset.xy) * tileOffset.z, 0.0, 1.0);
}

#endif

#ifdef _FRAGMENT_

in vec4 color;
layout(location=0) out vec4 data;

void main() {
    data = color;
}

#endif
