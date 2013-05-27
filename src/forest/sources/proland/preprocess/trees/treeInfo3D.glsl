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

const char *treeInfoSource = "\
#include \"globalsShader.glhl\"\n\
\n\
const int MAXN = 9;\n\
const int MAXNT = 2*MAXN*(MAXN+1)+1;\n\
\n\
uniform sampler2DArray treeShadowMap;\n\
\n\
bool selectTree(vec3 p, float seed)\n\
{\n\
    float d = getTreeDensity();\n\
    if (d == 0.0) {\n\
        return p.x > -10.0 && p.x < 10.0 && p.y > 50.0 && p.y < 70.0 && seed <= 0.01;\n\
    } else {\n\
        return seed <= d;\n\
    }\n\
}\n\
vec3 shadow0(vec3 coord)\n\
{\n\
    return texture(treeShadowMap, coord.xyz).xyz;\n\
}\n\
\n\
float shadow1(vec4 coord, float dz)\n\
{\n\
    vec2 za = texture(treeShadowMap, coord.xyz).xy;\n\
    float d = (coord.w - za.x) / dz;\n\
    return d < 0.0 ? 1.0 : za.y;\n\
}\n\
\n\
float shadow(vec4 coord, float dz)\n\
{\n\
    float ts = shadow1(coord, dz);\n\
\n\
    vec2 off = 1.0 / vec2(textureSize(treeShadowMap, 0).xy);\n\
    ts += shadow1(vec4(coord.x - off.x, coord.y, coord.zw), dz);\n\
    ts += shadow1(vec4(coord.x + off.x, coord.y, coord.zw), dz);\n\
    ts += shadow1(vec4(coord.x, coord.y - off.y, coord.zw), dz);\n\
    ts += shadow1(vec4(coord.x, coord.y + off.y, coord.zw), dz);\n\
    ts += shadow1(vec4(coord.x - off.x, coord.y - off.y, coord.zw), dz);\n\
    ts += shadow1(vec4(coord.x + off.x, coord.y - off.y, coord.zw), dz);\n\
    ts += shadow1(vec4(coord.x - off.x, coord.y + off.y, coord.zw), dz);\n\
    ts += shadow1(vec4(coord.x + off.x, coord.y + off.y, coord.zw), dz);\n\
    ts /= 9.0;\n\
\n\
    return ts;\n\
}\n\
\n\
int viewNumber(int i, int j) {\n\
    int n = getNViews();\n\
    return i * ((2 * n + 1) - abs(i)) + j + (n * (n + 1));\n\
}\n\
\n\
void findViews(vec3 cDIR, out ivec3 vv, out vec3 rr)\n\
{\n\
    int n = getNViews();\n\
    vec3 VDIR = vec3(cDIR.xy, max(cDIR.z, 0.01));\n\
    float a = abs(VDIR.x) > abs(VDIR.y) ? VDIR.y / VDIR.x : -VDIR.x / VDIR.y;\n\
    float nxx = n * (1.0 - a) * acos(VDIR.z) / 3.141592657;\n\
    float nyy = n * (1.0 + a) * acos(VDIR.z) / 3.141592657;\n\
    int i = int(floor(nxx));\n\
    int j = int(floor(nyy));\n\
    float ti = nxx - i;\n\
    float tj = nyy - j;\n\
    float alpha = 1.0 - ti - tj;\n\
    bool b = alpha > 0.0;\n\
    ivec3 ii = ivec3(b ? i : i + 1, i + 1, i);\n\
    ivec3 jj = ivec3(b ? j : j + 1, j, j + 1);\n\
    rr = vec3(abs(alpha), b ? ti : 1.0 - tj, b ? tj : 1.0 - ti);\n\
    if (abs(VDIR.y) >= abs(VDIR.x)) {\n\
        ivec3 tmp = ii;\n\
        ii = -jj;\n\
        jj = tmp;\n\
    }\n\
    ii *= int(sign(VDIR.x + VDIR.y));\n\
    jj *= int(sign(VDIR.x + VDIR.y));\n\
    vv = ivec3(viewNumber(ii.x,jj.x), viewNumber(ii.y,jj.y), viewNumber(ii.z,jj.z));\n\
}\n\
";
