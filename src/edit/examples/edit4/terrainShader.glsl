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
#extension GL_ARB_texture_rectangle : enable

struct samplerTile {
    sampler2DArray tilePool; // tile cache
    vec3 tileCoords; // coords of currently selected tile in tile cache (u,v,layer; u,v in [0,1]^2)
    vec3 tileSize; // size of currently selected tile in tile cache (du,dv,d; du,dv in [0,1]^2, d in pixels)
};

// returns content of currently selected tile, at uv coordinates (in [0,1]^2; relatively to this tile)
vec4 textureTile(samplerTile tex, vec2 uv) {
    vec3 uvl = tex.tileCoords + vec3(uv * tex.tileSize.xy, 0.0);
    return texture(tex.tilePool, uvl);
}

uniform struct {
    vec4 offset;
    vec4 camera;
    vec2 blending;
    mat4 localToScreen;
} deformation;

uniform samplerTile elevationSampler;
uniform samplerTile fragmentNormalSampler;
uniform samplerTile orthoSampler;

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

#ifdef _VERTEX_

layout(location=0) in vec3 vertex;
out vec3 p;
out vec2 uv;

void main() {
    vec4 zfc = textureTile(elevationSampler, vertex.xy);

    vec2 v = abs(deformation.camera.xy - vertex.xy);
    float d = max(max(v.x, v.y), deformation.camera.z);
    float blend = clamp((d - deformation.blending.x) / deformation.blending.y, 0.0, 1.0);

    float h = zfc.z * (1.0 - blend) + zfc.y * blend;
    p = vec3(vertex.xy * deformation.offset.z + deformation.offset.xy, h);

    gl_Position = deformation.localToScreen * vec4(p, 1.0);
    uv = vertex.xy;
}

#endif

#ifdef _FRAGMENT_

in vec3 p;
in vec2 uv;
layout(location=0) out vec4 data;

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
    for (int i = 0; i < nLayers; i++) {
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

vec2 advectedSlopes(SpriteList sprites, vec3 p, vec2 st, vec2 gradx, vec2 grady) {
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

void main() {
    data = textureTile(orthoSampler, uv);

    vec3 n = vec3(textureTile(fragmentNormalSampler, uv).xy * 2.0 - 1.0, 0.0);
    n.z = sqrt(max(0.0, 1.0 - dot(n.xy, n.xy)));

    float light = dot(n, normalize(vec3(1.0)));
    data.rgb *= 0.5 + 0.75 * light;

    float riverDepth = textureTile(river.riverSampler, uv).x;
    if (riverDepth > 0.0) {
        SpriteList sprites;
        getSprites(gl_FragCoord.xy / river.screenSize, sprites);

        vec2 gradx = dFdx(p.xy) / river.wave.length;
        vec2 grady = dFdy(p.xy) / river.wave.length;
        vec2 slopes = advectedSlopes(sprites, p, gl_FragCoord.xy, gradx, grady);

        vec3 normal = normalize(vec3(slopes.xy * river.waveSlopeFactor, 1.0));
        data.rgb = normal;
    }

    //data.r += mod(dot(floor(deformation.offset.xy / deformation.offset.z + 0.5), vec2(1.0)), 2.0);
}

#endif
