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

#ifndef _PROLAND_DEFORMATION_H_
#define _PROLAND_DEFORMATION_H_

#include "ork/math/vec3.h"
#include "ork/math/box2.h"
#include "ork/math/box3.h"
#include "ork/render/FrameBuffer.h"
#include "ork/scenegraph/SceneManager.h"
#include "proland/terrain/TerrainQuad.h"

using namespace ork;

namespace proland
{

class TerrainNode;

/**
 * A deformation of space. Such a deformation maps a 3D source point to a 3D
 * destination point. The source space is called the <i>local</i> space, while
 * the destination space is called the <i>deformed</i> space. Source and
 * destination points are defined with their x,y,z coordinates in an orthonormal
 * reference frame. A Deformation is also responsible to set the shader uniforms
 * that are necessary to project a TerrainQuad on screen, taking the deformation
 * into account. The default implementation of this class implements the
 * identity deformation, i.e. the deformed point is equal to the local one.
 * @ingroup terrain
 * @authors Eric Bruneton, Antoine Begault
 */
PROLAND_API class Deformation : public Object
{
public:
    /**
     * Creates a new Deformation.
     */
    Deformation();

    /**
     * Deletes this Deformation.
     */
    virtual ~Deformation();

    /**
     * Returns the deformed point corresponding to the given source point.
     *
     * @param localPt a point in the local (i.e., source) space.
     * @return the corresponding point in the deformed (i.e., destination) space.
     */
    virtual vec3d localToDeformed(const vec3d &localPt) const;

    /**
     * Returns the differential of the deformation function at the given local
     * point. This differential gives a linear approximation of the deformation
     * around a given point, represented with a matrix. More precisely, if p
     * is near localPt, then the deformed point corresponding to p can be
     * approximated with localToDeformedDifferential(localPt) * (p - localPt).
     *
     * @param localPt a point in the local (i.e., source) space. <i>The z
     *      coordinate of this point is ignored, and considered to be 0</i>.
     * @return the differential of the deformation function at the given local
     *      point.
     */
    virtual mat4d localToDeformedDifferential(const vec3d &localPt, bool clamp = false) const;

    /**
     * Returns the local point corresponding to the given source point.
     *
     * @param deformedPt a point in the deformed (i.e., destination) space.
     * @return the corresponding point in the local (i.e., source) space.
     */
    virtual vec3d deformedToLocal(const vec3d &deformedPt) const;

    /**
     * Returns the local bounding box corresponding to the given source disk.
     *
     * @param deformedPt the source disk center in deformed space.
     * @param deformedRadius the source disk radius in deformed space.
     * @return the local bounding box corresponding to the given source disk.
     */
    virtual box2f deformedToLocalBounds(const vec3d &deformedCenter, double deformedRadius) const;

    /**
     * Returns an orthonormal reference frame of the tangent space at the given
     * deformed point. This reference frame is such that its xy plane is the
     * tangent plane, at deformedPt, to the deformed surface corresponding to
     * the local plane z=0. Note that this orthonormal reference frame does
     * <i>not</i> give the differential of the inverse deformation funtion,
     * which in general is not an orthonormal transformation. If p is a deformed
     * point, then deformedToLocalFrame(deformedPt) * p gives the coordinates of
     * p in the orthonormal reference frame defined above.
     *
     * @param deformedPt a point in the deformed (i.e., destination) space.
     * @return the orthonormal reference frame at deformedPt defined above.
     */
    virtual mat4d deformedToTangentFrame(const vec3d &deformedPt) const;

    /**
     * Sets the shader uniforms that are necessary to project on screen the
     * TerrainQuad of the given TerrainNode. This method can set the uniforms
     * that are common to all the quads of the given terrain.
     *
     * @param context the SceneNode to which the TerrainNode belongs. This node
     *      defines the absolute position and orientation of the terrain in
     *      world space (through SceneNode#getLocalToWorld).
     * @param n a TerrainNode.
     */
    virtual void setUniforms(ptr<SceneNode> context, ptr<TerrainNode> n, ptr<Program> prog) const;

    /**
     * Sets the shader uniforms that are necessary to project on screen the
     * given TerrainQuad. This method can set the uniforms that are specific to
     * the given quad.
     *
     * @param context the SceneNode to which the TerrainNode belongs. This node
     *      defines the absolute position and orientation of the terrain in
     *      world space (through SceneNode#getLocalToWorld).
     * @param q a TerrainQuad.
     */
    virtual void setUniforms(ptr<SceneNode> context, ptr<TerrainQuad> q, ptr<Program> prog) const;

    /**
     * Returns the distance in local (i.e., source) space between a point and a
     * bounding box.
     *
     * @param localPt a point in local space.
     * @param localBox a bounding box in local space.
     */
    virtual float getLocalDist(const vec3d &localPt, const box3d &localBox) const;

    /**
     * Returns the visibility of a bounding box in local space, in a view
     * frustum defined in deformed space.
     *
     * @param t a TerrainNode. This is node is used to get the camera position
     *      in local and deformed space with TerrainNode#getLocalCamera and
     *      TerrainNode#getDeformedCamera, as well as the view frustum planes
     *      in deformed space with TerrainNode#getDeformedFrustumPlanes.
     * @param localBox a bounding box in local space.
     * @return the visibility of the bounding box in the view frustum.
     */
    virtual SceneManager::visibility getVisibility(const TerrainNode *t, const box3d &localBox) const;

protected:
    /**
     * The transformation from camera space to screen space.
     */
    mutable mat4f cameraToScreen;

    /**
     * The transformation from local space to screen space.
     */
    mutable mat4d localToScreen;

    /**
     * The transformation from local space to tangent space (in z=0 plane).
     */
    mutable mat3f localToTangent;

    /**
     * The program that contains the uniforms that were set during the last
     * call to setUniforms(ptr<SceneNode>, ptr<TerrainNode>, ...).
     */
    mutable ptr<Program> lastNodeProg;

    /**
     * The program that contains the uniforms that were set during the last
     * call to setUniforms(ptr<SceneNode>, ptr<TerrainQuad>, ...).
     */
    mutable ptr<Program> lastQuadProg;

    /**
     * The coordinates of a TerrainQuad (ox,oy,l,l).
     */
    mutable ptr<Uniform4f> offsetU;

    /**
     * The camera coordinates relatively to a TerrainQuad. This vector is
     * defined as (camera.x - ox) / l, (camera.y - oy) / l), (camera.z -
     * groundHeightAtCamera) / l), 1.
     */
    mutable ptr<Uniform4f> cameraU;

    /**
     * The blending parameters for geomorphing. This vector is defined by
     * (splitDistance + 1, splitDistance - 1).
     */
    mutable ptr<Uniform2f> blendingU;

    /**
     * The transformation from local space to screen space.
     */
    mutable ptr<UniformMatrix4f> localToScreenU;

    /**
     * The transformation from local tile coordinates to tangent space.
     */
    mutable ptr<UniformMatrix3f> tileToTangentU;

    /**
     * The deformed corners of a quad in screen space.
     */
    mutable ptr<UniformMatrix4f> screenQuadCornersU;

    /**
     * The deformed vertical vectors at the corners of a quad,
     * in screen space.
     */
    mutable ptr<UniformMatrix4f> screenQuadVerticalsU;

    virtual void setScreenUniforms(ptr<SceneNode> context, ptr<TerrainQuad> q, ptr<Program> prog) const;
};

}

#endif
