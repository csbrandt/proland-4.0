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

#extension GL_EXT_gpu_shader4 : enable

#include "globalsShader.glhl"
#include "treeBrdf.glhl"

uniform vec4 tileOffset;
uniform mat2 tileDeform;
uniform vec4 tileClip;

uniform sampler2DArray densitySampler;
uniform vec4 densityOSL;

#ifdef _VERTEX_

layout(location=0) in vec3 vertex;
out vec3 pos;

void main()
{
    pos = vertex;
}

#endif

#ifdef _GEOMETRY_

layout(points) in;
layout(triangle_strip,max_vertices=4) out;

in vec3 pos[];
out vec2 uv;

vec2 deform(vec2 p)
{
    return tileDeform * (p - vec2(0.5)) + vec2(0.5);
}

void main()
{
    vec4 seeds = unpackUnorm4x8(floatBitsToUint(pos[0].z));
    float minSize = MIN_RADIUS;
    float maxSize = MAX_RADIUS;
    float size = mix(minSize, maxSize, dot(seeds, vec4(0.25)));
    float r = 2.0 * tileOffset.w;// * size;
    if (seeds.w <= getTreeDensity()) {
        vec2 P = deform(pos[0].xy);
        if (P.x > tileClip.x && P.x < tileClip.y && P.y > tileClip.z && P.y < tileClip.w) {
            vec3 uvl = vec3(densityOSL.xy + P * densityOSL.z, densityOSL.w);
            vec2 lc = texture(densitySampler, uvl).rg;
            if (lc.r > 0.25) {
                P = deform(pos[0].xy + vec2(-r, -r));
                gl_Position = vec4(tileOffset.xy + P * tileOffset.z, 0.0, 1.0);
                uv = vec2(0.0, 0.0);
                EmitVertex();
                P = deform(pos[0].xy + vec2(+r, -r));
                gl_Position = vec4(tileOffset.xy + P * tileOffset.z, 0.0, 1.0);
                uv = vec2(1.0, 0.0);
                EmitVertex();
                P = deform(pos[0].xy + vec2(-r, +r));
                gl_Position = vec4(tileOffset.xy + P * tileOffset.z, 0.0, 1.0);
                uv = vec2(0.0, 1.0);
                EmitVertex();
                P = deform(pos[0].xy + vec2(+r, +r));
                gl_Position = vec4(tileOffset.xy + P * tileOffset.z, 0.0, 1.0);
                uv = vec2(1.0, 1.0);
                EmitVertex();
                EndPrimitive();
            }
        }
    }
}

#endif

#ifdef _FRAGMENT_

uniform sampler2D dotSampler;

in vec2 uv;
layout(location=0) out vec4 color;

void main()
{
    color = vec4(0.0, 0.0, texture(dotSampler, uv).r, 0.0);
}

#endif
