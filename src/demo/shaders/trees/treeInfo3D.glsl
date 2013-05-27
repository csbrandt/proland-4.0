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

#include "globalsShader.glhl"
#include "treeBrdf.glhl"

#define ALPHASHADOWMAP

//#define MONO_SPECIES

const float M_PI = 3.14159265;

const int N = 9;
const int NT = 2*N*(N+1)+1;

const vec3 treeReflectance0 = vec3(0.02, 0.053, 0.0226);
const vec3 treeReflectance1 = vec3(0.0736, 0.145, 0.0448);

uniform sampler3D treeKcSampler;
uniform sampler2D treeAOSampler;
uniform sampler3D groundCoverSampler;
uniform sampler2DArray treeShadowMap;

// horizontal radius, vertical radius and tree base (relative to horizontal radius)
vec3 getTreeSize(int species)
{
    return vec3(1.0, 0.5 * getTreeHeight(species), getTreeBase(species));
}

float getTreeDensityFactor()
{
    return getTreeDensity();
}

float treeKc(int species, float density, float thetaV, float thetaL, float phi)
{
    const vec3 A = vec3(0.5) / vec3(16.0, 16.0, 128.0);
    const vec3 B = vec3(15.0, 15.0, 127.0) / vec3(16.0, 16.0, 128.0);
    float d = min(density * 7.0, 6.999);
    int l = int(floor(d));
    float u = fract(d);
    vec3 uvw = A + vec3(thetaV / (M_PI / 2.0), thetaL / (M_PI / 2.0), phi / M_PI * 15.0 / 127.0 + l * 16.0 / 127.0) * B;
    vec2 r1 = texture(treeKcSampler, uvw).xy;
    vec2 r2 = texture(treeKcSampler, vec3(uvw.x, uvw.y, uvw.z + 16.0 / 128.0)).xy;
    vec2 r = (1.0 - u) * r1 + u * r2;
    return species == 0 ? r.x : r.y;
}

vec2 groundCover(int species, float density, float thetaV, float thetaL, float phi)
{
    const vec3 A = vec3(0.5) / vec3(16.0, 16.0, 128.0);
    const vec3 B = vec3(15.0, 15.0, 127.0) / vec3(16.0, 16.0, 128.0);
    float d = min(density * 7.0, 6.999);
    int l = int(floor(d));
    float u = fract(d);
    vec3 uvw = A + vec3(thetaV / (M_PI / 2.0), thetaL / (M_PI / 2.0), phi / M_PI * 15.0 / 127.0 + l * 16.0 / 127.0) * B;
    vec4 r1 = texture(groundCoverSampler, uvw);
    vec4 r2 = texture(groundCoverSampler, vec3(uvw.x, uvw.y, uvw.z + 16.0 / 128.0));
    vec4 r = (1.0 - u) * r1 + u * r2;
    return species == 0 ? r.xy : r.zw;
}

float treeAO(int species, float density, float thetaV)
{
    const vec2 A = vec2(0.5) / vec2(16.0, 8.0);
    const vec2 B = vec2(15.0, 7.0) / vec2(16.0, 8.0);
    vec2 uv = vec2(thetaV / (M_PI / 2.0), density);
    vec4 r = texture(treeAOSampler, A + uv * B);
    return species == 0 ? r.x : r.y;
}

float inTreeHotspot(vec3 v, vec3 l)
{
    return 1.0 + 2.0 * exp(-8.0 * acos(-dot(v, l)));
}

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

int viewNumber(int i, int j) {
    return i * ((2 * N + 1) - abs(i)) + j + (N * (N + 1));
}

void findViews(int species, vec3 cDIR, out ivec3 vv, out vec3 rr)
{
    vec3 VDIR = vec3(cDIR.xy, max(cDIR.z, 0.01));
    float a = abs(VDIR.x) > abs(VDIR.y) ? VDIR.y / VDIR.x : -VDIR.x / VDIR.y;
    //float nxx = N * (1.0 - a) * (1.0 - VDIR.z) / 2.0; // uniform sampling in cos(theta)
    //float nyy = N * (1.0 + a) * (1.0 - VDIR.z) / 2.0;
    float nxx = N * (1.0 - a) * acos(VDIR.z) / 3.141592657; // uniform sampling in theta
    float nyy = N * (1.0 + a) * acos(VDIR.z) / 3.141592657;
    int i = int(floor(nxx));
    int j = int(floor(nyy));
    float ti = nxx - i;
    float tj = nyy - j;
    float alpha = 1.0 - ti - tj;
    bool b = alpha > 0.0;
    ivec3 ii = ivec3(b ? i : i + 1, i + 1, i);
    ivec3 jj = ivec3(b ? j : j + 1, j, j + 1);
    rr = vec3(abs(alpha), b ? ti : 1.0 - tj, b ? tj : 1.0 - ti);
    if (abs(VDIR.y) >= abs(VDIR.x)) {
        ivec3 tmp = ii;
        ii = -jj;
        jj = tmp;
    }
    ii *= int(sign(VDIR.x + VDIR.y));
    jj *= int(sign(VDIR.x + VDIR.y));
    vv = ivec3(species * NT) + ivec3(viewNumber(ii.x,jj.x), viewNumber(ii.y,jj.y), viewNumber(ii.z,jj.z));
}

#ifdef MONO_SPECIES
const int species = 0;
#endif
