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

#include "proland/dem/ElevationGraphLayer.h"

#include "proland/dem/ElevationMargin.h"
#include "proland/graph/producer/GetCurveDatasTask.h"

namespace proland
{

ElevationGraphLayer::ElevationGraphLayer(const char *name) : GraphLayer(name), CurveDataFactory()
{
}

ElevationGraphLayer::ElevationGraphLayer(const char *name, ptr<GraphProducer> graphProducer, ptr<Program> layerProgram, ptr<TileProducer> elevations, int displayLevel, bool quality, bool storeGraphTiles, bool deform) :
    GraphLayer(name)
{
    init(graphProducer, layerProgram, elevations, displayLevel, quality, storeGraphTiles, deform);
}

ElevationGraphLayer::~ElevationGraphLayer()
{
}

void ElevationGraphLayer::init(ptr<GraphProducer> graphProducer, ptr<Program> layerProgram, ptr<TileProducer> elevations, int displayLevel, bool quality, bool storeGraphTiles, bool deform)
{
    GraphLayer::init(graphProducer, layerProgram, displayLevel, quality, storeGraphTiles, deform);
    CurveDataFactory::init(graphProducer);
    this->elevations = elevations;
}

void ElevationGraphLayer::getReferencedProducers(vector< ptr<TileProducer> > &producers) const
{
    TileLayer::getReferencedProducers(producers);
    producers.push_back(elevations);
}

void ElevationGraphLayer::startCreateTile(int level, int tx, int ty, unsigned int deadline, ptr<Task> task, ptr<TaskGraph> result)
{
    Task *t = NULL;
    if (level >= displayLevel) {
        if (result != NULL) {
            t = new GetCurveDatasTask<GraphLayer>(task.get(), result.get(), this, graphProducer.get(), elevations.get(), this, level, tx, ty, deadline);
            result->addTask(t);
            result->addDependency(task, t);
        }
    }
    GraphLayer::startCreateTile(level, tx, ty, deadline, t == NULL ? task : t, result);
}

void ElevationGraphLayer::stopCreateTile(int level, int tx, int ty)
{
    GraphLayer::stopCreateTile(level, tx, ty);
    CurveDataFactory::releaseCurveData(level, tx, ty);
}

void ElevationGraphLayer::drawCurveAltitude(const vec3d &tileCoords, CurvePtr p, ElevationCurveData *data, float width, float nwidth, float stepLength,
    bool caps, ptr<FrameBuffer> fb, ptr<Program> prog, Mesh<vec4f, unsigned int> &mesh, vec2d *nx, vec2d *ny, vec2d *lx, vec2d *ly)
{
    assert(data != NULL);
    int n = p->getSize();
    vec2d prev = vec2d(0., 0.);
    vec2d cur = p->getXY(0);
    vec2d next = p->getXY(1);
    double prevl = 0.0f;
    double nextl = (next - cur).length();
    double curs = p->getS(0);
    double curz =  data->getAltitude(curs);
    double nexts = p->getS(1);
    double nextz = data->getAltitude(nexts);
    double w = width / 2;

    if (isDeformed()) {
        for (int i = 0; i < n; ++i) {
            double dx, dy;
            if (i == 0) {
                double x = next.x - cur.x;
                double y = next.y - cur.y;
                dx = nx->x * x + ny->x * y;
                dy = nx->y * x + ny->y * y;
                double f = w / ((*lx) * dx + (*ly) * dy).length();
                dx *= f;
                dy *= f;
            } else if (i == n - 1) {
                double x = cur.x - prev.x;
                double y = cur.y - prev.y;
                dx = nx->x * x + ny->x * y;
                dy = nx->y * x + ny->y * y;
                double f = w / ((*lx) * dx + (*ly) * dy).length();
                dx *= f;
                dy *= f;
            } else {
                float dx0 = cur.x - prev.x;
                float dy0 = cur.y - prev.y;
                float dx1 = next.x - cur.x;
                float dy1 = next.y - cur.y;
                float det = dx0 * dy1 - dy0 * dx1;
                if (abs(atan2(det, dx0 * dx1 + dy0 * dy1)) > 0.5) {
                    float k0 = w / ((*lx) * dy0 - (*ly) * dx0).length();
                    float k1 = w / ((*lx) * dy1 - (*ly) * dx1).length();
                    float t = (dy1*(k0*dy0-k1*dy1) - dx1*(k1*dx1-k0*dx0)) / det;
                    dx = -k0*dy0 + t*dx0;
                    dy = k0*dx0 + t*dy0;
                } else {
                    float Dx0 = nx->x * dx0 + ny->x * dy0;
                    float Dy0 = nx->y * dx0 + ny->y * dy0;
                    float f0 = w / ((*lx) * Dx0 + (*ly) * Dy0).length();
                    float Dx1 = nx->x * dx1 + ny->x * dy1;
                    float Dy1 = nx->y * dx1 + ny->y * dy1;
                    float f1 = w / ((*lx) * Dx1 + (*ly) * Dy1).length();
                    dx = 0.5 * (Dx0 * f0 + Dx1 * f1);
                    dy = 0.5 * (Dy0 * f0 + Dy1 * f1);
                }
            }

            if (i == 0) {
                CurvePtr parent = p->getAncestor();
                if (caps && curs == parent->getS(0) && extremity(parent, parent->getStart())) {
                    double f = 1 / nwidth;
                    mesh.setMode(TRIANGLES);
                    mesh.clear();
                    vec2d a = (vec2d(cur.x + dx, cur.y + dy) - tileCoords.xy()) * tileCoords.z;
                    vec2d b = (vec2d(cur.x + dx * f, cur.y + dy * f) - tileCoords.xy()) * tileCoords.z;
                    vec2d c = (vec2d(cur.x - dx * f, cur.y - dy * f) - tileCoords.xy()) * tileCoords.z;
                    vec2d d = (vec2d(cur.x - dx, cur.y - dy) - tileCoords.xy()) * tileCoords.z;
                    mesh.addVertex(vec4f(a.x, a.y, curz, nwidth));
                    mesh.addVertex(vec4f(b.x, b.y, curz, 1));
                    mesh.addVertex(vec4f(c.x, c.y, curz, 1));
                    mesh.addVertex(vec4f(d.x, d.y, curz, nwidth));
                    double Dx = next.x - cur.x;
                    double Dy = next.y - cur.y;
                    float fd = ((*lx) * Dx + (*ly) * Dy).length();
                    double idx = w * Dx / fd * (nwidth - 1) / nwidth;
                    double idy = w * Dy / fd * (nwidth - 1) / nwidth;
                    a = (vec2d(cur.x + dx - idx, cur.y + dy - idy) - tileCoords.xy()) * tileCoords.z;
                    b = (vec2d(cur.x + dx * f - idx, cur.y + dy * f - idy) - tileCoords.xy()) * tileCoords.z;
                    c = (vec2d(cur.x - dx * f - idx, cur.y - dy * f - idy) - tileCoords.xy()) * tileCoords.z;
                    d = (vec2d(cur.x - dx - idx, cur.y - dy - idy) - tileCoords.xy()) * tileCoords.z;
                    mesh.addVertex(vec4f(a.x, a.y, curz, nwidth));
                    mesh.addVertex(vec4f(b.x, b.y, curz, nwidth));
                    mesh.addVertex(vec4f(c.x, c.y, curz, nwidth));
                    mesh.addVertex(vec4f(d.x, d.y, curz, nwidth));

                    mesh.addIndice(0); mesh.addIndice(1); mesh.addIndice(4);
                    mesh.addIndice(4); mesh.addIndice(1); mesh.addIndice(5);
                    mesh.addIndice(1); mesh.addIndice(2); mesh.addIndice(5);
                    mesh.addIndice(5); mesh.addIndice(2); mesh.addIndice(6);
                    mesh.addIndice(2); mesh.addIndice(3); mesh.addIndice(7);
                    mesh.addIndice(7); mesh.addIndice(6); mesh.addIndice(2);

                    fb->draw(prog, mesh);
                }
                mesh.setMode(TRIANGLE_STRIP);
                mesh.clear();
            }
            vec2d a = (vec2d(cur.x + dx, cur.y + dy) - tileCoords.xy()) * tileCoords.z;
            vec2d b = (vec2d(cur.x - dx, cur.y - dy) - tileCoords.xy()) * tileCoords.z;
            mesh.addVertex(vec4f(a.x, a.y, curz, -nwidth));
            mesh.addVertex(vec4f(b.x, b.y, curz, nwidth));
            if (i < n - 1) {
                for (int j = 1; j < floor(nextl / stepLength); ++j) {
                    double c = j * stepLength / nextl;
                    double x = cur.x + c * (next.x - cur.x);
                    double y = cur.y + c * (next.y - cur.y);
                    double s = curs + c * (nexts - curs);
                    double z = data->getAltitude(s);
                    a = (vec2d(x + dx, y + dy) - tileCoords.xy()) * tileCoords.z;
                    b = (vec2d(x - dx, y - dy) - tileCoords.xy()) * tileCoords.z;
                    mesh.addVertex(vec4f(a.x, a.y, z, -nwidth));
                    mesh.addVertex(vec4f(b.x, b.y, z, nwidth));
                }
            }
            if (i == n - 1) {
                fb->draw(prog, mesh);
                CurvePtr parent = p->getAncestor();
                int pn = parent->getSize();
                if (caps && curs == parent->getS(pn - 1) && extremity(parent, parent->getEnd())) {
                    mesh.setMode(TRIANGLES);
                    mesh.clear();
                    double f = 1 / nwidth;
                    vec2d a = (vec2d(cur.x + dx, cur.y + dy) - tileCoords.xy()) * tileCoords.z;
                    vec2d b = (vec2d(cur.x + dx * f, cur.y + dy * f) - tileCoords.xy()) * tileCoords.z;
                    vec2d c = (vec2d(cur.x - dx * f, cur.y - dy * f) - tileCoords.xy()) * tileCoords.z;
                    vec2d d = (vec2d(cur.x - dx, cur.y - dy) - tileCoords.xy()) * tileCoords.z;
                    mesh.addVertex(vec4f(a.x, a.y, curz, nwidth));
                    mesh.addVertex(vec4f(b.x, b.y, curz, 1));
                    mesh.addVertex(vec4f(c.x, c.y, curz, 1));
                    mesh.addVertex(vec4f(d.x, d.y, curz, nwidth));
                    double Dx = cur.x - prev.x;
                    double Dy = cur.y - prev.y;
                    float fd = ((*lx) * Dx + (*ly) * Dy).length();
                    double idx = -w * Dx / fd * (nwidth - 1) / nwidth;
                    double idy = -w * Dy / fd * (nwidth - 1) / nwidth;
                    a = (vec2d(cur.x + dx - idx, cur.y + dy - idy) - tileCoords.xy()) * tileCoords.z;
                    b = (vec2d(cur.x + dx * f - idx, cur.y + dy * f - idy) - tileCoords.xy()) * tileCoords.z;
                    c = (vec2d(cur.x - dx * f - idx, cur.y - dy * f - idy) - tileCoords.xy()) * tileCoords.z;
                    d = (vec2d(cur.x - dx - idx, cur.y - dy - idy) - tileCoords.xy()) * tileCoords.z;
                    mesh.addVertex(vec4f(a.x, a.y, curz, nwidth));
                    mesh.addVertex(vec4f(b.x, b.y, curz, nwidth));
                    mesh.addVertex(vec4f(c.x, c.y, curz, nwidth));
                    mesh.addVertex(vec4f(d.x, d.y, curz, nwidth));

                    mesh.addIndice(0); mesh.addIndice(1); mesh.addIndice(4);
                    mesh.addIndice(4); mesh.addIndice(1); mesh.addIndice(5);
                    mesh.addIndice(1); mesh.addIndice(2); mesh.addIndice(5);
                    mesh.addIndice(5); mesh.addIndice(2); mesh.addIndice(6);
                    mesh.addIndice(2); mesh.addIndice(3); mesh.addIndice(7);
                    mesh.addIndice(7); mesh.addIndice(6); mesh.addIndice(2);

                    fb->draw(prog, mesh);
                }
            }
            prev = cur;
            prevl = nextl;
            cur = next;
            curs = nexts;
            curz = nextz;
            if (i < n - 2) {
                next = p->getXY(i + 2);
                nextl = (next - cur).length();
                nexts = p->getS(i + 2);
                nextz = data->getAltitude(nexts);
            }
        }
    } else {
        for (int i = 0; i < n; ++i) {
            double dx, dy;
            if (i == 0) {
                dx = (next.x - cur.x) / nextl;
                dy = (next.y - cur.y) / nextl;
            } else if (i == n - 1) {
                dx = (cur.x - prev.x) / prevl;
                dy = (cur.y - prev.y) / prevl;
            } else {
                dx = (cur.x - prev.x) / prevl;
                dy = (cur.y - prev.y) / prevl;
                dx += (next.x - cur.x) / nextl;
                dy += (next.y - cur.y) / nextl;
                double l = (dx*dx + dy*dy) * 0.5f;
                dx = dx / l;
                dy = dy / l;
            }
            dx = dx * w;
            dy = dy * w;
            if (i == 0) {
                CurvePtr parent = p->getAncestor();
                if (caps && curs == parent->getS(0) && extremity(parent, parent->getStart())) {
                    mesh.setMode(TRIANGLES);
                    mesh.clear();
                    vec2d a = (vec2d(cur.x - dy, cur.y + dx) - tileCoords.xy()) * tileCoords.z;
                    vec2d b = (vec2d(cur.x - dy/nwidth, cur.y + dx/nwidth) - tileCoords.xy()) * tileCoords.z;
                    vec2d c = (vec2d(cur.x + dy/nwidth, cur.y - dx/nwidth) - tileCoords.xy()) * tileCoords.z;
                    vec2d d = (vec2d(cur.x + dy, cur.y - dx) - tileCoords.xy()) * tileCoords.z;
                    mesh.addVertex(vec4f(a.x, a.y, curz, nwidth));
                    mesh.addVertex(vec4f(b.x, b.y, curz, 1));
                    mesh.addVertex(vec4f(c.x, c.y, curz, 1));
                    mesh.addVertex(vec4f(d.x, d.y, curz, nwidth));
                    double idx = dx * (nwidth - 1) / nwidth;
                    double idy = dy * (nwidth - 1) / nwidth;
                    a = (vec2d(cur.x - dy - idx, cur.y + dx - idy) - tileCoords.xy()) * tileCoords.z;
                    b = (vec2d(cur.x - dy/nwidth - idx, cur.y + dx/nwidth - idy) - tileCoords.xy()) * tileCoords.z;
                    c = (vec2d(cur.x + dy/nwidth - idx, cur.y - dx/nwidth - idy) - tileCoords.xy()) * tileCoords.z;
                    d = (vec2d(cur.x + dy - idx, cur.y - dx - idy) - tileCoords.xy()) * tileCoords.z;
                    mesh.addVertex(vec4f(a.x, a.y, curz, nwidth));
                    mesh.addVertex(vec4f(b.x, b.y, curz, nwidth));
                    mesh.addVertex(vec4f(c.x, c.y, curz, nwidth));
                    mesh.addVertex(vec4f(d.x, d.y, curz, nwidth));

                    mesh.addIndice(0); mesh.addIndice(1); mesh.addIndice(4);
                    mesh.addIndice(4); mesh.addIndice(1); mesh.addIndice(5);
                    mesh.addIndice(1); mesh.addIndice(2); mesh.addIndice(5);
                    mesh.addIndice(5); mesh.addIndice(2); mesh.addIndice(6);
                    mesh.addIndice(2); mesh.addIndice(3); mesh.addIndice(7);
                    mesh.addIndice(7); mesh.addIndice(6); mesh.addIndice(2);

                    fb->draw(prog, mesh);
                }
                mesh.setMode(TRIANGLE_STRIP);
                mesh.clear();
            }
            vec2d a = (vec2d(cur.x - dy, cur.y + dx) - tileCoords.xy()) * tileCoords.z;
            vec2d b = (vec2d(cur.x + dy, cur.y - dx) - tileCoords.xy()) * tileCoords.z;
            mesh.addVertex(vec4f(a.x, a.y, curz, -nwidth));
            mesh.addVertex(vec4f(b.x, b.y, curz, nwidth));
            if (i < n - 1) {
                double nx = w * (next.x - cur.x) / nextl;
                double ny = w * (next.y - cur.y) / nextl;
                for (int j = 1; j < floor(nextl / stepLength); ++j) {
                    double c = j * stepLength / nextl;
                    double x = cur.x + c * (next.x - cur.x);
                    double y = cur.y + c * (next.y - cur.y);
                    double s = curs + c * (nexts - curs);
                    double z = data->getAltitude(s);
                    a = (vec2d(x - ny, y + nx) - tileCoords.xy()) * tileCoords.z;
                    b = (vec2d(x + ny, y - nx) - tileCoords.xy()) * tileCoords.z;
                    mesh.addVertex(vec4f(a.x, a.y, z, -nwidth));
                    mesh.addVertex(vec4f(b.x, b.y, z, nwidth));
                }
            }
            if (i == n - 1) {
                fb->draw(prog, mesh);
                CurvePtr parent = p->getAncestor();
                int pn = parent->getSize();
                if (caps && curs == parent->getS(pn - 1) && extremity(parent, parent->getEnd())) {
                    mesh.setMode(TRIANGLES);
                    mesh.clear();
                    vec2d a = (vec2d(cur.x - dy, cur.y + dx) - tileCoords.xy()) * tileCoords.z;
                    vec2d b = (vec2d(cur.x - dy/nwidth, cur.y + dx/nwidth) - tileCoords.xy()) * tileCoords.z;
                    vec2d c = (vec2d(cur.x + dy/nwidth, cur.y - dx/nwidth) - tileCoords.xy()) * tileCoords.z;
                    vec2d d = (vec2d(cur.x + dy, cur.y - dx) - tileCoords.xy()) * tileCoords.z;
                    mesh.addVertex(vec4f(a.x, a.y, curz, nwidth));
                    mesh.addVertex(vec4f(b.x, b.y, curz, 1));
                    mesh.addVertex(vec4f(c.x, c.y, curz, 1));
                    mesh.addVertex(vec4f(d.x, d.y, curz, nwidth));
                    double idx = -dx * (nwidth - 1) / nwidth;
                    double idy = -dy * (nwidth - 1) / nwidth;
                    a = (vec2d(cur.x - dy - idx, cur.y + dx - idy) - tileCoords.xy()) * tileCoords.z;
                    b = (vec2d(cur.x - dy/nwidth - idx, cur.y + dx/nwidth - idy) - tileCoords.xy()) * tileCoords.z;
                    c = (vec2d(cur.x + dy/nwidth - idx, cur.y - dx/nwidth - idy) - tileCoords.xy()) * tileCoords.z;
                    d = (vec2d(cur.x + dy - idx, cur.y - dx - idy) - tileCoords.xy()) * tileCoords.z;
                    mesh.addVertex(vec4f(a.x, a.y, curz, nwidth));
                    mesh.addVertex(vec4f(b.x, b.y, curz, nwidth));
                    mesh.addVertex(vec4f(c.x, c.y, curz, nwidth));
                    mesh.addVertex(vec4f(d.x, d.y, curz, nwidth));

                    mesh.addIndice(0); mesh.addIndice(1); mesh.addIndice(4);
                    mesh.addIndice(4); mesh.addIndice(1); mesh.addIndice(5);
                    mesh.addIndice(1); mesh.addIndice(2); mesh.addIndice(5);
                    mesh.addIndice(5); mesh.addIndice(2); mesh.addIndice(6);
                    mesh.addIndice(2); mesh.addIndice(3); mesh.addIndice(7);
                    mesh.addIndice(7); mesh.addIndice(6); mesh.addIndice(2);

                    fb->draw(prog, mesh);
                }
            }
            prev = cur;
            prevl = nextl;
            cur = next;
            curs = nexts;
            curz = nextz;
            if (i < n - 2) {
                next = p->getXY(i + 2);
                nextl = (next - cur).length();
                nexts = p->getS(i + 2);
                nextz = data->getAltitude(nexts);
            }
        }
    }
}

void ElevationGraphLayer::swap(ptr<ElevationGraphLayer> p)
{
    GraphLayer::swap(p);
    CurveDataFactory::swap(p.get());
    std::swap(elevations, p->elevations);
}

}
