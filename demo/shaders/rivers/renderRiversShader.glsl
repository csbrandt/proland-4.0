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

#extension GL_EXT_gpu_shader4 : require

#include "textureTile.glsl"

uniform sampler2D depthSampler;

uniform struct {
    mat4 screenToLocal;
} terrainInfos[6];

struct waveTile {
    sampler2D patternTex;
    float patternTexSize;
    float time;
    float timeLoop;
    float length;
};

uniform struct {
    float drawMode;
    float displayGrid;
    float enableSunEffects;
    float waveSlopeFactor;
    float depth;
    float useBedTex;
    vec2 screenSize;
    vec2 gridSize;
    isampler2D spriteTable;
    sampler2D spriteParamTable;
    samplerTile riverSampler;
    waveTile wave;
    waveTile bed;
} river;

#define maxRiverParticlesPerCell 16
#define maxRiverParticlesParams 12

float riverParticleRadiusFactor = 3.0;
vec3 riverShallowColor = vec3(0.3, 0.73, 0.63) * 0.2;

float smoothFunc(float x) {
    return  x * x * x * (10.0 + x * (-15.0 + x * 6.0));
}

#ifdef _VERTEX_

layout(location=0) in vec2 vertex;
layout(location=1) in vec2 wPos;
layout(location=2) in vec2 oPos;
layout(location=3) in float intensity;
layout(location=4) in float radius;
layout(location=5) in float terrainId;
out vec2 sPosOut;
out vec2 wPosOut;
out vec2 oPosOut;
out float intensityOut;
out float radiusOut;
out float terrainIdOut;

void main() {
    sPosOut = vertex;
    wPosOut = wPos;
    oPosOut = oPos;
    intensityOut = intensity;
    radiusOut = radius * riverParticleRadiusFactor;
    terrainIdOut = terrainId;
    gl_Position = vec4(sPosOut, 0.0, 1.0);
}

#endif

#ifdef _GEOMETRY_

#extension GL_EXT_geometry_shader4 : enable

layout (points) in;
layout (triangle_strip, max_vertices=4) out;
in vec2 sPosOut[];
in vec2 wPosOut[];
in vec2 oPosOut[];
in float intensityOut[];
in float radiusOut[];
in float terrainIdOut[];
out vec2 sPos;
out vec2 wPos;
out vec2 oPos;
out float intensity;
out float radius;
out float terrainId;

void main() {
    sPos = sPosOut[0];
    wPos = wPosOut[0];
    oPos = oPosOut[0];
    intensity = intensityOut[0];
    radius = radiusOut[0];
    terrainId = terrainIdOut[0];

    if (terrainId > -0.5) {
        vec2 pos = gl_PositionIn[0].xy;
        vec2 p = 2.0 * (pos - vec2(radius)) / river.screenSize - 1.0;
        gl_Position = vec4(p, 0.0, 1.0);
        EmitVertex();
        p = 2.0 * (pos + vec2(radius, -radius)) / river.screenSize - 1.0;
        gl_Position = vec4(p, 0.0, 1.0);
        EmitVertex();
        p = 2.0 * (pos + vec2(-radius, radius)) / river.screenSize - 1.0;
        gl_Position = vec4(p, 0.0, 1.0);
        EmitVertex();
        p = 2.0 * (pos + vec2(radius, radius)) / river.screenSize - 1.0;
        gl_Position = vec4(p, 0.0, 1.0);
        EmitVertex();
    }
    EndPrimitive();
}

#endif

#ifdef _FRAGMENT_

in vec2 sPos;
in vec2 wPos;
in vec2 oPos;
in float intensity;
in float radius;
in float terrainId;
layout(location=0) out vec4 data;

void main() {
    vec4 pos = gl_FragCoord;
    vec2 st = pos.xy;
    pos = 2.0 * vec4(pos.xy / river.screenSize, texture2D(depthSampler, st / river.screenSize).x, 1.0) - 1.0;

    vec4 realWPos = terrainInfos[0].screenToLocal * pos;
    realWPos /= realWPos.w;
    vec2 gradx = dFdx(realWPos.xy) / river.wave.length;
    vec2 grady = dFdy(realWPos.xy) / river.wave.length;
//    gl_FragColor = vec4(pos.z, 0.0, 0.0, 1.0);// / vec4(10, 1200, 1.0, 1.0);
//    gl_FragColor = vec4((bla - 0.9) * 100.0, 0.0, 0.0, 1.0);
//    gl_FragColor = vec4(gradx, 0.0, 1.0);//realWPos / vec4(1200.0, 1200.0, -1.0, 1.0);
////    gl_FragColor = realWPos / realWPos.w;
//    return;

    float d = distance(st, sPos);
    float s = d / (radius);

    vec2 rCoord = (realWPos.xy - wPos) + oPos;
    vec2 slopesVal = textureGrad(river.wave.patternTex, rCoord / river.wave.length, gradx, grady).xy;
    float weight = smoothFunc(clamp(1.0 - s, 0.0, 1.0)) * smoothFunc(intensity);
    data = vec4(slopesVal * weight, weight * weight, 1.0);
}

#endif
