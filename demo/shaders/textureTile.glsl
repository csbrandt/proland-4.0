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

// debug option to highlight or hide quadtree quads
#define QUADTREE_OFF

#ifdef GL_EXT_texture_array
#extension GL_EXT_texture_array : enable
#endif

struct samplerTile {
    sampler2DArray tilePool; // tile cache

    // currently selected tile
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
