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
#include "textureTile.glsl"

#if !defined(VERTEX_NORMALS) && !defined(FRAGMENT_NORMALS)
#define VERTEX_NORMALS
#endif

uniform samplerTile elevationSampler;
uniform samplerTile vertexNormalSampler;
uniform samplerTile fragmentNormalSampler;
uniform samplerTile orthoSampler;
uniform samplerTile lccSampler;

uniform vec2 time;

uniform struct {
    vec4 offset;
    vec4 camera;
    vec2 blending;
    mat4 localToScreen;
    mat4 screenQuadCorners;
    mat4 screenQuadVerticals;
} deformation;

#ifdef RIVERS

#extension GL_EXT_gpu_shader4 : enable
#extension GL_ARB_texture_rectangle : enable

#include "oceanBrdf.glhl"

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
    sampler2D riverTex;
    samplerTile riverSampler;
    waveTile wave;
    waveTile bed;
} river;

#define maxRiverParticlesPerCell 16
#define maxRiverParticlesParams 8

float riverParticleRadiusFactor = 3.0;
vec3 riverShallowColor = vec3(0.3, 0.73, 0.63) * 0.2;

#endif //RIVERS

#ifdef _VERTEX_

layout(location=0) in vec3 vertex;
out vec3 p;
out vec3 n;
out vec2 uv;

void main() {
    vec4 zfc = textureTile(elevationSampler, vertex.xy);
    vec4 nfc = textureTile(vertexNormalSampler, vertex.xy) * 2.0 - vec4(1.0);

    vec2 v = abs(deformation.camera.xy - vertex.xy);
    float d = max(max(v.x, v.y), deformation.camera.z);
    float blend = clamp((d - deformation.blending.x) / deformation.blending.y, 0.0, 1.0);

    float h = zfc.z * (1.0 - blend) + zfc.y * blend;
    p = vec3(vertex.xy * deformation.offset.z + deformation.offset.xy, h);

#ifdef VERTEX_NORMALS
    vec3 nf = vec3(nfc.xy, sqrt(1.0 - dot(nfc.xy, nfc.xy)));
    vec3 nc = vec3(nfc.zw, sqrt(1.0 - dot(nfc.zw, nfc.zw)));
    n = nf * (1.0 - blend) + nc * blend;
#endif

    mat4 C = deformation.screenQuadCorners;
    mat4 N = deformation.screenQuadVerticals;

    vec4 uvUV = vec4(vertex.xy, vec2(1.0) - vertex.xy);
    vec4 alpha = uvUV.zxzx * uvUV.wwyy;
    gl_Position = (C + h * N) * alpha;

    uv = vertex.xy;
}

#endif

#ifdef _FRAGMENT_

#include "atmosphereShader.glhl"

in vec3 p;
in vec3 n;
in vec2 uv;
layout(location=0) out vec4 data;

#ifdef RIVERS

struct SpriteParam {
    vec2 sPos;
    vec2 wPos;
    vec2 oPos;
    float intensity;
    float radius;
};

struct SpriteList {
    int ids[maxRiverParticlesPerCell];
};

void getSpriteParam(in float id, out SpriteParam param) {
    float params[maxRiverParticlesParams];
    float nRows = ceil(float(maxRiverParticlesParams) / 4.0);
    int j = 0;
    for (float i = 0.0; i < nRows; i++) {
        vec4 v = texelFetch2D(river.spriteParamTable, ivec2(i, id), 0);
        params[j] = v.x;
        params[j + 1] = v.y;
        params[j + 2] = v.z;
        params[j + 3] = v.w;

        j += 4;
    }

    param.sPos = vec2(params[0], params[1]);
    param.wPos = vec2(params[2], params[3]);
    param.oPos = vec2(params[4], params[5]);
    param.intensity = params[6];
    param.radius = params[7] * riverParticleRadiusFactor;
}

