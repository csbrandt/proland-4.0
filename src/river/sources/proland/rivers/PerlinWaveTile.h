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

#ifndef _PROLAND_PERLINWAVETILE_H_
#define _PROLAND_PERLINWAVETILE_H_

#include "proland/rivers/WaveTile.h"

namespace proland
{

/**
 * WaveTile are Texture used to advect rivers normal.
 * They can be updated through time, in order to change the waves profiles.
 * PerlinWaveTile are basically Noise Texture.
 * @ingroup rivers
 * @author Antoine Begault
 */
PROLAND_API class PerlinWaveTile : public WaveTile
{
public:
    /**
     * Creates a new PerlinWaveTile.
     * See WaveTile#WaveTile().
     */
    PerlinWaveTile(std::string &name, int tileSize, int gridSize, float waveLength, int timeLoop);

    /**
     * Deletes this PerlinWaveTile.
     */
    virtual ~PerlinWaveTile();

protected:
    /**
     * 2D Noise generator. Taken from Qizhi's implementation.
     */
    struct Noise //2D noise
    {
    public:
        Noise(unsigned int seed=171717);

        void reinitialize(unsigned int seed);

        float operator()(float x, float y) const;

        float operator()(float x, float y, int w) const;

        float operator()(const vec2f &x) const
        {
            return (*this)(x[0], x[1]);
        }

    protected:
        static const unsigned int n=256;

        vec2f basis[n];

        int perm[n];

        unsigned int hash_index(int i, int j, int w) const
        {
            return perm[(perm[i%w]+j)%w];
        }

        unsigned int hash_index(int i, int j) const
        {
            return perm[(perm[i%n]+j)%n];
        }
    };

    /**
     * Creates a new PerlinWaveTile.
     */
    PerlinWaveTile();

    /**
     * Initializes the fields of a PerlinWaveTile.
     * See WaveTile#init().
     */
    void init(ptr<Texture2D> tex, int size, int numLodLevel);

    /**
     * Initializes the fields of a PerlinWaveTile.
     * See WaveTile#init().
     */
    virtual void init(std::string &name, int tileSize, int gridSize, float waveLength, int timeLoop);

    virtual void swap(ptr<PerlinWaveTile> t);
};

}

#endif
