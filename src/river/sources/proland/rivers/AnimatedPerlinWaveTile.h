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

#ifndef _PROLAND_ANIMATEDPERLINWAVETILE_H_
#define _PROLAND_ANIMATEDPERLINWAVETILE_H_

#include "proland/rivers/WaveTile.h"

namespace proland
{

/**
 * WaveTile are Texture used to advect rivers normal.
 * They can be updated through time, in order to change the waves profiles.
 * AnimatedPerlinWaveTile is a serie of #timeLoop Noise Textures displayed successively.
 * @ingroup rivers
 * @author Antoine Begault
 */
PROLAND_API class AnimatedPerlinWaveTile : public WaveTile
{
public:
    /**
     * Creates a new AnimatedPerlinWaveTile.
     * See WaveTile#WaveTile().
     */
    AnimatedPerlinWaveTile(std::string &name, int tileSize, int gridSize, float waveLength, int timeLoop);

    /**
     * Deletes an AnimatedPerlinWaveTile.
     */
    virtual ~AnimatedPerlinWaveTile();

    /**
     * See WaveTile#updateUniform().
     */
    virtual void updateUniform(ptr<Program> p);

protected:
    /**
     * 3D Noise generator. Taken from Qizhi's implementation.
     */
    struct Noise //3D noise
    {
    public:
       Noise (unsigned int seed=171717);

       void reinitialize(unsigned int seed);

       float operator()(float x, float y, float z) const;

       float operator()(float x, float y, float z, int wxy, int wz) const;

       float operator()(const vec3f &x) const
       {
           return (*this)(x[0], x[1], x[2]);
       }

    protected:
       static const unsigned int n=256;

       vec3f basis[n];

       int perm[n];

       unsigned int hash_index(int i, int j, int k, int wxy, int wz) const
       {
           return perm[(perm[(perm[i % wxy] + j) % wxy] + k % wz) % wxy];
       }

       unsigned int hash_index(int i, int j, int k) const
       {
           return perm[(perm[(perm[i % n] + j) % n] + k) % n];
       }
    };

    /**
     * Creates a new AnimatedPerlinWaveTile.
     */
    AnimatedPerlinWaveTile();

    /**
     * Initializes the fields of a AnimatedPerlinWaveTile.
     * See WaveTile#init().
     */
    void init(ptr<Texture2D> t, int size, int numLodLevels, int timeLoop, unsigned int seed);

    /**
     * Initializes the fields of a AnimatedPerlinWaveTile.
     * See WaveTile#init().
     */
    virtual void init(std::string &name, int tileSize, int gridSize, float waveLength, int timeLoop);

    virtual void swap(ptr<AnimatedPerlinWaveTile> t);

    /**
     * Contains the textures used to animate the wave profile.
     */
    std::vector<ptr<Texture2D> > tex;
};

}

#endif
