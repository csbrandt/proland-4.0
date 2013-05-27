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

#ifndef _PROLAND_WAVETILE_H_
#define _PROLAND_WAVETILE_H_

#include "ork/core/Object.h"
#include "ork/render/Program.h"
#include "ork/render/Texture2D.h"

using namespace ork;

namespace proland
{

/**
 * WaveTile are Texture used to advect rivers normal.
 * They can be updated through time, in order to change the waves profiles.
 * @ingroup rivers
 * @author Antoine Begault
 */
PROLAND_API class WaveTile : public Object
{
public:
    /**
     * Creates a new WaveTile.
     *
     * @param tex the Texture2D that contains the wave profiles.
     * @param gridSize size of the texture.
     * @param tileSize size of a tile.
     * @param waveLength size of a wave.
     * @param timeLoop number of frames of a wave cycle.
     */
    WaveTile(std::string &name, ptr<Texture2D> tex, int gridSize, int tileSize, float waveLength, int timeLoop);

    /**
     * Deletes this WaveTile.
     */
    virtual ~WaveTile();

    /**
     * Updates the Texture depending on the time spent between the two images.
     */
    virtual void timeStep(float dt);

    /**
     * Updates the GLSL uniforms.
     */
    virtual void updateUniform(ptr<Program> p);

    virtual void checkUniforms(ptr<Program> p);

    void setWaveLength(float length);

    float getWaveLength() const;

protected:
    /**
     * Creates a new WaveTile.
     */
    WaveTile();

    virtual void swap(ptr<WaveTile> t);
    /**
     * initializes the fields of a WaveTile.
     *
     * @param tex the Texture2D that contains the wave profiles.
     * @param gridSize size of the texture.
     * @param tileSize size of a tile.
     * @param waveLength size of a wave.
     * @param timeLoop number of frames of a wave cycle.
     */
    virtual void init(std::string &name, ptr<Texture2D> tex, int gridSize, int tileSize, float waveLength, int timeLoop);

    /**
     * Current wave tile's name.
     */
    std::string name;

    /**
     * The Texture2D that contains the wave profiles.
     */
    ptr<Texture2D> tex;

    /**
     * Size of the texture.
     */
    int gridSize;

    /**
     * Size of a tile.
     */
    int tileSize;

    /**
     * Current Time in the time cycle.
     */
    float time;

    /**
     * Number of frames of a wave cycle.
     */
    int timeLoop;

    /**
     * Size of a wave.
     */
    float waveLength;

    /**
     * Last updated program.
     * Used to avoid querying again every uniforms.
     */
    ptr<Program> lastProgram;

    /**
     * Current Time in the time cycle.
     */
    ptr<Uniform1f> timeU;

    /**
     * Number of frames of a wave cycle.
     */
    ptr<Uniform1f> timeLoopU;

    /**
     * Size of a wave.
     */
    ptr<Uniform1f> lengthU;

    /**
     * Size of a wave pattern inside texture.
     */
    ptr<Uniform1f> patternTexSizeU;

    /**
     * The sampler containing the texture.
     */
    ptr<UniformSampler> patternTexU;
};

}

#endif
