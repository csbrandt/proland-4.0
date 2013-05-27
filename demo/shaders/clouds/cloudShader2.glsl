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

#include "textureTile.glsl"

uniform float cloudScale;

uniform samplerTile elevationSampler;
uniform samplerTile normalSampler;
uniform samplerTile orthoSampler;

uniform struct {
    vec4 offset;
    vec4 camera;
    vec2 blending;
    mat4 worldToScreen;
    float radius;
    mat3 localToWorld;
    mat4 screenQuadCorners;
    mat4 screenQuadVerticals;
    vec4 screenQuadCornerNorms;
    mat3 tangentFrameToWorld;
} deformation;

const float CLAMP = 0.2;

#ifdef _VERTEX_

layout(location=0) in vec3 vertex;
out vec3 p;
out vec2 uv;
out float z;
out float h;

void main() {
    //vec4 zfc = vec4(0.0, 0.0, 0.0, 1.0);
    //vec4 nfc = vec4(0.0);
    vec4 zfc = textureTile(elevationSampler, vertex.xy);
    zfc.xyz = max((zfc.xyz - vec3(CLAMP * 5000.0)) / (5000.0 * (1.0 - CLAMP)) * cloudScale, 0.0);
    //vec4 nfc = textureTile(normalSampler, Vertex.xy);

    vec2 v = abs(deformation.camera.xy - vertex.xy);
    float d = max(max(v.x, v.y), deformation.camera.z);
    float blend = clamp((d - deformation.blending.x) / deformation.blending.y, 0.0, 1.0);

    float R = deformation.radius;
    mat4 C = deformation.screenQuadCorners;
    mat4 N = deformation.screenQuadVerticals;
    vec4 L = deformation.screenQuadCornerNorms;
    vec3 P = vec3(vertex.xy * deformation.offset.z + deformation.offset.xy, R);

    vec4 uvUV = vec4(vertex.xy, vec2(1.0) - vertex.xy);
    vec4 alpha = uvUV.zxzx * uvUV.wwyy;
    vec4 alphaPrime = alpha * L / dot(alpha, L);

    float h = (zfc.z * (1.0 - blend) + zfc.y * blend) * vertex.z;
    float k = min(length(P) / dot(alpha, L) * 1.0000003, 1.0);
    float hPrime = (h + R * (1.0 - k)) / k;

    gl_Position = (C + hPrime * N) * alphaPrime;

    p = vec3(vertex.xy * deformation.offset.z + deformation.offset.xy, deformation.radius);
    p = (deformation.radius + h) * normalize(deformation.localToWorld * p);

    /*vec3 nf = vec3(nfc.xy, sqrt(1.0 - dot(nfc.xy, nfc.xy)));
    vec3 nc = vec3(nfc.zw, sqrt(1.0 - dot(nfc.zw, nfc.zw)));
    n = deformation.tangentFrameToWorld * (nf * (1.0 - blend) + nc * blend);*/

    uv = vertex.xy;
    z = vertex.z;
}

#endif

#ifdef _FRAGMENT_

#include "globalsShader.glhl"
#include "atmosphereShader.glhl"

uniform float cloudReflectanceFactor;

in vec3 p;
in vec2 uv;
in float z;
in float h;
layout(location=0) out vec4 data;

//-------------------------------------------------------------\\
//                                                             \\
//   Rendu de nuage utilisant l algorithme d Antoine Bouthors  \\
//                                                             \\
//    TODO : Rendu du dessous du nuage                         \\
//                                                             \\
//-------------------------------------------------------------\\

// Quelques definitions de variables constantes
#define PI      3.14159265358979323846
#define N0      3.0e8
#define re      7.0e-6
#define Pf      0.51
#define kappa   PI*N0*re*re
#define kappa_s (1.0-Pf)*kappa
#define R       6360000.0
#define Rp      6365000.0

// Textures utilisees comme conteneur d information
// Textures des parametre pour la transmission et la reflexion 1D
uniform sampler2D Params;

// Phase 1 definit P(\theta_{vl})
uniform sampler1D Phase1;

// Phase 2 definit (P*P)(\theta_{vl}) ou * est la convolution
uniform sampler1D Phase2;

// BlurredTransp
uniform sampler2D BlurredTransp;

float tau(float H) {
    return exp(-kappa_s * H);
}

vec3 I0(float theta, float H_v) {
    return exp(-kappa_s * H_v) * texture(BlurredTransp, vec2(theta / PI, kappa_s * H_v)).rgb;
}

