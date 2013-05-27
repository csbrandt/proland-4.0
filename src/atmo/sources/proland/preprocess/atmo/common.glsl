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

/**
 * Precomputed Atmospheric Scattering
 * Copyright (c) 2008 INRIA
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * Author: Eric Bruneton
 */

namespace proland
{

const char* commonAtmoShader = "\n\
// ----------------------------------------------------------------------------\n\
// PHYSICAL MODEL PARAMETERS\n\
// ----------------------------------------------------------------------------\n\
\n\
#ifdef _FRAGMENT_\n\
uniform float AVERAGE_GROUND_REFLECTANCE;// = 0.1;\n\
\n\
// Rayleigh\n\
uniform float HR;// = 8.0;\n\
uniform vec3 betaR;// = vec3(5.8e-3, 1.35e-2, 3.31e-2);\n\
\n\
// Mie\n\
// DEFAULT\n\
uniform float HM;// = 1.2;\n\
uniform vec3 betaMSca;// = vec3(4e-3);\n\
uniform vec3 betaMEx;// = betaMSca / 0.9;\n\
uniform float mieG;// = 0.8;\n\
// CLEAR SKY\n\
/*const float HM = 1.2;\n\
const vec3 betaMSca = vec3(20e-3);\n\
const vec3 betaMEx = betaMSca / 0.9;\n\
const float mieG = 0.76;*/\n\
// PARTLY CLOUDY\n\
/*const float HM = 3.0;\n\
const vec3 betaMSca = vec3(3e-3);\n\
const vec3 betaMEx = betaMSca / 0.9;\n\
const float mieG = 0.65;*/\n\
\n\
// ----------------------------------------------------------------------------\n\
// NUMERICAL INTEGRATION PARAMETERS\n\
// ----------------------------------------------------------------------------\n\
\n\
const int TRANSMITTANCE_INTEGRAL_SAMPLES = 500;\n\
const int INSCATTER_INTEGRAL_SAMPLES = 50;\n\
const int IRRADIANCE_INTEGRAL_SAMPLES = 32;\n\
const int INSCATTER_SPHERICAL_INTEGRAL_SAMPLES = 16;\n\
\n\
const float M_PI = 3.141592657;\n\
\n\
// ----------------------------------------------------------------------------\n\
// PARAMETERIZATION OPTIONS\n\
// ----------------------------------------------------------------------------\n\
\n\
#define TRANSMITTANCE_NON_LINEAR\n\
#define INSCATTER_NON_LINEAR\n\
\n\
// ----------------------------------------------------------------------------\n\
// PARAMETERIZATION FUNCTIONS\n\
// ----------------------------------------------------------------------------\n\
\n\
uniform sampler2D transmittanceSampler;\n\
\n\
vec2 getTransmittanceUV(float r, float mu) {\n\
    float uR, uMu;\n\
#ifdef TRANSMITTANCE_NON_LINEAR\n\
	uR = sqrt((r - Rg) / (Rt - Rg));\n\
	uMu = atan((mu + 0.15) / (1.0 + 0.15) * tan(1.5)) / 1.5;\n\
#else\n\
	uR = (r - Rg) / (Rt - Rg);\n\
	uMu = (mu + 0.15) / (1.0 + 0.15);\n\
#endif\n\
    return vec2(uMu, uR);\n\
}\n\
\n\
void getTransmittanceRMu(out float r, out float muS) {\n\
    r = gl_FragCoord.y / float(TRANSMITTANCE_H);\n\
    muS = gl_FragCoord.x / float(TRANSMITTANCE_W);\n\
#ifdef TRANSMITTANCE_NON_LINEAR\n\
    r = Rg + (r * r) * (Rt - Rg);\n\
    muS = -0.15 + tan(1.5 * muS) / tan(1.5) * (1.0 + 0.15);\n\
#else\n\
    r = Rg + r * (Rt - Rg);\n\
    muS = -0.15 + muS * (1.0 + 0.15);\n\
#endif\n\
}\n\
\n\
vec2 getIrradianceUV(float r, float muS) {\n\
    float uR = (r - Rg) / (Rt - Rg);\n\
    float uMuS = (muS + 0.2) / (1.0 + 0.2);\n\
    return vec2(uMuS, uR);\n\
}\n\
\n\
void getIrradianceRMuS(out float r, out float muS) {\n\
    r = Rg + (gl_FragCoord.y - 0.5) / (float(SKY_H) - 1.0) * (Rt - Rg);\n\
    muS = -0.2 + (gl_FragCoord.x - 0.5) / (float(SKY_W) - 1.0) * (1.0 + 0.2);\n\
}\n\
\n\
vec4 texture4D(in sampler3D table, float r, float mu, float muS, float nu)\n\
{\n\
    float H = sqrt(Rt * Rt - Rg * Rg);\n\
    float rho = sqrt(r * r - Rg * Rg);\n\
#ifdef INSCATTER_NON_LINEAR\n\
    float rmu = r * mu;\n\
    float delta = rmu * rmu - r * r + Rg * Rg;\n\
    vec4 cst = rmu < 0.0 && delta > 0.0 ? vec4(1.0, 0.0, 0.0, 0.5 - 0.5 / float(RES_MU)) : vec4(-1.0, H * H, H, 0.5 + 0.5 / float(RES_MU));\n\
	float uR = 0.5 / float(RES_R) + rho / H * (1.0 - 1.0 / float(RES_R));\n\
    float uMu = cst.w + (rmu * cst.x + sqrt(delta + cst.y)) / (rho + cst.z) * (0.5 - 1.0 / float(RES_MU));\n\
    // paper formula\n\
    //float uMuS = 0.5 / float(RES_MU_S) + max((1.0 - exp(-3.0 * muS - 0.6)) / (1.0 - exp(-3.6)), 0.0) * (1.0 - 1.0 / float(RES_MU_S));\n\
    // better formula\n\
    float uMuS = 0.5 / float(RES_MU_S) + (atan(max(muS, -0.1975) * tan(1.26 * 1.1)) / 1.1 + (1.0 - 0.26)) * 0.5 * (1.0 - 1.0 / float(RES_MU_S));\n\
#else\n\
	float uR = 0.5 / float(RES_R) + rho / H * (1.0 - 1.0 / float(RES_R));\n\
    float uMu = 0.5 / float(RES_MU) + (mu + 1.0) / 2.0 * (1.0 - 1.0 / float(RES_MU));\n\
    float uMuS = 0.5 / float(RES_MU_S) + max(muS + 0.2, 0.0) / 1.2 * (1.0 - 1.0 / float(RES_MU_S));\n\
#endif\n\
    float lerp = (nu + 1.0) / 2.0 * (float(RES_NU) - 1.0);\n\
    float uNu = floor(lerp);\n\
    lerp = lerp - uNu;\n\
    return texture(table, vec3((uNu + uMuS) / float(RES_NU), uMu, uR)) * (1.0 - lerp) +\n\
           texture(table, vec3((uNu + uMuS + 1.0) / float(RES_NU), uMu, uR)) * lerp;\n\
}\n\
\n\
void getMuMuSNu(float r, vec4 dhdH, out float mu, out float muS, out float nu) {\n\
    float x = gl_FragCoord.x - 0.5;\n\
    float y = gl_FragCoord.y - 0.5;\n\
#ifdef INSCATTER_NON_LINEAR\n\
    if (y < float(RES_MU) / 2.0) {\n\
        float d = 1.0 - y / (float(RES_MU) / 2.0 - 1.0);\n\
        d = min(max(dhdH.z, d * dhdH.w), dhdH.w * 0.999);\n\
        mu = (Rg * Rg - r * r - d * d) / (2.0 * r * d);\n\
        mu = min(mu, -sqrt(1.0 - (Rg / r) * (Rg / r)) - 0.001);\n\
    } else {\n\
        float d = (y - float(RES_MU) / 2.0) / (float(RES_MU) / 2.0 - 1.0);\n\
        d = min(max(dhdH.x, d * dhdH.y), dhdH.y * 0.999);\n\
        mu = (Rt * Rt - r * r - d * d) / (2.0 * r * d);\n\
    }\n\
    muS = mod(x, float(RES_MU_S)) / (float(RES_MU_S) - 1.0);\n\
    // paper formula\n\
    //muS = -(0.6 + log(1.0 - muS * (1.0 -  exp(-3.6)))) / 3.0;\n\
    // better formula\n\
    muS = tan((2.0 * muS - 1.0 + 0.26) * 1.1) / tan(1.26 * 1.1);\n\
    nu = -1.0 + floor(x / float(RES_MU_S)) / (float(RES_NU) - 1.0) * 2.0;\n\
#else\n\
    mu = -1.0 + 2.0 * y / (float(RES_MU) - 1.0);\n\
    muS = mod(x, float(RES_MU_S)) / (float(RES_MU_S) - 1.0);\n\
    muS = -0.2 + muS * 1.2;\n\
    nu = -1.0 + floor(x / float(RES_MU_S)) / (float(RES_NU) - 1.0) * 2.0;\n\
#endif\n\
}\n\
\n\
// ----------------------------------------------------------------------------\n\
// UTILITY FUNCTIONS\n\
// ----------------------------------------------------------------------------\n\
\n\
// nearest intersection of ray r,mu with ground or top atmosphere boundary\n\
// mu=cos(ray zenith angle at ray origin)\n\
float limit(float r, float mu) {\n\
    float dout = -r * mu + sqrt(r * r * (mu * mu - 1.0) + RL * RL);\n\
    float delta2 = r * r * (mu * mu - 1.0) + Rg * Rg;\n\
    if (delta2 >= 0.0) {\n\
        float din = -r * mu - sqrt(delta2);\n\
        if (din >= 0.0) {\n\
            dout = min(dout, din);\n\
        }\n\
    }\n\
    return dout;\n\
}\n\
\n\
// transmittance(=transparency) of atmosphere for infinite ray (r,mu)\n\
// (mu=cos(view zenith angle)), intersections with ground ignored\n\
vec3 transmittance(float r, float mu) {\n\
	vec2 uv = getTransmittanceUV(r, mu);\n\
    return texture(transmittanceSampler, uv).rgb;\n\
}\n\
\n\
// transmittance(=transparency) of atmosphere for infinite ray (r,mu)\n\
// (mu=cos(view zenith angle)), or zero if ray intersects ground\n\
vec3 transmittanceWithShadow(float r, float mu) {\n\
    return mu < -sqrt(1.0 - (Rg / r) * (Rg / r)) ? vec3(0.0) : transmittance(r, mu);\n\
}\n\
\n\
// transmittance(=transparency) of atmosphere between x and x0\n\
// assume segment x,x0 not intersecting ground\n\
// r=||x||, mu=cos(zenith angle of [x,x0) ray at x), v=unit direction vector of [x,x0) ray\n\
vec3 transmittance(float r, float mu, vec3 v, vec3 x0) {\n\
    vec3 result;\n\
    float r1 = length(x0);\n\
    float mu1 = dot(x0, v) / r;\n\
    if (mu > 0.0) {\n\
        result = min(transmittance(r, mu) / transmittance(r1, mu1), 1.0);\n\
    } else {\n\
        result = min(transmittance(r1, -mu1) / transmittance(r, -mu), 1.0);\n\
    }\n\
    return result;\n\
}\n\
\n\
// optical depth for ray (r,mu) of length d, using analytic formula\n\
// (mu=cos(view zenith angle)), intersections with ground ignored\n\
// H=height scale of exponential density function\n\
float opticalDepth(float H, float r, float mu, float d) {\n\
    float a = sqrt((0.5/H)*r);\n\
    vec2 a01 = a*vec2(mu, mu + d / r);\n\
    vec2 a01s = sign(a01);\n\
    vec2 a01sq = a01*a01;\n\
    float x = a01s.y > a01s.x ? exp(a01sq.x) : 0.0;\n\
    vec2 y = a01s / (2.3193*abs(a01) + sqrt(1.52*a01sq + 4.0)) * vec2(1.0, exp(-d/H*(d/(2.0*r)+mu)));\n\
    return sqrt((6.2831*H)*r) * exp((Rg-r)/H) * (x + dot(y, vec2(1.0, -1.0)));\n\
}\n\
\n\
// transmittance(=transparency) of atmosphere for ray (r,mu) of length d\n\
// (mu=cos(view zenith angle)), intersections with ground ignored\n\
// uses analytic formula instead of transmittance texture\n\
vec3 analyticTransmittance(float r, float mu, float d) {\n\
    return exp(- betaR * opticalDepth(HR, r, mu, d) - betaMEx * opticalDepth(HM, r, mu, d));\n\
}\n\
\n\
// transmittance(=transparency) of atmosphere between x and x0\n\
// assume segment x,x0 not intersecting ground\n\
// d = distance between x and x0, mu=cos(zenith angle of [x,x0) ray at x)\n\
vec3 transmittance(float r, float mu, float d) {\n\
    vec3 result;\n\
    float r1 = sqrt(r * r + d * d + 2.0 * r * mu * d);\n\
    float mu1 = (r * mu + d) / r1;\n\
    if (mu > 0.0) {\n\
        result = min(transmittance(r, mu) / transmittance(r1, mu1), 1.0);\n\
    } else {\n\
        result = min(transmittance(r1, -mu1) / transmittance(r, -mu), 1.0);\n\
    }\n\
    return result;\n\
}\n\
\n\
vec3 irradiance(sampler2D sampler, float r, float muS) {\n\
    vec2 uv = getIrradianceUV(r, muS);\n\
    return texture(sampler, uv).rgb;\n\
}\n\
\n\
// Rayleigh phase function\n\
float phaseFunctionR(float mu) {\n\
    return (3.0 / (16.0 * M_PI)) * (1.0 + mu * mu);\n\
}\n\
\n\
// Mie phase function\n\
float phaseFunctionM(float mu) {\n\
	return 1.5 * 1.0 / (4.0 * M_PI) * (1.0 - mieG*mieG) * pow(1.0 + (mieG*mieG) - 2.0*mieG*mu, -3.0/2.0) * (1.0 + mu * mu) / (2.0 + mieG*mieG);\n\
}\n\
\n\
// approximated single Mie scattering (cf. approximate Cm in paragraph 'Angular precision')\n\
vec3 getMie(vec4 rayMie) { // rayMie.rgb=C*, rayMie.w=Cm,r\n\
	return rayMie.rgb * rayMie.w / max(rayMie.r, 1e-4) * (betaR.r / betaR);\n\
}\n\
#endif\n\
";

}
