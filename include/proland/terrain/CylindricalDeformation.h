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

#ifndef _PROLAND_CYLINDRICAL_DEFORMATION_H_
#define _PROLAND_CYLINDRICAL_DEFORMATION_H_

#include "proland/terrain/Deformation.h"

namespace proland
{

/**
 * A Deformation of space transforming planes to cylinders. This deformation
 * transforms the plane z=0 into a cylinder of radius R. The
 * deformation of p=(x,y,z) in local space is q=(x, r.sin(a), r.cos(a)),
 * where r=R-z and a=y/R.
 * @ingroup terrain
 * @authors Eric Bruneton, Antoine Begault
 */
PROLAND_API class CylindricalDeformation : public Deformation
{
public:
    /**
     * The radius of the cylinder into which the plane z=0 must be deformed.
     */
    const float R;

    /**
     * Creates a new CylindricalDeformation.
     *
     * @param R the radius of the cylinder into which the plane z=0 must be
     *      deformed.
     */
    CylindricalDeformation(float R);

    /**
     * Deletes this CylindricalDeformation.
     */
    virtual ~CylindricalDeformation();

    virtual vec3d localToDeformed(const vec3d &localPt) const;

    virtual mat4d localToDeformedDifferential(const vec3d &localPt, bool clamp = false) const;

    virtual vec3d deformedToLocal(const vec3d &worldPt) const;

    virtual box2f deformedToLocalBounds(const vec3d &deformedCenter, double deformedRadius) const;

    virtual mat4d deformedToTangentFrame(const vec3d &deformedPt) const;

    virtual void setUniforms(ptr<SceneNode> context, ptr<TerrainNode> n, ptr<Program> prog) const;

    virtual SceneManager::visibility getVisibility(const TerrainNode *t, const box3d &localBox) const;

private:
    mutable ptr<UniformMatrix4f> localToWorldU;

    mutable ptr<Uniform1f> radiusU;

    static SceneManager::visibility getVisibility(const vec4d &clip, const vec3d b[4], float f);
};

}

#endif
