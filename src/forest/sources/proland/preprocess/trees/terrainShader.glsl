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

const char *terrainShaderSource = "\
#include \"treeInfo3D.glsl\"\n\
\n\
uniform struct {\n\
    vec4 offset;\n\
    vec4 camera;\n\
    vec2 blending;\n\
    mat4 localToScreen;\n\
    mat3 tileToTangent;\n\
} deformation;\n\
\n\
#ifdef _VERTEX_\n\
\n\
layout(location=0) in vec3 vertex;\n\
out vec3 p;\n\
out vec3 q;\n\
out vec2 uv;\n\
\n\
void main() {\n\
    p = vec3(vertex.xy * deformation.offset.z + deformation.offset.xy, 0.0);\n\
    q = vec3((deformation.tileToTangent * vec3(vertex.xy, 1.0)).xy, 0.0);\n\
    uv = vertex.xy;\n\
    gl_Position = deformation.localToScreen * vec4(p, 1.0);\n\
}\n\
\n\
#endif\n\
\n\
#ifdef _FRAGMENT_\n\
\n\
in vec3 p;\n\
in vec3 q;\n\
in vec2 uv;\n\
layout(location=0) out vec4 data;\n\
\n\
uniform float pass;\n\
uniform float plantRadius;\n\
\n\
void main() {\n\
    vec3 WCP = getWorldCameraPos();\n\
    vec3 WSD = getWorldSunDir();\n\
    vec3 v = normalize(p - WCP);\n\
\n\
    float gshadow = 1.0;\n\
    int slice = -1;\n\
    slice += gl_FragCoord.z < getShadowLimit(0) ? 1 : 0;\n\
    slice += gl_FragCoord.z < getShadowLimit(1) ? 1 : 0;\n\
    slice += gl_FragCoord.z < getShadowLimit(2) ? 1 : 0;\n\
    slice += gl_FragCoord.z < getShadowLimit(3) ? 1 : 0;\n\
    if (slice >= 0) {\n\
        mat4 t2s = getTangentFrameToShadow(slice);\n\
        vec3 qs = (t2s * vec4(q, 1.0)).xyz * 0.5 + vec3(0.5);\n\
        gshadow = shadow(vec4(qs.xy, slice, qs.z), t2s[3][3]);\n\
    }\n\
    if (pass == 2.0) {\n\
        gshadow = length(p.xy - vec2(665.0, -364.0)) < plantRadius ? 0.0 : 1.0;\n\
    }\n\
\n\
    data.rgb = vec3(0.0, 0.0, gshadow);\n\
    data.a = 1.0;\n\
}\n\
\n\
#endif\n\
";
