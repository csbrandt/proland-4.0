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

#include "proland/util/CylinderViewController.h"

namespace proland
{

CylinderViewController::CylinderViewController(ptr<SceneNode> node, double R) :
    TerrainViewController(node, R * 0.9), R(R)
{
}

CylinderViewController::~CylinderViewController()
{
}

double CylinderViewController::getHeight()
{
    return R - sqrt(position.y * position.y + position.z * position.z);
}

void CylinderViewController::move(vec3d &oldp, vec3d &p)
{
    double oldlon = atan2(oldp.z, oldp.y);
    double lon = atan2(p.z, p.y);
    x0 -= p.x - oldp.x;
    y0 -= lon - oldlon;
}

void CylinderViewController::update()
{
    double ca = cos(y0);
    double sa = sin(y0);
    vec3d po = vec3d(x0, sa * (R - groundHeight), -ca * (R - groundHeight));
    vec3d px = vec3d(1.0, 0.0, 0.0);
    vec3d py = vec3d(0.0, ca, sa);
    vec3d pz = vec3d(0.0, -sa, ca);

    double ct = cos(theta);
    double st = sin(theta);
    double cp = cos(phi);
    double sp = sin(phi);
    vec3d cx = px * cp + py * sp;
    vec3d cy = -px * sp*ct + py * cp*ct + pz * st;
    vec3d cz = px * sp*st - py * cp*st + pz * ct;
    position = po + cz * d * zoom;

    double l = sqrt(position.y * position.y + position.z * position.z);
    if (l > R - 1.0 - groundHeight) {
        position.y = position.y * (R - 1.0 - groundHeight) / l;
        position.z = position.z * (R - 1.0 - groundHeight) / l;
    }

    mat4d view(cx.x, cx.y, cx.z, 0.0,
            cy.x, cy.y, cy.z, 0.0,
            cz.x, cz.y, cz.z, 0.0,
            0.0, 0.0, 0.0, 1.0);

    view = view * mat4d::translate(-position);

    node->setLocalToParent(view.inverse());
}

}