vec3 I12(float theta, float H_l, float H_v, bool reflected) {
    vec3 PTheta = texture(Phase1, theta / PI).rgb + texture(Phase2, theta / PI).rgb;
    float result;
    if (reflected) {
        result = (1.0 - exp(-kappa_s * (H_v + H_l))) / (H_l + H_v);
    } else  {
        result = (exp(-kappa_s * H_v) - exp(-kappa_s * H_l)) / (H_l - H_v);
    }
    return PTheta * H_v * result;
}

float I012(float H, float theta_l, float mu_l, bool reflected) {
    //float l = 0.5 / 9.0 + 8.5 / 9.0 * theta_l / (PI / 2.0);
    float l = 0.5 / 9.0 + 8.5 / 9.0 * max(theta_l, 20.0 / 180.0 * PI) / (PI / 2.0);
    float kH = texture(Params, vec2(l, 2.5 / 5.0)).x * H;
    float t = texture(Params, vec2(l, 3.5 / 5.0)).x;
    float r = texture(Params, vec2(l, 4.5 / 5.0)).x;
    float tau = exp(-kH);
    float tau2 = exp(-2.0 * kH);
    if (reflected) {
        return 0.5 * r * ((1.0 - tau2) + t * (1.0 - (2.0 * kH + 1.0) * tau2)); // R1 + R2
    } else {
        float tau3 = exp(-3.0 * kH);
        return exp(-kappa * H / abs(mu_l)) + t * kH * tau * (1.0 + 0.5 * t * kH) + 0.25 * r * r * (1.0 + (2.0 * kH - 1.0) * tau2) * tau3; // T0 + T1 + T2
    }
}

float Ims(float H, float theta_l, bool reflected) {
    //float l = 0.5 / 9.0 + 8.5 / 9.0 * theta_l / (PI / 2.0);
    float l = 0.5 / 9.0 + 8.5 / 9.0 * max(theta_l, 20.0 / 180.0 * PI) / (PI / 2.0);
    const float beta = 0.9961;
    float b = texture(Params, vec2(l, 0.5 / 5.0)).x;
    float c = texture(Params, vec2(l, 1.5 / 5.0)).x;
    float t = (b + (1.0 - b) * exp(-c * H)) * beta / (H - (H - 1.0) * beta);
    return reflected ? 1.0 - t : t;
}

float I3(float H, float theta_l, float mu_l, bool reflected) {
    float ims = Ims(H, theta_l, reflected);
    float i012 = I012(H, theta_l, mu_l, reflected);
    return (ims - i012) * abs(mu_l) / PI;
}

// Main du fragment shader
void main() {
    vec3 WCP = getWorldCameraPos();
    vec3 WSD = getWorldSunDir();

    vec4 ortho = textureTile(orthoSampler, uv).rrrr;
    ortho.r = (ortho.r - CLAMP) / (1.0 - CLAMP);
    if (ortho.r <= 0.0) {
        discard;
    }

    float H = ortho.r * cloudScale;

    vec3 N = normalize(p);
    vec3 L = normalize(WSD);
    vec3 P = N * max(length(p), deformation.radius);
    vec3 V = normalize(WCP - P);

    float mu_l = dot(N, L);
    float mu_v = dot(N, V);
    float mu_lv = -dot(V, L);

    if (z <= 0.5 && mu_v > 0.0) {
        discard;
    }

    float theta_l = acos(abs(mu_l));
    float theta_lv = acos(mu_lv);

    float H_l = H / abs(mu_l);
    float H_v = H / abs(mu_v);

    vec3 SunColorIntensity;
    vec3 SkyColorIntensity;
    sunRadianceAndSkyIrradiance(P, N, WSD, SunColorIntensity, SkyColorIntensity);

    bool reflected = (length(WCP) > Rp) == (mu_l > 0.0);

    vec3 i0 = mu_v > 0.0 ? vec3(0.0) : I0(theta_lv, H_v);
    vec3 i12 = I12(theta_lv, H_l, H_v, reflected);
    float i3 = I3(H, theta_l, mu_l, reflected);
    float ims = Ims(H, 0.0, length(WCP) > Rp);// mu_v > 0.0);

    vec3 I = SunColorIntensity * (i0 + i12 + vec3(i3));
    vec3 S = SkyColorIntensity * ims / PI;
    vec3 All = I + S;

    All *= ortho.g; // HACK!

    vec3 extinction;
    vec3 inscatter = inScattering(WCP, P, WSD, extinction, 0.0);
    All = All * extinction + inscatter;

    float transparency = 1.0 - tau(H_v);

    data = vec4(hdr(All), transparency);
}

#endif
