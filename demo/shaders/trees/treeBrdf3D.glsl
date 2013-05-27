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
#include "treeInfo3D.glsl"

vec3 treeBrdf(vec3 q, float d, vec4 lc, vec3 v, vec3 fn, vec3 WSD, vec3 V, vec3 reflectance, vec3 sunL, vec3 skyE)
{
    float cTheta = dot(fn, WSD);
    float gshadow = 1.0;
    int slice = -1;
    slice += gl_FragCoord.z < getShadowLimit(0) ? 1 : 0;
    slice += gl_FragCoord.z < getShadowLimit(1) ? 1 : 0;
    slice += gl_FragCoord.z < getShadowLimit(2) ? 1 : 0;
    slice += gl_FragCoord.z < getShadowLimit(3) ? 1 : 0;
    if (slice >= 0) {
        mat4 t2s = getTangentFrameToShadow(slice);
        vec3 qs = (t2s * vec4(q, 1.0)).xyz * 0.5 + vec3(0.5);
        gshadow = shadow(vec4(qs.xy, slice, qs.z), t2s[3][3]);
    }

    float gblend = d / getMaxTreeDistance();

    float treeDens = min(lc.b / 0.68, 1.0);

    if (treeDens > 0.0) {
#ifndef MONO_SPECIES
        float tseed = (mod(lc.g, 0.01) / 0.02 - 0.5) * 0.5 + 1.0;
        int species = tseed * lc.g / lc.r < 0.5 ? 1 : 0;
#endif
        float shear = 2.0 / getTreeHeight(species);
        vec3 sv = -normalize(v + dot(v, V) * (shear - 1.0) * V);
        vec3 sn = normalize(fn + dot(fn, V) * (1.0 / shear - 1.0) * V);
        vec3 sl = normalize(WSD + dot(WSD, V) * (shear - 1.0) * V);
        float treeDensity = getTreeDensityFactor() * dot(sn, V);
        float cThetaV = dot(sv, sn);
        float cThetaL = dot(sl, sn);
        float cPhi = clamp((dot(sv, sl) - cThetaV * cThetaL) / (sqrt(1.0 - cThetaV * cThetaV) * sqrt(1.0 - cThetaL * cThetaL)), -1.0, 1.0);
        float nkc = treeKc(species, treeDensity, acos(cThetaV), acos(cThetaL), acos(cPhi));
        float tao = treeAO(species, treeDensity, acos(cThetaV));
        float gao = exp(-1.7 * treeDens) * (1.0 + 0.75 * (lc.r - treeDens));
        float hs = inTreeHotspot(v, WSD);
        vec2 kgz = groundCover(species, treeDens, acos(cThetaV), acos(cThetaL), acos(cPhi));
        float kg = kgz.x;
        float kgkz = kgz.y;
        float nkg = min(kg / kgkz, 1.0);
        float ikg = mix(gshadow, nkg, smoothstep(0.0, 0.8, gblend));
        vec3 treeReflectance = species == 0 ? treeReflectance0 : treeReflectance1;
        vec3 treeColor = treeReflectance * (sunL * nkc * hs + 2.0 * skyE * tao) / (4.0 * 3.14159265);
        vec3 gColor = reflectance * (sunL * max(cTheta, 0.0) * ikg + skyE * gao) / 3.14159265;
        float it = smoothstep(0.7, 1.0, gblend) * (1.0 - kgkz);
        return gColor * (1.0 - it) + treeColor * it;
    } else {
        float ikg = mix(gshadow, 1.0, smoothstep(0.0, 0.8, gblend));
        float gao = exp(-1.7 * treeDens) * (1.0 + 0.75 * (lc.r - treeDens));
        return reflectance * (sunL * max(cTheta, 0.0) * ikg + skyE * gao) / 3.14159265;
    }
}

#ifdef _FRAGMENT_
#endif