void getSprites(in vec2 pos, out SpriteList ids) {
    int nLayers = maxRiverParticlesPerCell / 4;
    ivec2 offset = ivec2(floor(pos * river.gridSize)) * ivec2(nLayers, 1.0);
    int k = 0;
    for (int i = 0; i < nLayers; i++)
    {
        ivec4 v = texelFetch(river.spriteTable, offset + ivec2(i, 0), 0);
        ids.ids[k] = v.x;
        ids.ids[k + 1] = v.y;
        ids.ids[k + 2] = v.z;
        ids.ids[k + 3] = v.w;
        k += 4;
    }
}

float smoothFunc(float x) {
    return  x * x * x * (10.0 + x * (-15.0 + x * 6.0));
}

vec2 advectedSlopes(SpriteList sprites, vec2 st, vec2 gradx, vec2 grady) {
    vec2 sumWeightedSlopes = vec2(0.0);
    float sumWeight = 0.0;

    for (int i = 0; i < maxRiverParticlesPerCell; i++) {
        float spriteId = sprites.ids[i];
        if (spriteId < 0.0) {
            break;
        }

        SpriteParam param;
        getSpriteParam(spriteId, param);

        float d = distance(st, param.sPos);
        float s = d / (param.radius);

        if (s <= 1.0) {
            vec2 rCoord = (p.xy - param.wPos) + param.oPos;
            vec2 slopesVal = texture2DGrad(river.wave.patternTex, rCoord / river.wave.length, gradx, grady).xy;
            float weight = smoothFunc(1.0 - s) * smoothFunc(param.intensity);
            sumWeightedSlopes += slopesVal * weight;
            sumWeight += weight * weight;
        }
    }

    return sumWeight > 0.0 ? sumWeightedSlopes / sqrt(sumWeight) : texture2DGrad(river.wave.patternTex, p.xy / river.wave.length, gradx, grady).xy;
}

#endif //RIVERS

