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

#ifndef _PROLAND_TERRAIN_VIEW_CONTROLLER_H_
#define _PROLAND_TERRAIN_VIEW_CONTROLLER_H_

#include "ork/scenegraph/SceneNode.h"

using namespace ork;

namespace proland
{

/**
 * @defgroup proland_util util
 * Provides utility classes to control camera movements.
 * @ingroup proland
 */

/**
 * A view controller for flat terrains. The camera position is specified
 * from a "look at" position (x0,y0) on ground, with a distance d between
 * camera and this position, and two angles (theta,phi) for the direction
 * of this vector. The #update method sets the localToParent transformation
 * of the SceneNode associated with this controller, which is intended to
 * represent the camera position and orientation in the scene.
 * @ingroup proland_util
 * @authors Eric Bruneton, Antoine Begault, Guillaume Piolat
 */
PROLAND_API class TerrainViewController : public Object
{
public:
    /**
     * The field of view angle.
     */
    double fov;

    /**
     * The x coordinate of the point the camera is looking at on the ground.
     */
    double x0;

    /**
     * The y coordinate of the point the camera is looking at on the ground.
     */
    double y0;

    /**
     * The zenith angle of the vector between the "look at" point and the camera.
     */
    double theta;

    /**
     * The azimuth angle of the vector between the "look at" point and the camera.
     */
    double phi;

    /**
     * The distance between the "look at" point and the camera.
     */
    double d;

    /**
     * Zoom factor (realized by increasing d and decreasing fov).
     */
    double zoom;

    /**
     * The camera position in world space resulting from the x0,y0,theta,phi,
     * and d parameters.
     */
    vec3d position;

    /**
     * Creates a new TerrainViewController to control the given SceneNode.
     *
     * @param node a SceneNode representing a camera position and orientation
     *      in the scene.
     * @param d0 the initial valued of the #d distance.
     */
    TerrainViewController(ptr<SceneNode> node, double d0);

    /**
     * Deletes this TerrainViewController.
     */
    virtual ~TerrainViewController();

    /**
     * Returns the SceneNode associated with this TerrainViewController.
     * This SceneNode represents a camera position and orientation in the
     * scene.
     */
    ptr<SceneNode> getNode();

    /**
     * Sets the SceneNode associated with this TerrainViewController.
     *
     * @param node a SceneNode representing a camera position and orientation
     *      in the scene.
     */
    void setNode(ptr<SceneNode> node);

    /**
     * Returns the %terrain elevation below the camera.
     */
    float getGroundHeight();

    /**
     * Sets the %terrain elevation below the camera. This elevation is used
     * to adjust the camera position so that it is not below the ground.
     *
     * @param groundHeight the %terrain elevation below the camera.
     */
    void setGroundHeight(float groundHeight);

    /**
     * Returns the height of the camera above the z=0 surface.
     */
    virtual double getHeight();

    /**
     * Moves the "look at" point so that "oldp" appears at the position of "p"
     * on screen.
     *
     * @param oldp a %terrain point.
     * @param p another %terrain point.
     */
    virtual void move(vec3d &oldp, vec3d &p);

    virtual void moveForward(double distance);

    virtual void turn(double angle);

    /**
     * Sets the position as the interpolation of the two given positions with
     * the interpolation parameter t (between 0 and 1). The source position is
     * sx0,sy0,stheta,sphi,sd, the destination is dx0,dy0,dtheta,dphi,dd.
     *
     * @return the new value of the interpolation parameter t.
     */
    virtual double interpolate(double sx0, double sy0, double stheta, double sphi, double sd,
            double dx0, double dy0, double dtheta, double dphi, double dd, double t);

    virtual void interpolatePos(double sx0, double sy0, double dx0, double dy0, double t, double &x0, double &y0);

    /**
     * Returns a direction interpolated between the two given direction.
     *
     * @param slon start longitude.
     * @param slat start latitude.
     * @param elon end longitude.
     * @param elat end latitude.
     * @param t interpolation parameter between 0 and 1.
     * @param[out] lon interpolated longitude.
     * @param[out] lat interpolated latitude.
     */
    void interpolateDirection(double slon, double slat, double elon, double elat, double t, double &lon, double &lat);

    /**
     * Sets the localToParent transform of the SceneNode associated with this
     * TerrainViewController. The transform is computed from the view parameters
     * x0,y0,theta,phi and d.
     */
    virtual void update();

    /**
     * Sets the camera to screen perspective projection.
     *
     * @param znear an optional znear plane (0.0 means that a default value
     *      must be used).
     * @param zfar an optional zfar plane (0.0 means that a default value
     *      must be used).
     * @param viewport an optional viewport to select a part of the image.
     *      The default value [-1:1]x[-1:1] selects the whole image.
     */
    virtual void setProjection(float znear = 0.0f, float zfar = 0.0f, vec4f viewport = vec4f(-1.0f, 1.0f, -1.0f, 1.0f));

protected:
    /**
     * The SceneNode associated with this TerrainViewController.
     */
    ptr<SceneNode> node;

    /**
     * The %terrain elevation below the camera.
     */
    float groundHeight;
};

}

#endif
