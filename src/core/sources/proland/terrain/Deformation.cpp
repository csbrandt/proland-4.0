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

#include "proland/terrain/Deformation.h"

#include "ork/render/Program.h"
#include "proland/terrain/TerrainNode.h"

using namespace std;

namespace proland
{

Deformation::Deformation()  : Object("Deformation"),
    offsetU(NULL), cameraU(NULL), blendingU(NULL), localToScreenU(NULL),
    screenQuadCornersU(NULL), screenQuadVerticalsU(NULL)
{
}

Deformation::~Deformation()
{
}

vec3d Deformation::localToDeformed(const vec3d &localPt) const
{
    return localPt;
}

mat4d Deformation::localToDeformedDifferential(const vec3d &localPt, bool clamp) const
{
    return mat4d::translate(vec3d(localPt.x, localPt.y, 0.0));
}

vec3d Deformation::deformedToLocal(const vec3d &deformedPt) const
{
    return deformedPt;
}

box2f Deformation::deformedToLocalBounds(const vec3d &deformedCenter, double deformedRadius) const
{
    return box2f(deformedCenter.x - deformedRadius, deformedCenter.x + deformedRadius,
        deformedCenter.y - deformedRadius, deformedCenter.y + deformedRadius);
}

mat4d Deformation::deformedToTangentFrame(const vec3d &deformedPt) const
{
    return mat4d::translate(vec3d(-deformedPt.x, -deformedPt.y, 0.0));
}

void Deformation::setUniforms(ptr<SceneNode> context, ptr<TerrainNode> n, ptr<Program> prog) const
{
    if (lastNodeProg != prog) {
        blendingU = prog->getUniform2f("deformation.blending");
        localToScreenU = prog->getUniformMatrix4f("deformation.localToScreen");
        tileToTangentU = prog->getUniformMatrix3f("deformation.tileToTangent");
        lastNodeProg = prog;
    }

    if (blendingU != NULL) {
        float d1 = n->getSplitDistance() + 1.0f;
        float d2 = 2.0f * n->getSplitDistance();
        blendingU->set(vec2f(d1, d2 - d1));
    }
    cameraToScreen = context->getOwner()->getCameraToScreen().cast<float>();
    localToScreen = context->getOwner()->getCameraToScreen() * context->getLocalToCamera();
    if (localToScreenU != NULL) {
        localToScreenU->setMatrix(localToScreen.cast<float>());
    }
    if (tileToTangentU != NULL) {
        vec3d localCameraPos = n->getLocalCamera();
        vec3d worldCamera = context->getOwner()->getCameraNode()->getWorldPos();
        vec3d deformedCamera = localToDeformed(localCameraPos);
        mat4d A = localToDeformedDifferential(localCameraPos);
        mat4d B = deformedToTangentFrame(worldCamera);
        mat4d ltow = context->getLocalToWorld();
        mat4d ltot = B * ltow * A;
        localToTangent = mat3f(ltot[0][0], ltot[0][1], ltot[0][3],
                               ltot[1][0], ltot[1][1], ltot[1][3],
                               ltot[3][0], ltot[3][1], ltot[3][3]);
    }
}

void Deformation::setUniforms(ptr<SceneNode> context, ptr<TerrainQuad> q, ptr<Program> prog) const
{
    if (lastQuadProg != prog) {
        offsetU = prog->getUniform4f("deformation.offset");
        cameraU = prog->getUniform4f("deformation.camera");
        tileToTangentU = prog->getUniformMatrix3f("deformation.tileToTangent");
        screenQuadCornersU = prog->getUniformMatrix4f("deformation.screenQuadCorners");
        screenQuadVerticalsU = prog->getUniformMatrix4f("deformation.screenQuadVerticals");
        lastQuadProg = prog;
    }

    if (offsetU != NULL) {
        offsetU->set(vec4d(q->ox, q->oy, q->l, q->level).cast<float>());
    }
    if (cameraU != NULL) {
        vec3d camera = q->getOwner()->getLocalCamera();
        cameraU->set(vec4f(float((camera.x - q->ox) / q->l),
            float((camera.y - q->oy) / q->l),
            float((camera.z - TerrainNode::groundHeightAtCamera) / (q->l * q->getOwner()->getDistFactor())),
            camera.z));
    }
    if (tileToTangentU != NULL) {
        vec3d c = q->getOwner()->getLocalCamera();
        mat3f m = localToTangent * mat3f(q->l, 0.0, q->ox - c.x, 0.0, q->l, q->oy - c.y, 0.0, 0.0, 1.0);
        tileToTangentU->setMatrix(m);
    }

    setScreenUniforms(context, q, prog);
}

void Deformation::setScreenUniforms(ptr<SceneNode> context, ptr<TerrainQuad> q, ptr<Program> prog) const
{
    vec3d p0 = vec3d(q->ox, q->oy, 0.0);
    vec3d p1 = vec3d(q->ox + q->l, q->oy, 0.0);
    vec3d p2 = vec3d(q->ox, q->oy + q->l, 0.0);
    vec3d p3 = vec3d(q->ox + q->l, q->oy + q->l, 0.0);

    if (screenQuadCornersU != NULL) {
        mat4d corners = mat4d(
            p0.x, p1.x, p2.x, p3.x,
            p0.y, p1.y, p2.y, p3.y,
            p0.z, p1.z, p2.z, p3.z,
            1.0, 1.0, 1.0, 1.0);
        screenQuadCornersU->setMatrix((localToScreen * corners).cast<float>());
    }

    if (screenQuadVerticalsU != NULL) {
        mat4d verticals = mat4d(
            0.0, 0.0, 0.0, 0.0,
            0.0, 0.0, 0.0, 0.0,
            1.0, 1.0, 1.0, 1.0,
            0.0, 0.0, 0.0, 0.0);
        screenQuadVerticalsU->setMatrix((localToScreen * verticals).cast<float>());
    }
}

float Deformation::getLocalDist(const vec3d &localPt, const box3d &localBox) const
{
    return (float) max(abs(localPt.z - localBox.zmax),
                   max(min(abs(localPt.x - localBox.xmin), abs(localPt.x - localBox.xmax)),
                        min(abs(localPt.y - localBox.ymin), abs(localPt.y - localBox.ymax))));
}

SceneManager::visibility Deformation::getVisibility(const TerrainNode *t, const box3d &localBox) const
{
    // localBox = deformedBox, so we can compare the deformed frustum with it
    return SceneManager::getVisibility(t->getDeformedFrustumPlanes(), localBox);
}

}
