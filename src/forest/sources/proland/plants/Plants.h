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

#ifndef _PROLAND_PLANTS_H_
#define _PROLAND_PLANTS_H_

#include "ork/render/FrameBuffer.h"

using namespace ork;

namespace proland
{

/**
 * @defgroup plants plants
 * Provides classes to render many 3D models on a %terrain with OpenGL instancing.
 * @ingroup proland
 */

/**
 * TODO.
 * @ingroup plants
 * @author Eric Bruneton, Antoine Begault, Guillaume Piolat
 */
PROLAND_API class Plants : public Object
{
public:
    /**
     * The GLSL Shader that is able to determine if a seed is valid (see DrawPlantsTask). If
     * it is, it will determine its properties, such as the size of the plant, its type,
     * color etc... Eliminated seeds will contain (0, 0, 0, 0) values.
     */
    ptr<Program> selectProg;

    /**
     * The GLSL Shader used to render the plants shadows, based on the selected seeds.
     */
    ptr<Program> shadowProg;

    /**
     * The GLSL Shader used to render the plants, based on the selected seeds.
     */
    ptr<Program> renderProg;

    /**
     * Creates a new Plants.
     *
     * @param minLevel the first level to display the models from this Plants.
     * @param maxLevel the last level to display the models from this Plants.
     * @param maxDistance the furthest distance at which to display the models.
     * @param lodDistance the distance between each lod.
     */
    Plants(int minLevel, int maxLevel, int minDensity, int maxDensity, int tileCacheSize, float maxDistance);

    /**
     * Deletes this Plants.
     */
    ~Plants();

    /**
     * TODO.
     */
    int getMinLevel();

    /**
     * TODO.
     */
    int getMaxLevel();

    /**
     * TODO.
     */
    int getMinDensity();

    /**
     * TODO.
     */
    int getMaxDensity();

    /**
     * TODO.
     */
    int getTileCacheSize();

    /**
     * TODO.
     */
    float getMaxDistance();

    /**
     * TODO.
     */
    int getPatternCount();

    /**
     * TODO.
     */
    float getPoissonRadius();

    /**
     * Returns the i'th pattern.
     */
    ptr<MeshBuffers> getPattern(int index);

    /**
     * TODO.
     */
    void addPattern(ptr<MeshBuffers> pattern);

    /**
     * TODO.
     */
    void setMaxDistance(float maxDistance);

protected:
    /**
     * Creates a new Plants.
     */
    Plants();

    /**
     * Initializes a Plants fields.
     *
     * See #Plants.
     */
    void init(int minLevel, int maxLevel, int minDensity, int maxDensity, int tileCacheSize, float maxDistance);

    void swap(ptr<Plants> p);

private:
    int minLevel;

    int maxLevel;

    int minDensity;

    int maxDensity;

    int tileCacheSize;

    float maxDistance;

    std::vector< ptr<MeshBuffers> > patterns;
};

}

#endif
