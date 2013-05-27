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

const vec2 TILE_MAP_SIZE = vec2(1.0 / 4096.0, 1.0 / 8.0);

struct samplerTile {
    sampler2DArray tilePool; // tile cache
    vec3 tileCoords; // coords of currently selected tile in tile cache (u,v,layer; u,v in [0,1]^2)
    vec3 tileSize; // size of currently selected tile in tile cache (du,dv,d; du,dv in [0,1]^2, d in pixels)

    // tile map info (indirection structure to get tile coords in tile cache from absolute position)
    sampler2D tileMap; // associates tile coords in tile cache (in pixels) to tile id (level, tx, ty) (tx, ty relative to tx,ty of tile under camera)
    vec4 quadInfo; // rootTileSize, splitDistance, k=ceil(splitDistance), 4*k+2
    vec4 poolInfo; // tile size in pixels including borders, border in pixels, tilePool 1/w,1/h
    vec4 camera[6]; // camera xyz, max quadtree level
};

// returns content of currently selected tile, at uv coordinates (in [0,1]^2; relatively to this tile)
vec4 textureTile(samplerTile tex, vec2 uv) {
    vec3 uvl = tex.tileCoords + vec3(uv * tex.tileSize.xy, 0.0);
    return texture(tex.tilePool, uvl);
}

// computes tile coordinates from absolute position p
// tileLXY = level,size,tx,ty of tile corresponding to p
// tileUV = relative uv coordinates of p inside this tile (uv in [0,1]^2)
void textureTileCoords(samplerTile tex, vec4 camera, vec2 p, out vec4 tileLLXY, out vec2 tileUV)
{
    float rootTileSize = tex.quadInfo.x;
    float splitDistance = tex.quadInfo.y;
    // first step: computes tile level (first guess based on distance to camera, splitDistance and rootTileSize)
    vec3 lv = 1.0 + floor(-log2(abs(vec3(p, 0.0) - camera.xyz) / (splitDistance * rootTileSize)));
    float l = min(min(lv.x, min(lv.y, lv.z)), camera.w); // tile level
    // second step: checks that tile is indeed farther away than splitDistance*L. If not tile level is indeed l+1
    float L = rootTileSize / exp2(l); // tile size
    vec2 xy = floor(p / L) * L; // tile lower left corner (absolute position)
    vec4 dv = abs(vec4(xy - camera.xy, xy + vec2(L) - camera.xy)); // distances from camera to tile corners
    float d = max(camera.z, max(min(dv.x, dv.z), min(dv.y, dv.w))); // distance from camera to tile
    vec2 lL = l < camera.w && d < splitDistance * L ? vec2(l + 1.0, L * 0.5) : vec2(l, L); // tile level, tile size
    // third step: computes final tile coordinates
    tileLLXY = vec4(lL.xy, floor(p / lL.y));
    tileUV = mod(p / lL.y, 1.0);
}

// returns content of tile cache from absolute position (i.e. meters from bottom left quadtree corner)
// finds the tile coords from p (using indirection data tileMap), and then returns tile value at p (using tile cache tilePool)
vec4 textureQuadtree(samplerTile tex, vec2 p, float producer) {
    vec4 result;
    vec4 camera = tex.camera[int(producer)];
    vec4 tileLLXY; // tile coords l,size,tx,ty
    vec2 tileUV; // relative coords of p in this tile
    // first step: computes tile coords in absolute quadtree
    p += vec2(tex.quadInfo.x / 2.0);
    textureTileCoords(tex, camera, p, tileLLXY, tileUV);
    if (any(notEqual(floor(p / tex.quadInfo.x), vec2(0.0)))) {
        result = vec4(0.0);
    } else {
        // second step: computes tile coords in tileMap
        float k = tex.quadInfo.z; // 2 * ceil(splitDistance)
        float tileMapU;
        if (k > 4.0) {
            tileMapU = mod(tileLLXY.z + tileLLXY.w * pow(2.0, tileLLXY.x) + (pow(2.0, 2.0 * tileLLXY.x) - 1.0) / 3.0, 4093.0);
        } else {
            float K = tex.quadInfo.w; // 4*k + 2
            vec2 cameraTileXY = 2.0 * floor(camera.xy / tileLLXY.y * 0.5); // coords of tile of same level under camera
            vec2 dXY = tileLLXY.zw - cameraTileXY + vec2(k);
            tileMapU = (tileLLXY.x * K + dXY.y) * K + dXY.x;
        }
        // third step: get tile coords in tile cache (in number of tiles), using indirection map tileMap
        vec2 tileCacheXY = texture(tex.tileMap, (vec2(tileMapU, producer) + vec2(0.5)) * TILE_MAP_SIZE).xy * 255.0;
        // last step: computes coords of p in tile cache and do a lookup in tilePool at this point to get result
        if (tileCacheXY.y == 0.0) {
            result = vec4(0.0);
        } else {
            tileCacheXY.y -= 1.0;
            float s = tex.poolInfo.x;
            float b = tex.poolInfo.y;
            vec2 tileCacheUV = (vec2(b) + tileUV * (s - 2.0 * b)) * tex.poolInfo.zw;
            result = texture(tex.tilePool, vec3(tileCacheUV, tileCacheXY.x + tileCacheXY.y * 256.0));
        }
    }
    return result;
}

uniform struct {
    vec4 offset;
    vec4 camera;
    vec2 blending;
    mat4 localToScreen;
} deformation;

uniform samplerTile elevationSampler;
uniform samplerTile fragmentNormalSampler;
uniform samplerTile cloudsSampler;

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

vec3 getWorldSunDir();

void main() {
    data = vec4(0.6, 0.6, 0.6, 1.0);

    float h = textureTile(elevationSampler, uv).x;
    if (h < 0.1) {
        data = vec4(0.0, 0.0, 0.5, 1.0);
    }

    vec3 n = vec3(textureTile(fragmentNormalSampler, uv).xy * 2.0 - 1.0, 0.0);
    n.z = sqrt(max(0.0, 1.0 - dot(n.xy, n.xy)));

    vec3 l = getWorldSunDir();
    l *= (l.z > 0.0 ? 1.0 : 0.0);

    float light = max(dot(n, l), 0.0);

    if (l.z > 0.001) {
        vec3 cp = p + (1000.0-p.z)/l.z*l;
        float cz = textureQuadtree(cloudsSampler, cp.xy, 0.0).r;
        if (cz > 0.0) {
            light *= 0.2;
        }
    }

    data.rgb *= 0.5 + 0.75 * light;

    //data.r += mod(dot(floor(deformation.offset.xy / deformation.offset.z + 0.5), vec2(1.0)), 2.0);
}

#endif
