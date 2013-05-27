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

#include "proland/rivers/AnimatedPerlinWaveTile.h"

#include <climits>
#include "GL/glew.h"

#include "ork/render/CPUBuffer.h"
#include "ork/resource/ResourceTemplate.h"

using namespace std;
using namespace ork;

namespace proland
{

// transforms even the sequence 0,1,2,3,... into reasonably good random numbers
// challenge: improve on this in speed and "randomness"!
inline unsigned int randhash(unsigned int seed)
{
   unsigned int i=(seed^12345391u)*2654435769u;
   i^=(i<<6)^(i>>26);
   i*=2654435769u;
   i+=(i<<5)^(i>>12);
   return i;
}

float randhashf(unsigned int seed, float a, float b)
{
    return ( (b-a)*randhash(seed)/(float)UINT_MAX + a);
}

static vec3f sample_sphere3(unsigned int &seed)
{
    vec3f v;
    float m2;
    do {
        for(unsigned int i = 0; i < 3; ++i){
            v[i] = randhashf(seed++, -1, 1);
        }
        m2 = (v).length();
    }
    while (m2 > 1 || m2 == 0);
    return v / std::sqrt(m2);
}

template<class S,class T>
inline S lerp(const S &value0, const S &value1, T f)
{ return (1-f)*value0 + f*value1; }

template<class S,class T>
inline S bilerp(const S &v00, const S &v10,
                const S &v01, const S &v11,
                T fx, T fy)
{
   return lerp(
               lerp(v00, v10, fx),
               lerp(v01, v11, fx),
               fy);
}

template<class S,class T>
inline S trilerp(
   const S& v000, const S& v100,
   const S& v010, const S& v110,
   const S& v001, const S& v101,
   const S& v011, const S& v111,
   T fx, T fy, T fz)
{
   return lerp(
            bilerp(v000, v100, v010, v110, fx, fy),
            bilerp(v001, v101, v011, v111, fx, fy),
            fz);
}

AnimatedPerlinWaveTile::Noise::Noise(unsigned int seed)
{
    for(unsigned int i = 0; i < n; ++i){
      basis[i] = sample_sphere3(seed);
      perm[i] = i;
    }
    reinitialize(seed);
}

void AnimatedPerlinWaveTile::Noise::reinitialize(unsigned int seed)
{
    for(unsigned int i = 1; i < n; ++i){
      int j = randhash(seed++) % (i + 1);
      std::swap(perm[i], perm[j]);
    }
}

float AnimatedPerlinWaveTile::Noise::operator()(float x, float y, float z) const
{
    float floorx = std::floor(x);
    float floory = std::floor(y);
    float floorz = std::floor(z);
    int i = (int) floorx;
    int j = (int) floory;
    int k = (int) floorz;
    const vec3f &n000 = basis[hash_index(i, j, k)];
    const vec3f &n100 = basis[hash_index(i + 1, j, k)];
    const vec3f &n010 = basis[hash_index(i, j + 1, k)];
    const vec3f &n110 = basis[hash_index(i + 1, j + 1, k)];
    const vec3f &n001 = basis[hash_index(i, j, k + 1)];
    const vec3f &n101 = basis[hash_index(i + 1, j, k + 1)];
    const vec3f &n011 = basis[hash_index(i, j + 1, k + 1)];
    const vec3f &n111 = basis[hash_index(i + 1, j + 1, k + 1)];
    float fx = x - floorx;
    float fy = y - floory;
    float fz = z - floorz;
    float sx = fx * fx * fx * (10 - fx * (15 - fx * 6)),
         sy = fy * fy * fy * (10 - fy * (15 - fy * 6)),
         sz = fz * fz * fz * (10 - fz * (15 - fz * 6));
    return trilerp(    fx*n000[0] +     fy*n000[1] +     fz*n000[2],
                  (fx-1)*n100[0] +     fy*n100[1] +     fz*n100[2],
                      fx*n010[0] + (fy-1)*n010[1] +     fz*n010[2],
                  (fx-1)*n110[0] + (fy-1)*n110[1] +     fz*n110[2],
                      fx*n001[0] +     fy*n001[1] + (fz-1)*n001[2],
                  (fx-1)*n101[0] +     fy*n101[1] + (fz-1)*n101[2],
                      fx*n011[0] + (fy-1)*n011[1] + (fz-1)*n011[2],
                  (fx-1)*n111[0] + (fy-1)*n111[1] + (fz-1)*n111[2],
                  sx, sy, sz);
}

float AnimatedPerlinWaveTile::Noise::operator()(float x, float y, float z, int wxy, int wz) const
{
    float floorx=std::floor(x);
    float floory=std::floor(y);
    float floorz=std::floor(z);
    int i=(int)floorx;
    int j=(int)floory;
    int k=(int)floorz;
    const vec3f &n000=basis[hash_index(i,j,k, wxy, wz)];
    const vec3f &n100=basis[hash_index(i+1,j,k, wxy, wz)];
    const vec3f  &n010=basis[hash_index(i,j+1,k, wxy, wz)];
    const vec3f  &n110=basis[hash_index(i+1,j+1,k, wxy, wz)];
    const vec3f  &n001=basis[hash_index(i,j,k+1, wxy, wz)];
    const vec3f  &n101=basis[hash_index(i+1,j,k+1, wxy, wz)];
    const vec3f  &n011=basis[hash_index(i,j+1,k+1, wxy, wz)];
    const vec3f  &n111=basis[hash_index(i+1,j+1,k+1, wxy, wz)];
    float fx=x-floorx, fy=y-floory, fz=z-floorz;
    float sx=fx*fx*fx*(10-fx*(15-fx*6)),
         sy=fy*fy*fy*(10-fy*(15-fy*6)),
         sz=fz*fz*fz*(10-fz*(15-fz*6));
    return trilerp(    fx*n000[0] +     fy*n000[1] +     fz*n000[2],
                  (fx-1)*n100[0] +     fy*n100[1] +     fz*n100[2],
                      fx*n010[0] + (fy-1)*n010[1] +     fz*n010[2],
                  (fx-1)*n110[0] + (fy-1)*n110[1] +     fz*n110[2],
                      fx*n001[0] +     fy*n001[1] + (fz-1)*n001[2],
                  (fx-1)*n101[0] +     fy*n101[1] + (fz-1)*n101[2],
                      fx*n011[0] + (fy-1)*n011[1] + (fz-1)*n011[2],
                  (fx-1)*n111[0] + (fy-1)*n111[1] + (fz-1)*n111[2],
                  sx, sy, sz);
}

AnimatedPerlinWaveTile::AnimatedPerlinWaveTile() : WaveTile()
{
}

AnimatedPerlinWaveTile::AnimatedPerlinWaveTile(string &name, int gridSize, int tileSize, float waveLength, int timeLoop)
{
    init(name, gridSize, tileSize, waveLength, timeLoop);
}

AnimatedPerlinWaveTile::~AnimatedPerlinWaveTile()
{
}

void AnimatedPerlinWaveTile::init(ptr<Texture2D> texture, int size, int numLodLevel, int t, unsigned int seed)
{
    Noise noise(seed);
    int tw = 4;
//    m_normalMaps = new GLuint[tsize];
//    m_texNormalMaps = new TextureID[tsize];

    float** hd = new float*[numLodLevel];
    int nsize = size;
    for(int i = 0; i < numLodLevel;i++)
    {
        hd[i] = new float[nsize*nsize];
        nsize /= 2;
    }


    //generate height field;
    int n = 4;//m_numOctave;
    float p = 0.5;//m_persistence;
    for(int i = 0; i < size*size; i++)
        hd[0][i]=0;

    int w = 32;
    float u = 1.0;

    for(int k = 0; k < n; k++)
    {
            int i = 0;
            for(int r = 0; r < size; r++)
            {
                for(int c = 0; c < size; c++)
                {
                    float x = (c/(float)size) * w;
                    float y = (r/(float)size) * w;
                    float z = (t/(float)timeLoop) * tw;

                    float pn = noise(x, y, z, w, tw);
                    pn *= u;
                    hd[0][i] += pn;
                    i++;
                }
            }
            u *= p;
            w *= 2;
    }


    //generate mipmap
    nsize = size;
    for(int i = 1; i < numLodLevel; i++)
    {
        int hsize = nsize/2;
        for(int r = 0; r < hsize; r++)
        {
            for(int c = 0; c < hsize; c++)
            {
                int k = r * hsize + c;
                int k0 = (2*r) * nsize + (2*c);
                int k1 = (2*r) * nsize + (2*c)+1;
                int k2 = (2*r+1) * nsize + (2*c);
                int k3 = (2*r+1) * nsize + (2*c)+1;

                hd[i][k] = 1/4.0*(hd[i-1][k0]+hd[i-1][k1]+hd[i-1][k2]+hd[i-1][k3]);

            }
        }

        nsize /= 2;
    }

    float* texData = new float[size * size * 3];

    float scale = .5;
    nsize = size;
    for(int level = 0; level < numLodLevel; level++)
    {
        int k = 0;
        for(int r = 0; r < nsize; r++)
        {
            for(int c = 0; c <nsize; c++)
            {

                float nx;
                if ( c < (nsize -1 ))
                    nx = hd[level][ r * nsize + c + 1 ] - hd[level][ r * nsize + c];
                else
                    nx = hd[level][r*nsize] - hd[level][r*nsize + c];

                float ny;
                if ( r > 0)
                    ny = hd[level][ (r - 1) * nsize + c] - hd[level][ r * nsize + c];
                else
                    ny = hd[level][ r * nsize + c] - hd[level][c];


                texData[3*k] = nx * scale;//rand()/(float)RAND_MAX; //nx  ;
                texData[3*k + 1] =ny * scale;//rand()/(float)RAND_MAX;//ny; //exData[3*k];
                texData[3*k + 2] = 1.0;

                k++;
            }
        }


        texture->setSubImage(level, 0, 0, nsize, nsize, RGB, FLOAT, Buffer::Parameters(), CPUBuffer(texData));
        nsize /= 2;
    }

    delete []texData;
    for(int i = 0; i < numLodLevel; i++)
        delete []hd[i];
    delete []hd;

}

void AnimatedPerlinWaveTile::updateUniform(ptr<Program> p)
{
    checkUniforms(p);

    if (patternTexU != NULL) {
        patternTexU->set(tex[(int)(time / 5.0f) % timeLoop]);
    }
    if (patternTexSizeU != NULL) {
        patternTexSizeU->set(tileSize);
    }
    if (lengthU != NULL) {
        lengthU->set(waveLength);
    }
    if (timeU != NULL) {
        timeU->set(time);
    }
    if (timeLoopU != NULL) {
        timeLoopU->set(timeLoop);
    }
}

void AnimatedPerlinWaveTile::init(string &name, int gridSize, int tileSize, float waveLength, int timeLoop)
{
    WaveTile::init(name, NULL, gridSize, tileSize, waveLength, timeLoop);

    int size = gridSize;
    int numLodLevel = int(log((double)size) / log(2.0)) + 1;

    unsigned int seed = rand();
    for (int i = 0; i < timeLoop; i++) {
        ptr<Texture2D> T = new Texture2D(size, size, RGB16F, RGB, FLOAT, Texture::Parameters().wrapS(REPEAT).wrapT(REPEAT).min(LINEAR_MIPMAP_LINEAR).mag(LINEAR).lodMin(0).lodMax(numLodLevel).maxAnisotropyEXT(16.0f), Buffer::Parameters(), CPUBuffer(0));
        init(T, size, numLodLevel, i, seed);
        tex.push_back(T);
    }
}

void AnimatedPerlinWaveTile::swap(ptr<AnimatedPerlinWaveTile> t)
{
    WaveTile::swap(t);
    std::swap(tex, t->tex);
}

class AnimatedPerlinWaveTileResource : public ResourceTemplate<50, AnimatedPerlinWaveTile>
{
public:
    AnimatedPerlinWaveTileResource(ptr<ResourceManager> manager, const string &name, ptr<ResourceDescriptor> desc, const TiXmlElement *e = NULL) :
        ResourceTemplate<50, AnimatedPerlinWaveTile>(manager, name, desc)
    {
        e = e == NULL ? desc->descriptor : e;

        checkParameters(desc, e, "name,samplerName,tileSize,gridSize,waveLength,timeLoop,");

        int gridSize = 256;
        int tileSize = 1;
        float waveLength = 1.0f;
        int timeLoop = 32;
        string sName = e->Attribute("samplerName");

        if (e->Attribute("gridSize") != NULL) {
            getIntParameter(desc, e, "gridSize", &gridSize);
        }
        if (e->Attribute("tileSize") != NULL) {
            getIntParameter(desc, e, "tileSize", &tileSize);
        }
        if (e->Attribute("waveLength") != NULL) {
            getFloatParameter(desc, e, "waveLength", &waveLength);
        }
        if (e->Attribute("timeLoop") != NULL) {
            getIntParameter(desc, e, "timeLoop", &timeLoop);
        }
        init(sName, gridSize, tileSize, waveLength, timeLoop);
    }
};


extern const char animatedPerlinWaveTile[] = "animatedPerlinWaveTile";

static ResourceFactory::Type<animatedPerlinWaveTile, AnimatedPerlinWaveTileResource> AnimatedPerlinWaveTileType;

}
