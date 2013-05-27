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

#include "proland/ocean/DrawOceanFFTTask.h"

#include "ork/core/Timer.h"
#include "ork/resource/ResourceTemplate.h"
#include "ork/render/FrameBuffer.h"
#include "ork/scenegraph/SceneManager.h"
#include "proland/terrain/TileSampler.h"
#include "proland/math/noise.h"

using namespace std;
using namespace ork;

namespace proland
{

const int PASSES = 8; // number of passes needed for the FFT 6 -> 64, 7 -> 128, 8 -> 256, etc
const int FFT_SIZE = 1 << PASSES; // size of the textures storing the waves in frequency and spatial domains
const int N_SLOPE_VARIANCE = 10;

// ----------------------------------------------------------------------------
// WAVES SPECTRUM GENERATION
// ----------------------------------------------------------------------------

float GRID1_SIZE = 5488.0; // size in meters (i.e. in spatial domain) of the first grid

float GRID2_SIZE = 392.0; // size in meters (i.e. in spatial domain) of the second grid

float GRID3_SIZE = 28.0; // size in meters (i.e. in spatial domain) of the third grid

float GRID4_SIZE = 2.0; // size in meters (i.e. in spatial domain) of the fourth grid

float WIND = 5.0; // wind speed in meters per second (at 10m above surface)

float OMEGA = 0.84; // sea state (inverse wave age)

bool propagate = true; // wave propagation?

float A = 1.0; // wave amplitude factor (should be one)

const float cm = 0.23; // Eq 59

const float km = 370.0; // Eq 59

float *spectrum12 = NULL;

float *spectrum34 = NULL;

float maxSlopeVariance = 0.0;

float sqr(float x)
{
    return x * x;
}

float omega(float k)
{
    return sqrt(9.81 * k * (1.0 + sqr(k / km))); // Eq 24
}

// 1/kx and 1/ky in meters
float spectrum(float kx, float ky, bool omnispectrum = false)
{
    float U10 = WIND;
    float Omega = OMEGA;

    // phase speed
    float k = sqrt(kx * kx + ky * ky);
    float c = omega(k) / k;

    // spectral peak
    float kp = 9.81 * sqr(Omega / U10); // after Eq 3
    float cp = omega(kp) / kp;

    // friction velocity
    float z0 = 3.7e-5 * sqr(U10) / 9.81 * pow(U10 / cp, 0.9f); // Eq 66
    float u_star = 0.41 * U10 / log(10.0 / z0); // Eq 60

    float Lpm = exp(- 5.0 / 4.0 * sqr(kp / k)); // after Eq 3
    float gamma = Omega < 1.0 ? 1.7 : 1.7 + 6.0 * log(Omega); // after Eq 3 // log10 or log??
    float sigma = 0.08 * (1.0 + 4.0 / pow(Omega, 3.0f)); // after Eq 3
    float Gamma = exp(-1.0 / (2.0 * sqr(sigma)) * sqr(sqrt(k / kp) - 1.0));
    float Jp = pow(gamma, Gamma); // Eq 3
    float Fp = Lpm * Jp * exp(- Omega / sqrt(10.0) * (sqrt(k / kp) - 1.0)); // Eq 32
    float alphap = 0.006 * sqrt(Omega); // Eq 34
    float Bl = 0.5 * alphap * cp / c * Fp; // Eq 31

    float alpham = 0.01 * (u_star < cm ? 1.0 + log(u_star / cm) : 1.0 + 3.0 * log(u_star / cm)); // Eq 44
    float Fm = exp(-0.25 * sqr(k / km - 1.0)); // Eq 41
    float Bh = 0.5 * alpham * cm / c * Fm; // Eq 40

    Bh *= Lpm; // bug fix???

    if (omnispectrum) {
        return A * (Bl + Bh) / (k * sqr(k)); // Eq 30
    }

    float a0 = log(2.0) / 4.0; float ap = 4.0; float am = 0.13 * u_star / cm; // Eq 59
    float Delta = tanh(a0 + ap * pow(c / cp, 2.5f) + am * pow(cm / c, 2.5f)); // Eq 57

    float phi = atan2(ky, kx);

    if (propagate) {
        if (kx < 0.0) {
            return 0.0;
        } else {
            Bl *= 2.0;
            Bh *= 2.0;
        }
    }

    return A * (Bl + Bh) * (1.0 + Delta * cos(2.0 * phi)) / (2.0 * M_PI * sqr(sqr(k))); // Eq 67
}

void getSpectrumSample(int i, int j, float lengthScale, float kMin, float *result)
{
    static long seed = 1234;
    float dk = 2.0 * M_PI / lengthScale;
    float kx = i * dk;
    float ky = j * dk;
    if (abs(kx) < kMin && abs(ky) < kMin) {
        result[0] = 0.0;
        result[1] = 0.0;
    } else {
        float S = spectrum(kx, ky);
        float h = sqrt(S / 2.0) * dk;
        float phi = frandom(&seed) * 2.0 * M_PI;
        result[0] = h * cos(phi);
        result[1] = h * sin(phi);
    }
}

// generates the waves spectrum
void generateWavesSpectrum(ptr<Texture2D> spectrum12Tex, ptr<Texture2D> spectrum34Tex)
{
    if (spectrum12 != NULL) {
        delete[] spectrum12;
        delete[] spectrum34;
    }
    spectrum12 = new float[FFT_SIZE * FFT_SIZE * 4];
    spectrum34 = new float[FFT_SIZE * FFT_SIZE * 4];

    for (int y = 0; y < FFT_SIZE; ++y) {
        for (int x = 0; x < FFT_SIZE; ++x) {
            int offset = 4 * (x + y * FFT_SIZE);
            int i = x >= FFT_SIZE / 2 ? x - FFT_SIZE : x;
            int j = y >= FFT_SIZE / 2 ? y - FFT_SIZE : y;
            getSpectrumSample(i, j, GRID1_SIZE, M_PI / GRID1_SIZE, spectrum12 + offset);
            getSpectrumSample(i, j, GRID2_SIZE, M_PI * FFT_SIZE / GRID1_SIZE, spectrum12 + offset + 2);
            getSpectrumSample(i, j, GRID3_SIZE, M_PI * FFT_SIZE / GRID2_SIZE, spectrum34 + offset);
            getSpectrumSample(i, j, GRID4_SIZE, M_PI * FFT_SIZE / GRID3_SIZE, spectrum34 + offset + 2);
        }
    }

    spectrum12Tex->setSubImage(0, 0, 0, FFT_SIZE, FFT_SIZE, RGBA, FLOAT, Buffer::Parameters(), CPUBuffer(spectrum12));
    spectrum34Tex->setSubImage(0, 0, 0, FFT_SIZE, FFT_SIZE, RGBA, FLOAT, Buffer::Parameters(), CPUBuffer(spectrum34));
}

float getSlopeVariance(float kx, float ky, float *spectrumSample)
{
    float kSquare = kx * kx + ky * ky;
    float real = spectrumSample[0];
    float img = spectrumSample[1];
    float hSquare = real * real + img * img;
    return kSquare * hSquare * 2.0;
}

// precomputes filtered slope variances in a 3d texture, based on the wave spectrum
void computeSlopeVariances(ptr<FrameBuffer> fbo, ptr<Program> variances, ptr<Texture3D> variancesTex)
{
    // slope variance due to all waves, by integrating over the full spectrum
    float theoreticSlopeVariance = 0.0;
    float k = 5e-3;
    while (k < 1e3) {
        float nextK = k * 1.001;
        theoreticSlopeVariance += k * k * spectrum(k, 0, true) * (nextK - k);
        k = nextK;
    }

    // slope variance due to waves, by integrating over the spectrum part
    // that is covered by the four nested grids. This can give a smaller result
    // than the theoretic total slope variance, because the higher frequencies
    // may not be covered by the four nested grid. Hence the difference between
    // the two is added as a "delta" slope variance in the "variances" shader,
    // to be sure not to lose the variance due to missing wave frequencies in
    // the four nested grids
    float totalSlopeVariance = 0.0;
    for (int y = 0; y < FFT_SIZE; ++y) {
        for (int x = 0; x < FFT_SIZE; ++x) {
            int offset = 4 * (x + y * FFT_SIZE);
            float i = 2.0 * M_PI * (x >= FFT_SIZE / 2 ? x - FFT_SIZE : x);
            float j = 2.0 * M_PI * (y >= FFT_SIZE / 2 ? y - FFT_SIZE : y);
            totalSlopeVariance += getSlopeVariance(i / GRID1_SIZE, j / GRID1_SIZE, spectrum12 + offset);
            totalSlopeVariance += getSlopeVariance(i / GRID2_SIZE, j / GRID2_SIZE, spectrum12 + offset + 2);
            totalSlopeVariance += getSlopeVariance(i / GRID3_SIZE, j / GRID3_SIZE, spectrum34 + offset);
            totalSlopeVariance += getSlopeVariance(i / GRID4_SIZE, j / GRID4_SIZE, spectrum34 + offset + 2);
        }
    }

    variances->getUniform4f("GRID_SIZES")->set(vec4f(GRID1_SIZE, GRID2_SIZE, GRID3_SIZE, GRID4_SIZE));
    variances->getUniform1f("slopeVarianceDelta")->set(theoreticSlopeVariance - totalSlopeVariance);

    for (int layer = 0; layer < N_SLOPE_VARIANCE; ++layer) {
        fbo->setTextureBuffer(COLOR0, variancesTex, 0, layer);
        variances->getUniform1f("c")->set(layer);
        fbo->drawQuad(variances);
    }

    maxSlopeVariance = 0.0;
    float *data = new float[N_SLOPE_VARIANCE * N_SLOPE_VARIANCE * N_SLOPE_VARIANCE];
    variancesTex->getImage(0, RED, FLOAT, data);
    for (int i = 0; i < N_SLOPE_VARIANCE * N_SLOPE_VARIANCE * N_SLOPE_VARIANCE; ++i) {
        maxSlopeVariance = max(maxSlopeVariance, data[i]);
    }
    delete[] data;
}

// ----------------------------------------------------------------------------
// WAVES GENERATION AND ANIMATION (using FFT on GPU)
// ----------------------------------------------------------------------------

int bitReverse(int i, int N)
{
	int j = i;
	int M = N;
	int Sum = 0;
	int W = 1;
	M = M / 2;
	while (M != 0) {
		j = (i & M) > M - 1;
		Sum += j * W;
		W *= 2;
		M = M / 2;
	}
	return Sum;
}

void computeWeight(int N, int k, float &Wr, float &Wi)
{
	Wr = cosl(2.0 * M_PI * k / float(N));
	Wi = sinl(2.0 * M_PI * k / float(N));
}

float *computeButterflyLookupTexture()
{
    float *data = new float[FFT_SIZE * PASSES * 4];

	for (int i = 0; i < PASSES; i++) {
		int nBlocks  = (int) powf(2.0, float(PASSES - 1 - i));
		int nHInputs = (int) powf(2.0, float(i));
		for (int j = 0; j < nBlocks; j++) {
			for (int k = 0; k < nHInputs; k++) {
			    int i1, i2, j1, j2;
				if (i == 0) {
					i1 = j * nHInputs * 2 + k;
					i2 = j * nHInputs * 2 + nHInputs + k;
					j1 = bitReverse(i1, FFT_SIZE);
					j2 = bitReverse(i2, FFT_SIZE);
				} else {
					i1 = j * nHInputs * 2 + k;
					i2 = j * nHInputs * 2 + nHInputs + k;
					j1 = i1;
					j2 = i2;
				}

				float wr, wi;
				computeWeight(FFT_SIZE, k * nBlocks, wr, wi);

                int offset1 = 4 * (i1 + i * FFT_SIZE);
                data[offset1 + 0] = (j1 + 0.5) / FFT_SIZE;
                data[offset1 + 1] = (j2 + 0.5) / FFT_SIZE;
                data[offset1 + 2] = wr;
                data[offset1 + 3] = wi;

                int offset2 = 4 * (i2 + i * FFT_SIZE);
                data[offset2 + 0] = (j1 + 0.5) / FFT_SIZE;
                data[offset2 + 1] = (j2 + 0.5) / FFT_SIZE;
                data[offset2 + 2] = -wr;
                data[offset2 + 3] = -wi;
			}
		}
	}

	return data;
}

void DrawOceanFFTTask::simulateFFTWaves(float t)
{
    // init
    fftInit->getUniform1f("t")->set(t);
    fftFbo1->drawQuad(fftInit);

    // fft passes
    for (int i = 0; i < PASSES; ++i) {
        fftx->getUniform1f("pass")->set(float(i + 0.5) / PASSES);
        if (i%2 == 0) {
            fftx->getUniformSampler("imgSampler")->set(ffta);
            fftFbo2->setDrawBuffer(COLOR1);
        } else {
            fftx->getUniformSampler("imgSampler")->set(fftb);
            fftFbo2->setDrawBuffer(COLOR0);
        }
        fftFbo2->drawQuad(fftx);
    }
    for (int i = PASSES; i < 2 * PASSES; ++i) {
        ffty->getUniform1f("pass")->set(float(i - PASSES + 0.5) / PASSES);
        if (i%2 == 0) {
            ffty->getUniformSampler("imgSampler")->set(ffta);
            fftFbo2->setDrawBuffer(COLOR1);
        } else {
            ffty->getUniformSampler("imgSampler")->set(fftb);
            fftFbo2->setDrawBuffer(COLOR0);
        }
        fftFbo2->drawQuad(ffty);
    }

    ffta->generateMipMap();
}

// ----------------------------------------------------------------------------
// DRAW OCEAN TASK
// ----------------------------------------------------------------------------

DrawOceanFFTTask::DrawOceanFFTTask() : AbstractTask("DrawOceanFFTTask")
{
}

DrawOceanFFTTask::DrawOceanFFTTask(float radius, float zmin,
    ptr<Program> fftInit, ptr<Program> fftx, ptr<Program> ffty, ptr<Program> variances,
    ptr<Module> brdfShader) : AbstractTask("DrawOceanFFTTask")
{
    init(radius, zmin, fftInit, fftx, ffty, variances, brdfShader);
}

void DrawOceanFFTTask::init(float radius, float zmin,
    ptr<Program> fftInit, ptr<Program> fftx, ptr<Program> ffty, ptr<Program> variances,
    ptr<Module> brdfShader)
{
    this->radius = radius;
    this->zmin = zmin;
    this->seaColor = vec3f(10.f / 255.f, 40.f / 255.f, 120.f / 255.f) * 0.1f;

    this->fftInit = fftInit;
    this->fftx = fftx;
    this->ffty = ffty;
    this->variances = variances;

    spectrum12 = new Texture2D(FFT_SIZE, FFT_SIZE, RGBA16F, RGBA, FLOAT,
        Texture::Parameters().min(NEAREST).mag(NEAREST).wrapS(REPEAT).wrapT(REPEAT),
        Buffer::Parameters(), CPUBuffer(NULL));
    spectrum34 = new Texture2D(FFT_SIZE, FFT_SIZE, RGBA16F, RGBA, FLOAT,
        Texture::Parameters().min(NEAREST).mag(NEAREST).wrapS(REPEAT).wrapT(REPEAT),
        Buffer::Parameters(), CPUBuffer(NULL));
    slopeVariances = new Texture3D(N_SLOPE_VARIANCE, N_SLOPE_VARIANCE, N_SLOPE_VARIANCE, R16F, RED, FLOAT,
        Texture::Parameters().min(NEAREST).mag(NEAREST).wrapS(CLAMP_TO_EDGE).wrapT(CLAMP_TO_EDGE).wrapR(CLAMP_TO_EDGE),
        Buffer::Parameters(), CPUBuffer(NULL));
    ffta = new Texture2DArray(FFT_SIZE, FFT_SIZE, 5, RGBA16F, RGBA, FLOAT,
        Texture::Parameters().min(LINEAR_MIPMAP_LINEAR).mag(LINEAR).wrapS(REPEAT).wrapT(REPEAT).maxAnisotropyEXT(16.0f),
        Buffer::Parameters(), CPUBuffer(NULL));
    fftb = new Texture2DArray(FFT_SIZE, FFT_SIZE, 5, RGBA16F, RGBA, FLOAT,
        Texture::Parameters().min(LINEAR_MIPMAP_LINEAR).mag(LINEAR).wrapS(REPEAT).wrapT(REPEAT).maxAnisotropyEXT(16.0f),
        Buffer::Parameters(), CPUBuffer(NULL));
    float *data = computeButterflyLookupTexture();
    ptr<Texture2D> butterfly = new Texture2D(FFT_SIZE, PASSES, RGBA16F, RGBA, FLOAT,
        Texture::Parameters().min(NEAREST).mag(NEAREST).wrapS(CLAMP_TO_EDGE).wrapT(CLAMP_TO_EDGE),
        Buffer::Parameters(), CPUBuffer(data));
    delete[] data;
    fftFbo1 = new FrameBuffer();
    fftFbo1->setTextureBuffer(COLOR0, ffta, 0, 0);
    fftFbo1->setTextureBuffer(COLOR1, ffta, 0, 1);
    fftFbo1->setTextureBuffer(COLOR2, ffta, 0, 2);
    fftFbo1->setTextureBuffer(COLOR3, ffta, 0, 3);
    fftFbo1->setTextureBuffer(COLOR4, ffta, 0, 4);
    fftFbo1->setDrawBuffers(BufferId(COLOR0 | COLOR1 | COLOR2 | COLOR3 | COLOR4));
    fftFbo1->setViewport(vec4<GLint>(0, 0, FFT_SIZE, FFT_SIZE));
    fftFbo2 = new FrameBuffer();
    fftFbo2->setTextureBuffer(COLOR0, ffta, 0, -1);
    fftFbo2->setTextureBuffer(COLOR1, fftb, 0, -1);
    fftFbo2->setViewport(vec4<GLint>(0, 0, FFT_SIZE, FFT_SIZE));
    variancesFbo = new FrameBuffer();
    variancesFbo->setViewport(vec4<GLint>(0, 0, N_SLOPE_VARIANCE, N_SLOPE_VARIANCE));

    fftInit->getUniformSampler("spectrum_1_2_Sampler")->set(spectrum12);
    fftInit->getUniformSampler("spectrum_3_4_Sampler")->set(spectrum34);
    fftInit->getUniform1f("FFT_SIZE")->set(FFT_SIZE);
    fftInit->getUniform4f("INVERSE_GRID_SIZES")->set(vec4f(
        2.0 * M_PI * FFT_SIZE / GRID1_SIZE,
        2.0 * M_PI * FFT_SIZE / GRID2_SIZE,
        2.0 * M_PI * FFT_SIZE / GRID3_SIZE,
        2.0 * M_PI * FFT_SIZE / GRID4_SIZE));

    fftx->getUniform1i("nLayers")->set(5);
    fftx->getUniformSampler("butterflySampler")->set(butterfly);
    ffty->getUniform1i("nLayers")->set(5);
    ffty->getUniformSampler("butterflySampler")->set(butterfly);

    generateWavesSpectrum(spectrum12, spectrum34);

    if (variances != NULL) {
        variances->getUniformSampler("spectrum_1_2_Sampler")->set(spectrum12);
        variances->getUniformSampler("spectrum_3_4_Sampler")->set(spectrum34);
        variances->getUniform1i("FFT_SIZE")->set(FFT_SIZE);
        variances->getUniform1f("N_SLOPE_VARIANCE")->set(N_SLOPE_VARIANCE);
        computeSlopeVariances(variancesFbo, variances, slopeVariances);
    }

    this->resolution = 8;
    this->oldLtoo = mat4d::IDENTITY;
    this->offset = vec3d::ZERO;
    this->brdfShader = brdfShader;
}

DrawOceanFFTTask::~DrawOceanFFTTask()
{
}

ptr<Task> DrawOceanFFTTask::getTask(ptr<Object> context)
{
    ptr<SceneNode> n = context.cast<Method>()->getOwner();
    return new Impl(n, this);
}

void DrawOceanFFTTask::swap(ptr<DrawOceanFFTTask> t)
{
    std::swap(*this, *t);
}

DrawOceanFFTTask::Impl::Impl(ptr<SceneNode> n, ptr<DrawOceanFFTTask> owner) :
    Task("DrawOcean", true, 0), n(n), o(owner)
{
}

DrawOceanFFTTask::Impl::~Impl()
{
}

bool DrawOceanFFTTask::Impl::run()
{
    if (Logger::DEBUG_LOGGER != NULL) {
        Logger::DEBUG_LOGGER->log("OCEAN", "DrawOcean");
    }
    ptr<FrameBuffer> fb = SceneManager::getCurrentFrameBuffer();
    ptr<Program> prog = SceneManager::getCurrentProgram();

    if (o->cameraToOceanU == NULL) {
        o->cameraToOceanU = prog->getUniformMatrix4f("cameraToOcean");
        o->screenToCameraU = prog->getUniformMatrix4f("screenToCamera");
        o->cameraToScreenU = prog->getUniformMatrix4f("cameraToScreen");
        o->oceanToCameraU = prog->getUniformMatrix3f("oceanToCamera");
        o->oceanToWorldU = prog->getUniformMatrix4f("oceanToWorld");
        o->oceanCameraPosU = prog->getUniform3f("oceanCameraPos");
        o->oceanSunDirU = prog->getUniform3f("oceanSunDir");
        o->horizon1U = prog->getUniform3f("horizon1");
        o->horizon2U = prog->getUniform3f("horizon2");
        o->radiusU = prog->getUniform1f("radius");
        o->heightOffsetU = prog->getUniform1f("heightOffset");
        o->gridSizeU = prog->getUniform2f("gridSize");
        prog->getUniformSampler("fftWavesSampler")->set(o->ffta);
        if (prog->getUniformSampler("slopeVarianceSampler") != NULL) {
            prog->getUniformSampler("slopeVarianceSampler")->set(o->slopeVariances);
        }
        prog->getUniform4f("GRID_SIZES")->set(vec4f(GRID1_SIZE, GRID2_SIZE, GRID3_SIZE, GRID4_SIZE));

        if (o->brdfShader != NULL) {
            assert(!o->brdfShader->getUsers().empty());
            Program *p = *(o->brdfShader->getUsers().begin());
            ptr<Uniform1f> u = p->getUniform1f("seaRoughness");
            if (u != NULL) {
                u->set(maxSlopeVariance);
            }
            p->getUniform3f("seaColor")->set(o->seaColor);
        }
    }

    vector< ptr<TileSampler> > uniforms;
    SceneNode::FieldIterator ui = n->getFields();
    while (ui.hasNext()) {
        ptr<TileSampler> u = ui.next().cast<TileSampler>();
        if (u != NULL && u->getTerrain(0) != NULL) {
            u->setTileMap();
        }
    }

    // compute ltoo = localToOcean transform, where ocean frame = tangent space at
    // camera projection on sphere o->radius in local space
    mat4d ctol = n->getLocalToCamera().inverse();
    vec3d cl = ctol * vec3d::ZERO; // camera in local space

    if ((o->radius == 0.0 && cl.z > o->zmin) ||
        (o->radius > 0.0 && cl.length() > o->radius + o->zmin) ||
        (o->radius < 0.0 && vec2d(cl.y, cl.z).length() < -o->radius - o->zmin))
    {
        o->oldLtoo = mat4d::IDENTITY;
        o->offset = vec3d::ZERO;
        return true;
    }

    vec3d ux, uy, uz, oo;

    if (o->radius == 0.0) {
        // flat ocean
        ux = vec3d::UNIT_X;
        uy = vec3d::UNIT_Y;
        uz = vec3d::UNIT_Z;
        oo = vec3d(cl.x, cl.y, 0.0);
    } else if (o->radius > 0.0) {
        // spherical ocean
        uz = cl.normalize(); // unit z vector of ocean frame, in local space
        if (o->oldLtoo != mat4d::IDENTITY) {
            ux = vec3d(o->oldLtoo[1][0], o->oldLtoo[1][1], o->oldLtoo[1][2]).crossProduct(uz).normalize();
        } else {
            ux = vec3d::UNIT_Z.crossProduct(uz).normalize();
        }
        uy = uz.crossProduct(ux); // unit y vector
        oo = uz * o->radius; // origin of ocean frame, in local space
    } else {
        // cylindrical ocean
        uz = vec3d(0.0, -cl.y, -cl.z).normalize();
        ux = vec3d::UNIT_X;
        uy = uz.crossProduct(ux);
        oo = vec3d(cl.x, 0.0, 0.0) + uz * o->radius;
    }

    mat4d ltoo = mat4d(
        ux.x, ux.y, ux.z, -ux.dotproduct(oo),
        uy.x, uy.y, uy.z, -uy.dotproduct(oo),
        uz.x, uz.y, uz.z, -uz.dotproduct(oo),
        0.0,  0.0,  0.0,  1.0);
    // compute ctoo = cameraToOcean transform
    mat4d ctoo = ltoo * ctol;

    if (o->oldLtoo != mat4d::IDENTITY) {
        vec3d delta = ltoo * (o->oldLtoo.inverse() * vec3d::ZERO);
        o->offset += delta;
    }
    o->oldLtoo = ltoo;

    mat4d ctos = n->getOwner()->getCameraToScreen();
    mat4d stoc = ctos.inverse();
    vec3d oc = ctoo * vec3d::ZERO;

    if (o->oceanSunDirU != NULL) {
        // TODO how to get sun dir in a better way?
        SceneManager::NodeIterator i = n->getOwner()->getNodes("light");
        if (i.hasNext()) {
            ptr<SceneNode> l = i.next();
            vec3d worldSunDir = l->getLocalToParent() * vec3d::ZERO;
            vec3d oceanSunDir = ltoo.mat3x3() * (n->getWorldToLocal().mat3x3() * worldSunDir);
            o->oceanSunDirU->set(oceanSunDir.cast<float>());
        }
    }

    vec4<GLint> screen = fb->getViewport();

    o->cameraToOceanU->setMatrix(ctoo.cast<float>());
    o->screenToCameraU->setMatrix(stoc.cast<float>());
    o->cameraToScreenU->setMatrix(ctos.cast<float>());
    o->oceanToCameraU->setMatrix(ctoo.inverse().mat3x3().cast<float>());
    o->oceanCameraPosU->set(vec3f(float(-o->offset.x), float(-o->offset.y), float(oc.z)));
    if (o->oceanToWorldU != NULL) {
        o->oceanToWorldU->setMatrix((n->getLocalToWorld() * ltoo.inverse()).cast<float>());
    }

    if (o->horizon1U != NULL) {
        float h = oc.z;
        vec3d A0 = (ctoo * vec4d((stoc * vec4d(0.0, 0.0, 0.0, 1.0)).xyz(), 0.0)).xyz();
        vec3d dA = (ctoo * vec4d((stoc * vec4d(1.0, 0.0, 0.0, 0.0)).xyz(), 0.0)).xyz();
        vec3d B = (ctoo * vec4d((stoc * vec4d(0.0, 1.0, 0.0, 0.0)).xyz(), 0.0)).xyz();
        if (o->radius == 0.0) {
            o->horizon1U->set(vec3f(-(h * 1e-6 + A0.z) / B.z, -dA.z / B.z, 0.0));
            o->horizon2U->set(vec3f::ZERO);
        } else {
            double h1 = h * (h + 2.0 * o->radius);
            double h2 = (h + o->radius) * (h + o->radius);
            double alpha = B.dotproduct(B) * h1 - B.z * B.z * h2;
            double beta0 = (A0.dotproduct(B) * h1 - B.z * A0.z * h2) / alpha;
            double beta1 = (dA.dotproduct(B) * h1 - B.z * dA.z * h2) / alpha;
            double gamma0 = (A0.dotproduct(A0) * h1 - A0.z * A0.z * h2) / alpha;
            double gamma1 = (A0.dotproduct(dA) * h1 - A0.z * dA.z * h2) / alpha;
            double gamma2 = (dA.dotproduct(dA) * h1 - dA.z * dA.z * h2) / alpha;
            o->horizon1U->set(vec3f(-beta0, -beta1, 0.0));
            o->horizon2U->set(vec3f(beta0 * beta0 - gamma0, 2.0 * (beta0 * beta1 - gamma1), beta1 * beta1 - gamma2));
        }
    }

    if (o->radiusU != NULL) {
        o->radiusU->set(o->radius < 0.0 ? -o->radius : o->radius);
    }
    o->heightOffsetU->set(0.0);
    o->gridSizeU->set(vec2f(o->resolution / float(o->screenWidth), o->resolution / float(o->screenHeight)));

    if (o->screenGrid == NULL || o->screenWidth != screen.z || o->screenHeight != screen.w) {
        o->screenWidth = screen.z;
        o->screenHeight = screen.w;
        o->screenGrid = new Mesh<vec2f, unsigned int>(TRIANGLES, GPU_STATIC);
        o->screenGrid->addAttributeType(0, 2, A32F, false);

        float f = 1.25f;
        int NX = int(f * screen.z / o->resolution);
        int NY = int(f * screen.w / o->resolution);
        for (int i = 0; i < NY; ++i) {
            for (int j = 0; j < NX; ++j) {
                o->screenGrid->addVertex(vec2f(2.0*f*j/(NX-1.0f)-f, 2.0*f*i/(NY-1.0f)-f));
            }
        }
        for (int i = 0; i < NY-1; ++i) {
            for (int j = 0; j < NX-1; ++j) {
                o->screenGrid->addIndice(i*NX+j);
                o->screenGrid->addIndice(i*NX+j+1);
                o->screenGrid->addIndice((i+1)*NX+j);
                o->screenGrid->addIndice((i+1)*NX+j);
                o->screenGrid->addIndice(i*NX+j+1);
                o->screenGrid->addIndice((i+1)*NX+j+1);
            }
        }
    }

    o->simulateFFTWaves(n->getOwner()->getTime() * 1e-6);

    fb->draw(prog, *(o->screenGrid));

    return true;
}

class DrawOceanFFTTaskResource : public ResourceTemplate<40, DrawOceanFFTTask>
{
public:
    DrawOceanFFTTaskResource(ptr<ResourceManager> manager, const string &name, ptr<ResourceDescriptor> desc, const TiXmlElement *e = NULL) :
        ResourceTemplate<40, DrawOceanFFTTask>(manager, name, desc)
    {
        e = e == NULL ? desc->descriptor : e;
        checkParameters(desc, e, "name,radius,zmin,brdfShader,");
        ptr<Program> variances;
        ptr<Module> brdfShader;
        float radius;
        float zmin;
        ptr<Program> fftInit = manager->loadResource("fftInitShader;").cast<Program>();
        ptr<Program> fftx = manager->loadResource("fftxShader;").cast<Program>();
        ptr<Program> ffty = manager->loadResource("fftyShader;").cast<Program>();
        if (e->Attribute("brdfShader") != NULL) {
            variances = manager->loadResource("variancesShader;").cast<Program>();
            brdfShader = manager->loadResource(getParameter(desc, e, "brdfShader")).cast<Module>();
        }
        getFloatParameter(desc, e, "radius", &radius);
        getFloatParameter(desc, e, "zmin", &zmin);
        init(radius, zmin, fftInit, fftx, ffty, variances, brdfShader);
    }
};

extern const char drawOceanFFT[] = "drawOceanFFT";

static ResourceFactory::Type<drawOceanFFT, DrawOceanFFTTaskResource> DrawOceanFFTTaskType;

}
