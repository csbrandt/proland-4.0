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

#include "treeBrdf.glhl"
#include "globalsShader.glhl"

#ifdef TREES
const vec3 treeReflectance0 = vec3(0.02, 0.053, 0.0226) * 0.09;
const vec3 treeReflectance1 = vec3(0.0736, 0.145, 0.0448) * 0.09;

uniform sampler2DArray treeShadowMap;

float shadow1(vec4 coord, float dz)
{
#ifdef ALPHASHADOWMAP
    vec2 za = texture(treeShadowMap, coord.xyz).xy;
#else
    vec2 za = vec2(texture(treeShadowMap, coord.xyz).x, 0.0);
#endif
    float d = (coord.w - za.x) / dz;
    return d < 0.0 || d > 50.0 ? 1.0 : za.y;
}

float shadow(vec4 coord, float dz)
{
    float ts = shadow1(coord, dz);

    vec2 off = 1.0 / vec2(textureSize(treeShadowMap, 0).xy);
    ts += shadow1(vec4(coord.x - off.x, coord.y, coord.zw), dz);
    ts += shadow1(vec4(coord.x + off.x, coord.y, coord.zw), dz);
    ts += shadow1(vec4(coord.x, coord.y - off.y, coord.zw), dz);
    ts += shadow1(vec4(coord.x, coord.y + off.y, coord.zw), dz);
    ts += shadow1(vec4(coord.x - off.x, coord.y - off.y, coord.zw), dz);
    ts += shadow1(vec4(coord.x + off.x, coord.y - off.y, coord.zw), dz);
    ts += shadow1(vec4(coord.x - off.x, coord.y + off.y, coord.zw), dz);
    ts += shadow1(vec4(coord.x + off.x, coord.y + off.y, coord.zw), dz);
    ts /= 9.0;

    return ts;
}
#endif

vec3 treeBrdf(vec3 q, float d, vec4 lc, vec3 v, vec3 fn, vec3 WSD, vec3 V, vec3 reflectance, vec3 sunL, vec3 skyE)
{
    float cTheta = dot(fn, WSD);
#ifdef TREES
    int slice = -1;
    slice += gl_FragCoord.z < getShadowLimit(0) ? 1 : 0;
    slice += gl_FragCoord.z < getShadowLimit(1) ? 1 : 0;
    slice += gl_FragCoord.z < getShadowLimit(2) ? 1 : 0;
    slice += gl_FragCoord.z < getShadowLimit(3) ? 1 : 0;
    if (slice >= 0) {
        mat4 t2s = getTangentFrameToShadow(slice);
        vec3 qs = (t2s * vec4(q, 1.0)).xyz * 0.5 + vec3(0.5);
        cTheta *= shadow(vec4(qs.xy, slice, qs.z), t2s[3][3]);
    }
    vec3 treeReflectance = lc.g / lc.r < 0.5 ? treeReflectance0 : treeReflectance1;
    float dens = min(lc.b / 0.68, 1.0);
    float AO = 1.0 - 0.9 * dens;
    vec3 nearColor = AO * reflectance * (sunL * max(cTheta, 0.0) + skyE) / 3.14159265;
    vec3 treeColor = treeReflectance * (sunL * max(dot(fn, WSD), 0.0) + skyE) / 3.14159265;
    vec3 farColor = mix(nearColor, treeColor, dens);
    float blend = smoothstep(0.8, 1.0, d / getMaxTreeDistance());
    return mix(nearColor, farColor, blend);
#else
    vec3 lightColor = (sunL * max(cTheta, 0.0) + skyE) / 3.14159265;
    return reflectance * lightColor;
#endif
}

#ifdef _FRAGMENT_
#endif
