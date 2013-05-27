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

#include "proland/rivers/PerlinWaveTile.h"

#include "ork/render/CPUBuffer.h"
#include "ork/resource/ResourceTemplate.h"

using namespace std;
using namespace ork;

namespace proland
{

PerlinWaveTile::Noise::Noise(unsigned int seed)
{
   for (unsigned int i = 0; i < n; ++i){
      float theta = (float) (i * 2 * M_PI) / n;
      basis[i][0] = std::cos(theta);
      basis[i][1] = std::sin(theta);
      perm[i] = i;
   }
   reinitialize(seed);
}

inline unsigned int randhash(unsigned int seed)
{
   unsigned int i=(seed^12345391u)*2654435769u;
   i^=(i<<6)^(i>>26);
   i*=2654435769u;
   i+=(i<<5)^(i>>12);
   return i;
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

void PerlinWaveTile::Noise::reinitialize(unsigned int seed)
{
   for(unsigned int i = 1; i < n; ++i){
      int j = randhash(seed++) % (i + 1);
      std::swap(perm[i], perm[j]);
   }
}

float PerlinWaveTile::Noise::operator()(float x, float y) const
{
   float floorx = std::floor(x);
   float floory = std::floor(y);
   int i = (int)floorx;
   int j = (int)floory;

   const vec2f &n00 = basis[hash_index(i, j)];
   const vec2f &n10 = basis[hash_index(i + 1, j)];
   const vec2f &n01 = basis[hash_index(i, j + 1)];
   const vec2f &n11 = basis[hash_index(i + 1, j + 1)];
   float fx = x - floorx;
   float fy = y - floory;
   float sx = fx * fx * fx * (10 - fx * (15 - fx * 6));
   float sy = fy * fy * fy * (10 - fy * (15 - fy * 6));

    return bilerp(      fx * n00[0] +       fy * n00[1],
                  (fx - 1) * n10[0] +       fy * n10[1],
                        fx * n01[0] + (fy - 1) * n01[1],
                  (fx - 1) * n11[0] + (fy - 1) * n11[1],
                  sx, sy);
}



float PerlinWaveTile::Noise::operator()(float x, float y, int w) const
{
   float floorx = std::floor(x);
   float floory = std::floor(y);
   int i = (int)floorx;
   int j = (int)floory;

   const vec2f &n00 = basis[hash_index(i, j, w)];
   const vec2f &n10 = basis[hash_index(i + 1, j, w)];
   const vec2f &n01 = basis[hash_index(i, j + 1, w)];
   const vec2f &n11 = basis[hash_index(i + 1, j + 1, w)];

   float fx = x - floorx;
   float fy = y - floory;
   float sx = fx * fx * fx * (10 - fx * (15 - fx * 6));
   float sy = fy * fy * fy * (10 - fy * (15 - fy * 6));

    return bilerp(    fx*n00[0] +     fy*n00[1],
                  (fx-1)*n10[0] +     fy*n10[1],
                      fx*n01[0] + (fy-1)*n01[1],
                  (fx-1)*n11[0] + (fy-1)*n11[1],
                  sx, sy);
}

PerlinWaveTile::PerlinWaveTile() : WaveTile()
{
}

PerlinWaveTile::PerlinWaveTile(string &name, int gridSize, int tileSize, float waveLength, int timeLoop) : WaveTile()
{
    init(name, gridSize, tileSize, waveLength, timeLoop);
}

PerlinWaveTile::~PerlinWaveTile()
{
}

void PerlinWaveTile::init(ptr<Texture2D> tex, int size, int numLodLevel)
{
    Noise noise;
       float** hd = new float* [numLodLevel];
    int nsize = size;

    for(int i = 0; i < numLodLevel;i++)
    {
        hd[i] = new float[nsize*nsize];
        nsize /= 2;
    }

    //generate height field;
    int n = 4;//m_numOctave;
    float p = 0.5;//m_persistence;
    for(int i = 0; i < size*size; i++) {
        hd[0][i]=0;
    }

    int w = 32;
    float u = 1.0;

    for(int k = 0; k < n; k++)
    {
        int i = 0;
        for(int r = 0; r < size; r++)
        {
            for(int c = 0; c < size; c++)
            {
                float x = (c / (float)size) * w;
                float y = (r / (float)size) * w;

                float t = noise(x, y, w);
                t *= u;
                hd[0][i] += t;
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
                int k0 = (2 * r) * nsize + (2 * c);
                int k1 = (2 * r) * nsize + (2 * c) + 1;
                int k2 = (2 * r + 1) * nsize + (2 * c);
                int k3 = (2 * r + 1) * nsize + (2 * c) + 1;

                hd[i][k] = 1 / 4.0 * (hd[i - 1][k0] + hd[i - 1][k1] + hd[i - 1][k2] + hd[i - 1][k3]);

            }
        }

        nsize /= 2;
    }




    //generate texture

//int w, int h, internalformat tf, format f, type t, const Buffer &pixels,
//    wrap ws, wrap wt, filter min, filter mag, int minLOD, int maxLOD, float maxAnisotropy, bool shadow, int si
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


                //if( level > 1) scale = 0;

                    texData[3 * k] = nx * scale;//rand()/(float)RAND_MAX; //nx  ;
                    texData[3 * k + 1] = ny * scale;//rand()/(float)RAND_MAX;//ny; //exData[3*k];
                    texData[3 * k + 2] = 1.0;
                k++;
            }
        }
        tex->setSubImage(level, 0, 0, nsize, nsize, RGB, FLOAT, Buffer::Parameters(), CPUBuffer(texData));

        nsize /= 2;
    }

    delete []texData;
    for(int i = 0; i < numLodLevel; i++)
        delete []hd[i];
    delete []hd;
}

void PerlinWaveTile::init(string &name, int gridSize, int tileSize, float waveLength, int timeLoop)
{
    int size = gridSize;
    int numLodLevel = int(log((double)size) / log(2.0)) + 1;

    ptr<Texture2D> t = new Texture2D(size, size, RGB16F, RGB, FLOAT, Texture::Parameters().wrapS(REPEAT).wrapT(REPEAT).min(LINEAR_MIPMAP_LINEAR).mag(LINEAR).lodMin(0).lodMax(numLodLevel).maxAnisotropyEXT(16.0f), Buffer::Parameters(), CPUBuffer(0));

    init(t, size, numLodLevel);
    WaveTile::init(name, t, gridSize, tileSize, waveLength, timeLoop);
}

void PerlinWaveTile::swap(ptr<PerlinWaveTile> t)
{
    WaveTile::swap(t);
}

class PerlinWaveTileResource : public ResourceTemplate<50, PerlinWaveTile>
{
public:
    PerlinWaveTileResource(ptr<ResourceManager> manager, const string &name, ptr<ResourceDescriptor> desc, const TiXmlElement *e = NULL) :
        ResourceTemplate<50, PerlinWaveTile>(manager, name, desc)
    {
        e = e == NULL ? desc->descriptor : e;

        checkParameters(desc, e, "name,samplerName,tileSize,gridSize,waveLength,timeLoop,");

        int gridSize = 256;
        int tileSize = 1;
        float waveLength = 1.0f;
        int timeLoop = 64;
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


extern const char perlinWaveTile[] = "perlinWaveTile";

static ResourceFactory::Type<perlinWaveTile, PerlinWaveTileResource> PerlinWaveTileType;

}
