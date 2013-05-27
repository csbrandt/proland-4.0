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

// computes deltaJ (line 7 in algorithm 4.1)

namespace proland
{

const char *inscatterSShader = "\n\
uniform float r;\n\
uniform vec4 dhdH;\n\
uniform int layer;\n\
\n\
uniform sampler2D deltaESampler;\n\
uniform sampler3D deltaSRSampler;\n\
uniform sampler3D deltaSMSampler;\n\
uniform float first;\n\
\n\
#ifdef _VERTEX_\n\
\n\
layout(location=0) in vec4 vertex;\n\
\n\
void main() {\n\
    gl_Position = vertex;\n\
}\n\
\n\
#endif\n\
\n\
#ifdef _GEOMETRY_\n\
#extension GL_EXT_geometry_shader4 : enable\n\
\n\
layout(triangles) in;\n\
layout(triangle_strip,max_vertices=3) out;\n\
\n\
void main() {\n\
    gl_Position = gl_PositionIn[0];\n\
    gl_Layer = layer;\n\
    EmitVertex();\n\
    gl_Position = gl_PositionIn[1];\n\
    gl_Layer = layer;\n\
    EmitVertex();\n\
    gl_Position = gl_PositionIn[2];\n\
    gl_Layer = layer;\n\
    EmitVertex();\n\
    EndPrimitive();\n\
}\n\
\n\
#endif\n\
\n\
#ifdef _FRAGMENT_\n\
\n\
layout(location=0) out vec4 data;\n\
\n\
const float dphi = M_PI / float(INSCATTER_SPHERICAL_INTEGRAL_SAMPLES);\n\
const float dtheta = M_PI / float(INSCATTER_SPHERICAL_INTEGRAL_SAMPLES);\n\
\n\
void inscatter(float r, float mu, float muS, float nu, out vec3 raymie) {\n\
    r = clamp(r, Rg, Rt);\n\
    mu = clamp(mu, -1.0, 1.0);\n\
    muS = clamp(muS, -1.0, 1.0);\n\
    float var = sqrt(1.0 - mu * mu) * sqrt(1.0 - muS * muS);\n\
    nu = clamp(nu, muS * mu - var, muS * mu + var);\n\
\n\
    float cthetamin = -sqrt(1.0 - (Rg / r) * (Rg / r));\n\
\n\
    vec3 v = vec3(sqrt(1.0 - mu * mu), 0.0, mu);\n\
    float sx = v.x == 0.0 ? 0.0 : (nu - muS * mu) / v.x;\n\
    vec3 s = vec3(sx, sqrt(max(0.0, 1.0 - sx * sx - muS * muS)), muS);\n\
\n\
    raymie = vec3(0.0);\n\
\n\
    // integral over 4.PI around x with two nested loops over w directions (theta,phi) -- Eq (7)\n\
    for (int itheta = 0; itheta < INSCATTER_SPHERICAL_INTEGRAL_SAMPLES; ++itheta) {\n\
        float theta = (float(itheta) + 0.5) * dtheta;\n\
        float ctheta = cos(theta);\n\
\n\
        float greflectance = 0.0;\n\
        float dground = 0.0;\n\
        vec3 gtransp = vec3(0.0);\n\
        if (ctheta < cthetamin) { // if ground visible in direction w\n\
            // compute transparency gtransp between x and ground\n\
            greflectance = AVERAGE_GROUND_REFLECTANCE / M_PI;\n\
            dground = -r * ctheta - sqrt(r * r * (ctheta * ctheta - 1.0) + Rg * Rg);\n\
            gtransp = transmittance(Rg, -(r * ctheta + dground) / Rg, dground);\n\
        }\n\
\n\
        for (int iphi = 0; iphi < 2 * INSCATTER_SPHERICAL_INTEGRAL_SAMPLES; ++iphi) {\n\
            float phi = (float(iphi) + 0.5) * dphi;\n\
            float dw = dtheta * dphi * sin(theta);\n\
            vec3 w = vec3(cos(phi) * sin(theta), sin(phi) * sin(theta), ctheta);\n\
\n\
            float nu1 = dot(s, w);\n\
            float nu2 = dot(v, w);\n\
            float pr2 = phaseFunctionR(nu2);\n\
            float pm2 = phaseFunctionM(nu2);\n\
\n\
            // compute irradiance received at ground in direction w (if ground visible) =deltaE\n\
            vec3 gnormal = (vec3(0.0, 0.0, r) + dground * w) / Rg;\n\
            vec3 girradiance = irradiance(deltaESampler, Rg, dot(gnormal, s));\n\
\n\
            vec3 raymie1; // light arriving at x from direction w\n\
\n\
            // first term = light reflected from the ground and attenuated before reaching x, =T.alpha/PI.deltaE\n\
            raymie1 = greflectance * girradiance * gtransp;\n\
\n\
            // second term = inscattered light, =deltaS\n\
            if (first == 1.0) {\n\
                // first iteration is special because Rayleigh and Mie were stored separately,\n\
                // without the phase functions factors; they must be reintroduced here\n\
                float pr1 = phaseFunctionR(nu1);\n\
                float pm1 = phaseFunctionM(nu1);\n\
                vec3 ray1 = texture4D(deltaSRSampler, r, w.z, muS, nu1).rgb;\n\
                vec3 mie1 = texture4D(deltaSMSampler, r, w.z, muS, nu1).rgb;\n\
                raymie1 += ray1 * pr1 + mie1 * pm1;\n\
            } else {\n\
                raymie1 += texture4D(deltaSRSampler, r, w.z, muS, nu1).rgb;\n\
            }\n\
\n\
            // light coming from direction w and scattered in direction v\n\
            // = light arriving at x from direction w (raymie1) * SUM(scattering coefficient * phaseFunction)\n\
            // see Eq (7)\n\
            raymie += raymie1 * (betaR * exp(-(r - Rg) / HR) * pr2 + betaMSca * exp(-(r - Rg) / HM) * pm2) * dw;\n\
        }\n\
    }\n\
\n\
    // output raymie = J[T.alpha/PI.deltaE + deltaS] (line 7 in algorithm 4.1)\n\
}\n\
\n\
void main() {\n\
    vec3 raymie;\n\
    float mu, muS, nu;\n\
    getMuMuSNu(r, dhdH, mu, muS, nu);\n\
    inscatter(r, mu, muS, nu, raymie);\n\
    data.rgb = raymie;\n\
}\n\
\n\
#endif\n\
";

}
