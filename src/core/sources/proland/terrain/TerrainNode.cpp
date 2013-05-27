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

#include "proland/terrain/TerrainNode.h"

#include "ork/resource/ResourceTemplate.h"
#include "ork/render/FrameBuffer.h"
#include "proland/terrain/SphericalDeformation.h"
#include "proland/terrain/CylindricalDeformation.h"

#include "pmath.h"

#define HORIZON_SIZE 256

using namespace std;
using namespace ork;

namespace proland
{

float TerrainNode::groundHeightAtCamera = 0.0f;

float TerrainNode::nextGroundHeightAtCamera = 0.0f;

TerrainNode::TerrainNode(ptr<Deformation> deform, ptr<TerrainQuad> root, float splitFactor, int maxLevel) :
    Object("TerrainNode")
{
    init(deform, root, splitFactor, maxLevel);
}

TerrainNode::TerrainNode() : Object("TerrainNode")
{
}

void TerrainNode::init(ptr<Deformation> deform, ptr<TerrainQuad> root, float splitFactor, int maxLevel)
{
    this->deform = deform;
    this->root = root;
    this->splitFactor = splitFactor;
    this->splitInvisibleQuads = false;
    this->horizonCulling = true;
    this->splitDist = 1.1f;
    this->maxLevel = maxLevel;
    root->owner = this;
    horizon = new float[HORIZON_SIZE];
}

TerrainNode::~TerrainNode()
{
    delete[] horizon;
}

vec3d TerrainNode::getDeformedCamera() const
{
    return deformedCameraPos;
}

const vec4d *TerrainNode::getDeformedFrustumPlanes() const
{
    return deformedFrustumPlanes;
}

vec3d TerrainNode::getLocalCamera() const
{
    return localCameraPos;
}

float TerrainNode::getCameraDist(const box3d &localBox) const
{
    return (float) max(abs(localCameraPos.z - localBox.zmax) / distFactor,
                   max(min(abs(localCameraPos.x - localBox.xmin), abs(localCameraPos.x - localBox.xmax)),
                        min(abs(localCameraPos.y - localBox.ymin), abs(localCameraPos.y - localBox.ymax))));
}

SceneManager::visibility TerrainNode::getVisibility(const box3d &localBox) const
{
    return deform->getVisibility(this, localBox);
}

float TerrainNode::getSplitDistance() const
{
    assert(isFinite(splitDist));
    assert(splitDist > 1.0f);
    return splitDist;
}

float TerrainNode::getDistFactor() const
{
    return distFactor;
}

void TerrainNode::update(ptr<SceneNode> owner)
{
    deformedCameraPos = owner->getLocalToCamera().inverse() * vec3d::ZERO;
    SceneManager::getFrustumPlanes(owner->getLocalToScreen(), deformedFrustumPlanes);
    localCameraPos = deform->deformedToLocal(deformedCameraPos);

    mat4d m = deform->localToDeformedDifferential(localCameraPos, true);
    distFactor = max(vec3d(m[0][0], m[1][0], m[2][0]).length(), vec3d(m[0][1], m[1][1], m[2][1]).length());

    ptr<FrameBuffer> fb = SceneManager::getCurrentFrameBuffer();
    vec3d left = deformedFrustumPlanes[0].xyz().normalize();
    vec3d right = deformedFrustumPlanes[1].xyz().normalize();
    float fov = (float) safe_acos(-left.dotproduct(right));
    splitDist = splitFactor * fb->getViewport().z / 1024.0f * tan(40.0f / 180.0f * M_PI) / tan(fov / 2.0f);
    if (splitDist < 1.1f || !(isFinite(splitDist))) {
        splitDist = 1.1f;
    }

    // initializes data structures for horizon occlusion culling
    if (horizonCulling && localCameraPos.z <= root->zmax) {
        vec3d deformedDir = owner->getLocalToCamera().inverse() * vec3d::UNIT_Z;
        vec2d localDir = (deform->deformedToLocal(deformedDir) - localCameraPos).xy().normalize();
        localCameraDir = mat2f(localDir.y, -localDir.x, -localDir.x, -localDir.y);
        for (int i = 0; i < HORIZON_SIZE; ++i) {
            horizon[i] = -INFINITY;
        }
    }

    root->update();
}

bool TerrainNode::addOccluder(const box3d &occluder)
{
    if (!horizonCulling || localCameraPos.z > root->zmax) {
        return false;
    }
    vec2f corners[4];
    vec2d o = localCameraPos.xy();
    corners[0] = localCameraDir * (vec2d(occluder.xmin, occluder.ymin) - o).cast<float>();
    corners[1] = localCameraDir * (vec2d(occluder.xmin, occluder.ymax) - o).cast<float>();
    corners[2] = localCameraDir * (vec2d(occluder.xmax, occluder.ymin) - o).cast<float>();
    corners[3] = localCameraDir * (vec2d(occluder.xmax, occluder.ymax) - o).cast<float>();
    if (corners[0].y <= 0.0f || corners[1].y <= 0.0f || corners[2].y <= 0.0f || corners[3].y <= 0.0f) {
        // skips bounding boxes that are not fully behind the "near plane"
        // of the reference frame used for horizon occlusion culling
        return false;
    }
    float dzmin = float(occluder.zmin - localCameraPos.z);
    float dzmax = float(occluder.zmax - localCameraPos.z);
    vec3f bounds[4];
    bounds[0] = vec3f(corners[0].x, dzmin, dzmax) / corners[0].y;
    bounds[1] = vec3f(corners[1].x, dzmin, dzmax) / corners[1].y;
    bounds[2] = vec3f(corners[2].x, dzmin, dzmax) / corners[2].y;
    bounds[3] = vec3f(corners[3].x, dzmin, dzmax) / corners[3].y;
    float xmin = min(min(bounds[0].x, bounds[1].x), min(bounds[2].x, bounds[3].x)) * 0.33f + 0.5f;
    float xmax = max(max(bounds[0].x, bounds[1].x), max(bounds[2].x, bounds[3].x)) * 0.33f + 0.5f;
    float zmin = min(min(bounds[0].y, bounds[1].y), min(bounds[2].y, bounds[3].y));
    float zmax = max(max(bounds[0].z, bounds[1].z), max(bounds[2].z, bounds[3].z));

    int imin = max(int(floor(xmin * HORIZON_SIZE)), 0);
    int imax = min(int(ceil(xmax * HORIZON_SIZE)), HORIZON_SIZE - 1);

    // first checks if the bounding box projection is below the current horizon line
    bool occluded = imax >= imin;
    for (int i = imin; i <= imax; ++i) {
        if (zmax > horizon[i]) {
            occluded = false;
            break;
        }
    }
    if (!occluded) {
        // if it is not, updates the horizon line with the projection of this bounding box
        imin = max(int(ceil(xmin * HORIZON_SIZE)), 0);
        imax = min(int(floor(xmax * HORIZON_SIZE)), HORIZON_SIZE - 1);
        for (int i = imin; i <= imax; ++i) {
            horizon[i] = max(horizon[i], zmin);
        }
    }
    return occluded;
}

bool TerrainNode::isOccluded(const box3d &box)
{
    if (!horizonCulling || localCameraPos.z > root->zmax) {
        return false;
    }
    vec2f corners[4];
    vec2d o = localCameraPos.xy();
    corners[0] = localCameraDir * (vec2d(box.xmin, box.ymin) - o).cast<float>();
    corners[1] = localCameraDir * (vec2d(box.xmin, box.ymax) - o).cast<float>();
    corners[2] = localCameraDir * (vec2d(box.xmax, box.ymin) - o).cast<float>();
    corners[3] = localCameraDir * (vec2d(box.xmax, box.ymax) - o).cast<float>();
    if (corners[0].y <= 0.0f || corners[1].y <= 0.0f || corners[2].y <= 0.0f || corners[3].y <= 0.0f) {
        return false;
    }
    float dz = float(box.zmax - localCameraPos.z);
    corners[0] = vec2f(corners[0].x, dz) / corners[0].y;
    corners[1] = vec2f(corners[1].x, dz) / corners[1].y;
    corners[2] = vec2f(corners[2].x, dz) / corners[2].y;
    corners[3] = vec2f(corners[3].x, dz) / corners[3].y;
    float xmin = min(min(corners[0].x, corners[1].x), min(corners[2].x, corners[3].x)) * 0.33f + 0.5f;
    float xmax = max(max(corners[0].x, corners[1].x), max(corners[2].x, corners[3].x)) * 0.33f + 0.5f;
    float zmax = max(max(corners[0].y, corners[1].y), max(corners[2].y, corners[3].y));
    int imin = max(int(floor(xmin * HORIZON_SIZE)), 0);
    int imax = min(int(ceil(xmax * HORIZON_SIZE)), HORIZON_SIZE - 1);
    for (int i = imin; i <= imax; ++i) {
        if (zmax > horizon[i]) {
            return false;
        }
    }
    return imax >= imin;
}

void TerrainNode::swap(ptr<TerrainNode> t)
{
    std::swap(deform, t->deform);
    std::swap(root, t->root);
    std::swap(splitFactor, t->splitFactor);
    std::swap(maxLevel, t->maxLevel);
    std::swap(deformedCameraPos, t->deformedCameraPos);
    std::swap(localCameraPos, t->localCameraPos);
    std::swap(splitDist, t->splitDist);

    for (int i = 0; i < 6; ++i) {
        std::swap(deformedFrustumPlanes[i], t->deformedFrustumPlanes[i]);
    }
}

class TerrainNodeResource : public ResourceTemplate<0, TerrainNode>
{
public:
    TerrainNodeResource(ptr<ResourceManager> manager, const string &name, ptr<ResourceDescriptor> desc, const TiXmlElement *e = NULL) :
        ResourceTemplate<0, TerrainNode>(manager, name, desc)
    {
        e = e == NULL ? desc->descriptor : e;
        float size;
        float zmin;
        float zmax;
        ptr<Deformation> deform;
        float splitFactor;
        int maxLevel;
        checkParameters(desc, e, "name,size,zmin,zmax,deform,radius,splitFactor,horizonCulling,maxLevel,");
        getFloatParameter(desc, e, "size", &size);
        getFloatParameter(desc, e, "zmin", &zmin);
        getFloatParameter(desc, e, "zmax", &zmax);
        if (e->Attribute("deform") != NULL && strcmp(e->Attribute("deform"), "sphere") == 0) {
            deform = new SphericalDeformation(size);
        }
        if (e->Attribute("deform") != NULL && strcmp(e->Attribute("deform"), "cylinder") == 0) {
            float radius;
            getFloatParameter(desc, e, "radius", &radius);
            deform = new CylindricalDeformation(radius);
        }
        if (deform == NULL) {
            deform = new Deformation();
        }
        getFloatParameter(desc, e, "splitFactor", &splitFactor);
        getIntParameter(desc, e, "maxLevel", &maxLevel);

        ptr<TerrainQuad> root = new TerrainQuad(NULL, NULL, 0, 0, -size, -size, 2.0 * size, zmin, zmax);
        init(deform, root, splitFactor, maxLevel);

        if (e->Attribute("horizonCulling") != NULL && strcmp(e->Attribute("horizonCulling"), "false") == 0) {
            horizonCulling = false;
        }
    }
};

extern const char terrainNode[] = "terrainNode";

static ResourceFactory::Type<terrainNode, TerrainNodeResource> TerrainNodeType;

}
