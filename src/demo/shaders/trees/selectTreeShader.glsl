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

#include "textureTile.glsl"
#include "globalsShader.glhl"
#include "treeBrdf.glhl"

#ifdef _VERTEX_

uniform mat2 tileDeform;
uniform samplerTile lccSampler;

layout(location=0) in vec3 vertex;
out vec3 uvs;
out vec2 generate;

void main()
{
    uvs = vec3(tileDeform * (vertex.xy - vec2(0.5)) + vec2(0.5), vertex.z);
    generate = vec2(0.0);
    if (uvs.x > 0.0 && uvs.x < 1.0 && uvs.y > 0.0 && uvs.y < 1.0) {
        vec4 lc = textureTile(lccSampler, uvs.xy);
        generate = lc.rg;
    }
}

#endif

#ifdef _GEOMETRY_

layout(points) in;
layout(points,max_vertices=1) out;

uniform vec3 tileOffset;
uniform samplerTile elevationSampler;
uniform samplerTile fragmentNormalSampler;
uniform samplerTile ambientApertureSampler;

in vec3 uvs[];
in vec2 generate[];
out vec3 pos;
out vec3 params;

void main()
{
    if (generate[0].r > 0.25 && unpackUnorm4x8(floatBitsToUint(uvs[0].z)).w <= getTreeDensity()) {
        pos = vec3(uvs[0].xy * tileOffset.z + tileOffset.xy, textureTile(elevationSampler, uvs[0].xy).z - 0.35);
        vec2 normal = textureTile(fragmentNormalSampler, uvs[0].xy).xy;
        vec3 aperture = textureTile(ambientApertureSampler, uvs[0].xy).xyz;
        vec4 seeds = unpackUnorm4x8(floatBitsToUint(uvs[0].z));
        vec3 color = vec3(1.0) + (seeds.rgb - vec3(0.5)) * vec3(0.15,0.15,0.25) * 3.0;
        float border = smoothstep(0.25, 0.35, generate[0].r);
        float minSize = MIN_RADIUS;
        float maxSize = MAX_RADIUS;
        //float minSize = mix(0.3 * MIN_RADIUS, MIN_RADIUS, border);
        //float maxSize = mix(0.8 * MAX_RADIUS, MAX_RADIUS, border);
        float tseed = (mod(generate[0].g, 0.01) / 0.02 - 0.5) * 0.5 + 1.0;
        float tspecies = tseed * generate[0].g < 0.5 * generate[0].r ? 0.5 : 0.0;
        float sizeAndSpecies = mix(minSize, maxSize, dot(seeds, vec4(0.25))) * 0.25 + tspecies;
        float seed = seeds.w;
        params = vec3(uintBitsToFloat(packUnorm2x16(normal)),
                    uintBitsToFloat(packUnorm4x8(vec4(aperture, seed))),
                    uintBitsToFloat(packUnorm4x8(vec4(color, sizeAndSpecies))));
        EmitVertex();
        EndPrimitive();
    }
}

#endif
