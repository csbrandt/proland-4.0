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

uniform globals {
    vec3 worldCameraPos;
    vec3 worldSunDir;
    // tone mapping
    float hdrExposure;
    // edition
    vec4 pencil;
    vec4 pencilColor;
    // oceans
    vec3 seaColor;
    float seaRoughness;
    // trees
    float treeHeight1;
    float treeHeight2;
    float treeBase1;
    float treeBase2;
    float treeDensity;
    float maxTreeDistance;
    vec4 shadowLimit;
    mat4 tangentFrameToShadow[4];
};

vec3 getWorldCameraPos() {
    return worldCameraPos;
}

vec3 getWorldSunDir() {
    return worldSunDir;
}

float getHdrExposure() {
    return hdrExposure;
}

vec3 hdr(vec3 L) {
#ifndef NOHDR
    L = L * getHdrExposure();
    L.r = L.r < 1.413 ? pow(L.r * 0.38317, 1.0 / 2.2) : 1.0 - exp(-L.r);
    L.g = L.g < 1.413 ? pow(L.g * 0.38317, 1.0 / 2.2) : 1.0 - exp(-L.g);
    L.b = L.b < 1.413 ? pow(L.b * 0.38317, 1.0 / 2.2) : 1.0 - exp(-L.b);
#endif
    return L;
}

vec4 getPencil() {
    return pencil;
}

vec4 getPencilColor() {
    return pencilColor;
}

vec3 getSeaColor() {
    return seaColor;
}

float getSeaRoughness() {
    return seaRoughness;
}

float getTreeHeight(int species) {
    return species == 0 ? treeHeight1 : treeHeight2;
}

float getTreeBase(int species) {
    return species == 0 ? treeBase1 : treeBase2;
}

float getTreeDensity() {
    return treeDensity;
}

float getMaxTreeDistance() {
    return maxTreeDistance;
}

float getShadowLimit(int i)
{
    return shadowLimit[i];
}

mat4 getTangentFrameToShadow(int i)
{
    return tangentFrameToShadow[i];
}
