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

#ifndef _PROLAND_PLANET_VIEW_CONTROLLER_H_
#define _PROLAND_PLANET_VIEW_CONTROLLER_H_

#include "proland/util/TerrainViewController.h"

using namespace ork;

namespace proland
{

/**
 * A TerrainViewController for spherical terrains (i.e., planets). This subclass
 * interprets the #x0 and #y0 fields as longitudes and latitudes on the planet,
 * and considers #theta and #phi as relative to the tangent plane at the (#x0,#y0)
 * point.
 * @ingroup proland_util
 * @authors Eric Bruneton, Antoine Begault, Guillaume Piolat
 */
PROLAND_API class PlanetViewController : public TerrainViewController
{
public:
    /**
     * The radius of the planet at sea level.
     */
    const double R;

    /**
     * Creates a new PlanetViewController.
     *
     * @param node a SceneNode representing a camera position and orientation
     *      in the scene.
     * @param R the planet radius at sea level.
     */
    PlanetViewController(ptr<SceneNode> node, double R);

    /**
     * Deletes this PlanetViewController.
     */
    virtual ~PlanetViewController();

    virtual double getHeight();

    virtual void move(vec3d &oldp, vec3d &p);

    virtual void moveForward(double distance);

    virtual void turn(double angle);

    virtual double interpolate(double sx0, double sy0, double stheta, double sphi, double sd,
            double dx0, double dy0, double dtheta, double dphi, double dd, double t);

    virtual void interpolatePos(double sx0, double sy0, double dx0, double dy0, double t, double &x0, double &y0);

    virtual void update();
};

}

#endif
