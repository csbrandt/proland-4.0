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

#include "proland/util/PlanetViewController.h"

using namespace std;

namespace proland
{

PlanetViewController::PlanetViewController(ptr<SceneNode> node, double R) :
    TerrainViewController(node, 6.0 * R), R(R)
{
}

PlanetViewController::~PlanetViewController()
{
}

double PlanetViewController::getHeight()
{
    return position.length() - R;
}

void PlanetViewController::move(vec3d &oldp, vec3d &p)
{
    oldp = oldp.normalize();
    p = p.normalize();
    double oldlat = safe_asin(oldp.z);
    double oldlon = atan2(oldp.y, oldp.x);
    double lat = safe_asin(p.z);
    double lon = atan2(p.y, p.x);
    x0 -= lon - oldlon;
    y0 -= lat - oldlat;
    y0 = max(-M_PI / 2.0, min(M_PI / 2.0, y0));
}

void PlanetViewController::moveForward(double distance)
{
    double co = cos(x0); // x0 = longitude
    double so = sin(x0);
    double ca = cos(y0); // y0 = latitude
    double sa = sin(y0);
    vec3d po = vec3d(co*ca, so*ca, sa) * R;
    vec3d px = vec3d(-so, co, 0.0);
    vec3d py = vec3d(-co*sa, -so*sa, ca);
    vec3d pd = (po - px * sin(phi) * distance + py * cos(phi) * distance).normalize();
    x0 = atan2(pd.y, pd.x);
    y0 = safe_asin(pd.z);
}

void PlanetViewController::turn(double angle)
{
    double co = cos(x0); // x0 = longitude
    double so = sin(x0);
    double ca = cos(y0); // y0 = latitude
    double sa = sin(y0);
    double l = d * sin(theta);
    vec3d po = vec3d(co*ca, so*ca, sa) * R;
    vec3d px = vec3d(-so, co, 0.0);
    vec3d py = vec3d(-co*sa, -so*sa, ca);
    vec3d f = -px * sin(phi) + py * cos(phi);
    vec3d r = px * cos(phi) + py * sin(phi);
    vec3d pd = (po + f * (cos(angle) - 1.0) * l - r * sin(angle) * l).normalize();
    x0 = atan2(pd.y, pd.x);
    y0 = safe_asin(pd.z);
    phi += angle;
}

double PlanetViewController::interpolate(double sx0, double sy0, double stheta, double sphi, double sd,
            double dx0, double dy0, double dtheta, double dphi, double dd, double t)
{
    vec3d s = vec3d(cos(sx0) * cos(sy0), sin(sx0) * cos(sy0), sin(sy0));
    vec3d e = vec3d(cos(dx0) * cos(dy0), sin(dx0) * cos(dy0), sin(dy0));
    double dist = max(safe_acos(s.dotproduct(e)) * R, 1e-3);

    t = min(t + min(0.1, 5000.0 / dist), 1.0);
    double T = 0.5 * atan(4.0 * (t - 0.5)) / atan(4.0 * 0.5) + 0.5;

    interpolateDirection(sx0, sy0, dx0, dy0, T, x0, y0);
    interpolateDirection(sphi, stheta, dphi, dtheta, T, phi, theta);

    const double W = 10.0;
    d = sd * (1.0 - t) + dd * t + dist * (exp(-W * (t - 0.5) * (t - 0.5)) - exp(-W * 0.25));

    return t;
}

void PlanetViewController::interpolatePos(double sx0, double sy0, double dx0, double dy0, double t, double &x0, double &y0)
{
    interpolateDirection(sx0, sy0, dx0, dy0, t, x0, y0);
}

void PlanetViewController::update()
{
    double co = cos(x0); // x0 = longitude
    double so = sin(x0);
    double ca = cos(y0); // y0 = latitude
    double sa = sin(y0);
    vec3d po = vec3d(co*ca, so*ca, sa) * (R + groundHeight);
    vec3d px = vec3d(-so, co, 0.0);
    vec3d py = vec3d(-co*sa, -so*sa, ca);
    vec3d pz = vec3d(co*ca, so*ca, sa);

    double ct = cos(theta);
    double st = sin(theta);
    double cp = cos(phi);
    double sp = sin(phi);
    vec3d cx = px * cp + py * sp;
    vec3d cy = -px * sp*ct + py * cp*ct + pz * st;
    vec3d cz = px * sp*st - py * cp*st + pz * ct;
    position = po + cz * d * zoom;

    if (position.length() < R + 0.5 + groundHeight) {
        position = position.normalize(R + 0.5 + groundHeight);
    }

    mat4d view(cx.x, cx.y, cx.z, 0.0,
            cy.x, cy.y, cy.z, 0.0,
            cz.x, cz.y, cz.z, 0.0,
            0.0, 0.0, 0.0, 1.0);

    view = view * mat4d::translate(-position);

    node->setLocalToParent(view.inverse());
}

}