void main() {
    vec3 WCP = getWorldCameraPos();
    vec3 WSD = getWorldSunDir();

    vec3 viewVec = p - WCP;

    vec2 st = gl_FragCoord.xy; //[0 FrameBufferX]x[0 FrameBufferY]
#ifdef VERTEX_NORMALS
    vec3 fn = normalize(n);
#else //ifndef VERTEX_NORMALS
    vec3 fn = vec3(textureTile(fragmentNormalSampler, uv).xy * 2.0 - 1.0, 0.0);
    fn.z = sqrt(max(0.0, 1.0 - dot(fn.xy, fn.xy)));
#endif //VERTEX_NORMALS

    vec4 reflectance = textureTile(orthoSampler, uv);
    reflectance.r = pow(reflectance.r, 3.0);
    reflectance.g = pow(reflectance.g, 3.8);
    reflectance.b = pow(reflectance.b, 3.4);

    vec3 sunL;
    vec3 skyE;
    sunRadianceAndSkyIrradiance(p, fn, WSD, sunL, skyE);
    float cTheta = dot(fn, WSD);

    // diffuse ground color
    vec3 lightColor = (sunL * max(cTheta, 0.0) + skyE) / 3.14159265;
    vec3 groundColor = reflectance.rgb * lightColor;

#ifdef RIVERS
    vec3 riverColor = groundColor;
    if (river.drawMode <= 0.0) {
//            riverColor = vec3(0.0);
    } else {
        float riverDepth = textureTile(river.riverSampler, uv).x;
        if (riverDepth <= 0.0) {
            // not inside river -> do nothing
        } else {
            vec2 gradx = dFdx(p.xy) / river.wave.length;
            vec2 grady = dFdy(p.xy) / river.wave.length;

            // for renormalizing
            int count = 0;
            int spriteCount = 0;
            bool fetchParticles = (river.drawMode < 10.0);
////////////
//    DRAW MODES
//0 = Sprites (previous section)
//1 = Sprites Coverage
//5 = Advected mesh
//10 = Non-advected mesh
            SpriteList sprites;
            if (fetchParticles) {
                getSprites(st / river.screenSize, sprites);
            }
            if (river.drawMode == 1.0) {
                riverColor = vec3(0.0);
////////////
//    SHOWS PARTILES COVERAGE
                bool finished = false;
                // first compute number of particles over a pixel / cell.
                for (int i = 0; i < maxRiverParticlesPerCell; i++) {
                    float spriteId = sprites.ids[i];
                    if (finished) {
                        continue;
                    }
                    if (spriteId < 0.0) {
                        finished = true;
                        continue;
                    }
                    spriteCount++;
                    SpriteParam param;
                    getSpriteParam(spriteId, param);

                    if (length(st - param.sPos) < param.radius) {
                        count++;
                    }
                }
                riverColor += vec3(float(count) / maxRiverParticlesPerCell, 0.0, 0.0);
                if (count == 0) {
                    riverColor.g += 1.0;
                }
                if (spriteCount == maxRiverParticlesPerCell) {
                    riverColor.b += 1.0;
                }
            } else {
/////////////
// COMPUTING ADVECTED NORMAL
                vec2 slopes;
                if (river.drawMode == 6.0) {
                    vec4 col = texture2D(river.riverTex, st / river.screenSize);
                    slopes = col.xy / sqrt(col.z);
                    if (col.z == 0.0 ) {
                        slopes = texture2DGrad(river.wave.patternTex, p.xy / river.wave.length, gradx, grady).xy;
                    }
                } else {
                    slopes = advectedSlopes(sprites, st, gradx, grady);
                }
/////////////
// REAL SHADING PART
//            //    //calculate normal
                vec3 vVec = normalize(-viewVec);
                vec3 normal = normalize(vec3(slopes.xy * river.waveSlopeFactor, 1.0));

                vec3 sunL;
                vec3 skyE;
                vec3 altitudePos = vec3(0.0, 0.0, WCP.z);
                float sigmaSq = max(2e-5, river.waveSlopeFactor * river.waveSlopeFactor * (1e-2 + 0.02 * (1.0 - exp(-10.0 * max(length(gradx), length(grady))))));
                sunRadianceAndSkyIrradiance(p, normal, WSD, sunL, skyE);

                /*if (river.useBedTex > 0.5) {
                    riverColor = riverShallowColor + groundColor * texture2DGrad(river.bed.patternTex, normal.xy * river.depth / 10.0 + p.xy / (river.bed.length / 2.0), gradx, grady).xyz / (dot(n, vVec) * river.depth * riverDepth);
                } else {
                    riverColor = riverShallowColor;
                }*/

                riverColor = oceanRadiance(vVec, normal, WSD, 0.02, sunL, skyE);
            }
//////////////
//    PRINTS THE SPRITEGRID
            if (river.displayGrid > 0.5) {
                vec2 v = abs(st * river.gridSize / river.screenSize - floor(st * river.gridSize / river.screenSize) - 0.5);
                if (v.x > 0.4 || v.y > 0.4) {
                    riverColor.rgb += vec3(0.2, 0.2, 0.2);
                    if (spriteCount == maxRiverParticlesPerCell) {
                        riverColor += vec3(-1, 1, -1);
                    }
                }
            }
        }
    }
    groundColor = riverColor;
#endif //RIVERS

    vec3 extinction;
    vec3 inscatter = inScattering(WCP, p, WSD, extinction, 0.0);

    vec3 finalColor = groundColor * extinction + inscatter;

    data.rgb = hdr(finalColor);
    data.a = 1.0;

    vec4 PEN = getPencil();
    if (length(p.xy - PEN.xy) < PEN.w) {
        data += getPencilColor();
    }

#ifdef QUADTREE_ON
    data.r += mod(dot(floor(deformation.offset.xy / deformation.offset.z + 0.5), vec2(1.0)), 2.0);
#endif
}

#endif
