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

layout(location=0) in vec2 vertex;
layout(location=1) in vec2 uv;
out vec2 st;

void main() {
    gl_Position = vec4((vertex - tileOffset.xy) * tileOffset.z, 0.0, 1.0);
    st = uv;
}

#endif

#ifdef _FRAGMENT_

uniform int river;

in vec2 st;
layout(location=0) out vec4 data;

void main() {
    float dist = abs(st.y);
    float z = st.x;
    float blending = 0.0;
    if (river != 0.0) {
        z = st.x;
        z = z - 1.0 + max(dist - 1.0, 0.0);
    }
    blending = smoothstep(1.0, 2.0, dist);
    data = vec4(0.0, 0.0, z, blending);
    if (river == 2.0) { // large rivers
        gl_FragDepth = 1.0;
    } else if (river == 1.0) { //normal rivers & lake boundaries
        gl_FragDepth = dist * 0.1 + 0.15;
    } else { // other
        gl_FragDepth = dist * 0.1 + 0.3;
    }
}

#endif
