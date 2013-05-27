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

uniform float maxTreeDistance;
uniform vec3 cameraRefPos;
uniform vec4 clip[4];
uniform mat4 localToTangentFrame;
uniform mat4 tangentFrameToScreen;
uniform mat4 tangentFrameToWorld;
uniform mat3 tangentSpaceToWorld;
uniform vec3 focalPos;

#ifdef _VERTEX_

bool isVisible(mat4 l2s, vec4 c, float r) {
    return dot(c, clip[0]) > - r && dot(c, clip[1]) > - r && dot(c, clip[2]) > - r && dot(c, clip[3]) > - r;
}

layout(location=0) in vec3 lPos;
layout(location=1) in vec3 lParams;
out vec3 gPos;
out vec3 gParams;
out float generate;

void main()
{
    gPos = (localToTangentFrame * vec4(lPos - vec3(cameraRefPos.xy, 0.0), 1.0)).xyz;
    gParams = lParams;
    float seed = unpackUnorm4x8(floatBitsToUint(lParams.z)).x;
    float maxDist = maxTreeDistance * (0.8 + 0.2 * seed);
    generate = length(gPos.xy) < maxDist && isVisible(tangentFrameToScreen, vec4(gPos + vec3(0.0, 0.0, 7.5), 1.0), 7.5) ? 1.0 : 0.0;
}

#endif

#ifdef _GEOMETRY_

layout(points) in;
layout(triangle_strip,max_vertices=4) out;

in vec3 gPos[];
in vec3 gParams[];
in float generate[];

out vec2 uv;
out vec3 lightColor;

void main()
{
    if (generate[0] > 0.5) {
        vec3 wPos = (tangentFrameToWorld * vec4(gPos[0], 1.0)).xyz; // world pos
        vec2 lN = unpackUnorm2x16(floatBitsToUint(gParams[0].x)) * 2.0 - vec2(1.0); // normal in tangent frame
        vec3 wN = (tangentFrameToWorld * vec4(lN, sqrt(max(0.0, 1.0 - dot(lN, lN))), 0.0)).xyz; // world normal
        vec4 colorAndSize = unpackUnorm4x8(floatBitsToUint(gParams[0].y));

        lightColor = colorAndSize.rgb * max(dot(wN, normalize(vec3(1.0))), 0.0) * 5.0;

        vec3 gDir1 = normalize(gPos[0] - vec3(0.0, 0.0, focalPos.z - 30.0));
        vec3 gDir2 = normalize(gPos[0] - focalPos);
        vec3 gDir = vec3(gDir2.xy, clamp(gDir1.z, -0.7, 0.0));
        vec3 gLeft = normalize(cross(vec3(0.0, 0.0, 1.0), gDir));
        vec3 gUp = normalize(cross(gDir, gLeft));

        vec2 SIZE = vec2(4.76, 15.0) * colorAndSize.w * 2.0;
        gLeft *= SIZE.x;
        gUp *= SIZE.y;

        vec3 gBottom = gPos[0] - gUp * (1.0 - colorAndSize.w) / 0.7 * 0.25;
        gl_Position = tangentFrameToScreen * vec4(gBottom + gLeft, 1.0);
        uv = vec2(0.0, 0.0);
        EmitVertex();
        gl_Position = tangentFrameToScreen * vec4(gBottom - gLeft, 1.0);
        uv = vec2(1.0, 0.0);
        EmitVertex();
        gl_Position = tangentFrameToScreen * vec4(gBottom + gLeft + gUp, 1.0);
        uv = vec2(0.0, 1.0);
        EmitVertex();
        gl_Position = tangentFrameToScreen * vec4(gBottom - gLeft + gUp, 1.0);
        uv = vec2(1.0, 1.0);
        EmitVertex();
        EndPrimitive();
    }
}

#endif

#ifdef _FRAGMENT_

uniform sampler2D treeSampler;

in vec2 uv;
in vec3 lightColor;
layout(location=0) out vec4 color;

void main() {
    vec4 reflectance = texture(treeSampler, uv);
    color.rgb = reflectance.rgb * uv.y * uv.y * lightColor;
    color.a = 1.0;
    if (reflectance.a < 0.3) {
        discard;
    }
}

#endif
