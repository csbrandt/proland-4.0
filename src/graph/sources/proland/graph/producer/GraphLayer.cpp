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

#include "proland/graph/producer/GraphLayer.h"

#include "proland/producer/CPUTileStorage.h"
#include "proland/graph/Area.h"
#include "proland/graph/GraphListener.h"

namespace proland
{

GraphLayer::GraphLayer(const char *name) : TileLayer(name)
{
    graphProducer = NULL;
    displayLevel = 0;
    storeGraphTiles = false;
    layerProgram = NULL;
}

GraphLayer::GraphLayer(const char *name, ptr<GraphProducer> graphProducer, ptr<Program> layerProgram,int displayLevel, bool quality, bool storeGraphTiles, bool deform) :
    TileLayer(name)
{
    init(graphProducer, layerProgram, displayLevel, quality, storeGraphTiles, deform);
}

GraphLayer::~GraphLayer()
{
    usedTiles.clear();
}

void GraphLayer::init(ptr<GraphProducer> graphProducer, ptr<Program> layerProgram, int displayLevel, bool quality, bool storeGraphTiles, bool deform)
{
    TileLayer::init(deform);
    this->graphProducer = graphProducer;
    this->displayLevel = displayLevel;
    this->storeGraphTiles = storeGraphTiles;
    this->quality = quality;
    this->layerProgram = layerProgram;
}

void GraphLayer::getReferencedProducers(vector< ptr<TileProducer> > &producers) const
{
    producers.push_back(graphProducer);
}

void GraphLayer::useTile(int level, int tx, int ty, unsigned int deadline)
{
    if (storeGraphTiles && level >= displayLevel) {
        TileCache::Tile *t = graphProducer->getTile(level, tx, ty, deadline);
        assert(t != NULL);
    }
}

void GraphLayer::unuseTile(int level, int tx, int ty)
{
    if (storeGraphTiles && level >= displayLevel) {
        TileCache::Tile *t = graphProducer->findTile(level, tx, ty);
        assert(t != NULL);
        graphProducer->putTile(t);
    }
}

void GraphLayer::setTileSize(int tileSize, int tileBorder, float rootQuadSize)
{
    TileLayer::setTileSize(tileSize, tileBorder, rootQuadSize);
    graphProducer->setTileSize(tileSize);
    graphProducer->setRootQuadSize(rootQuadSize);
}

void GraphLayer::prefetchTile(int level, int tx, int ty)
{
    if (level >= displayLevel) {
        graphProducer->prefetchTile(level, tx, ty);
    }
}

void GraphLayer::startCreateTile(int level, int tx, int ty, unsigned int deadline, ptr<Task> task, ptr<TaskGraph> result)
{
    if (level >= displayLevel) {
        TileCache::Tile *t = graphProducer->getTile(level, tx, ty, deadline);
        assert(t != NULL);
        if (result != NULL) {
            result->addTask(t->task);
            result->addDependency(task, t->task);
        }
    }
}

void GraphLayer::beginCreateTile()
{
}

void GraphLayer::endCreateTile()
{
}

void GraphLayer::stopCreateTile(int level, int tx, int ty)
{
    if (level >= displayLevel) {
        TileCache::Tile::Id id = TileCache::Tile::getId(level, tx, ty);
        map<TileCache::Tile::Id, pair<TileProducer *, set<TileCache::Tile*> > >::iterator p = usedTiles.find(id);
        if (p != usedTiles.end()) {
            for (set<TileCache::Tile*>::iterator i = p->second.second.begin(); i != p->second.second.end(); i++) {
                p->second.first->putTile(*i);
            }
            usedTiles.erase(p);
        }

        TileCache::Tile *t = graphProducer->findTile(level, tx, ty);
        assert(t != NULL);
        graphProducer->putTile(t);
    }
}

void GraphLayer::addUsedTiles(int level, int tx, int ty, TileProducer *producer, set<TileCache::Tile*> tiles)
{
    TileCache::Tile::Id id = TileCache::Tile::getId(level, tx, ty);
    usedTiles.insert(make_pair(id, make_pair(producer, tiles)));
}

void GraphLayer::swap(ptr<GraphLayer> p)
{
    TileLayer::swap(p);
    graphProducer->invalidateTiles();
    p->graphProducer->invalidateTiles();
    std::swap(graphProducer, p->graphProducer);
    std::swap(layerProgram, p->layerProgram);
    std::swap(displayLevel, p->displayLevel);
    std::swap(quality, p->quality);
    std::swap(storeGraphTiles, p->storeGraphTiles);
    std::swap(usedTiles, p->usedTiles);
}

void GraphLayer::drawCurve(const vec3d &tileCoords, CurvePtr p, float width, float scale, ptr<FrameBuffer> fb, ptr<Program> prog, Mesh<vec2f, unsigned int> &mesh, vec2d *nx, vec2d *ny, vec2d *lx, vec2d *ly)
{
    int n = p->getSize();
    if (width * scale > 0.0002) {
        float w = width / 2;
        mesh.setMode(TRIANGLE_STRIP);
        mesh.clear();
        if (isDeformed()) {
            vec2d prev = vec2d(0.f, 0.f);
            vec2d cur = p->getXY(0);
            vec2d next = p->getXY(1);
            for (int i = 0; i < n; ++i) {
                float dx, dy;
                if (i == 0) {
                    float x = next.x - cur.x;
                    float y = next.y - cur.y;
                    dx = nx->x * x + ny->x * y;
                    dy = nx->y * x + ny->y * y;
                    float f = w / ((*lx) * dx + (*ly) * dy).length();
                    dx *= f;
                    dy *= f;
                } else if (i == n - 1) {
                    float x = cur.x - prev.x;
                    float y = cur.y - prev.y;
                    dx = nx->x * x + ny->x * y;
                    dy = nx->y * x + ny->y * y;
                    float f = w / ((*lx) * dx + (*ly) * dy).length();
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
                mesh.addVertex(((vec2d(cur.x + dx, cur.y + dy) - tileCoords.xy()) * tileCoords.z).cast<float>());
                mesh.addVertex(((vec2d(cur.x - dx, cur.y - dy) - tileCoords.xy()) * tileCoords.z).cast<float>());
                prev = cur;
                cur = next;
                if (i < n - 2) {
                    next = p->getXY(i + 2);
                }
            }
        } else {
            vec2d prev = vec2d(0.f, 0.f);
            vec2d cur = p->getXY(0);
            vec2d next = p->getXY(1);
            float prevl = 0.0f;
            float nextl = (next - cur).length();
            for (int i = 0; i < n; ++i) {
                float dx, dy;
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
                    float l = (float) ((dx*dx + dy*dy) * 0.5);
                    dx = dx / l;
                    dy = dy / l;
                }
                dx = dx * w;
                dy = dy * w;
                mesh.addVertex(((vec2d(cur.x - dy, cur.y + dx) - tileCoords.xy()) * tileCoords.z).cast<float>());
                mesh.addVertex(((vec2d(cur.x + dy, cur.y - dx) - tileCoords.xy()) * tileCoords.z).cast<float>());
                prev = cur;
                prevl = nextl;
                cur = next;
                if (i < n - 2) {
                    next = p->getXY(i + 2);
                    nextl = (next - cur).length();
                }
            }
        }
    } else {
        mesh.setMode(LINE_STRIP);
        mesh.clear();
        for (int i = 0; i < n; ++i) {
            vec2d cp = (p->getXY(i) - tileCoords.xy()) * tileCoords.z;
            mesh.addVertex(vec2f(cp.x, cp.y));
        }
    }
    fb->draw(prog, mesh);
}

void GraphLayer::drawCurve(const vec3d &tileCoords, CurvePtr p, const vec4f &part, ptr<FrameBuffer> fb, ptr<Program> prog, Mesh<vec4f, unsigned int> &mesh, vec2d *nx, vec2d *ny, vec2d *lx, vec2d *ly)
{
    int n = p->getSize();
    mesh.setMode(TRIANGLE_STRIP);
    mesh.clear();
    if (isDeformed()) {
        float w = p->getWidth() / 2;
        vec2d prev = vec2d(0.f, 0.f);
        vec2d cur = p->getXY(0);
        vec2d next = p->getXY(1);
        for (int i = 0; i < n; ++i) {
            float dx, dy;
            if (i == 0) {
                float x = next.x - cur.x;
                float y = next.y - cur.y;
                dx = nx->x * x + ny->x * y;
                dy = nx->y * x + ny->y * y;
                float f = w / ((*lx) * dx + (*ly) * dy).length();
                dx *= f;
                dy *= f;
            } else if (i == n - 1) {
                float x = cur.x - prev.x;
                float y = cur.y - prev.y;
                dx = nx->x * x + ny->x * y;
                dy = nx->y * x + ny->y * y;
                float f = w / ((*lx) * dx + (*ly) * dy).length();
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
            vec2d a = (vec2d(cur.x + dx * part.x, cur.y + dy * part.x) - tileCoords.xy()) * tileCoords.z;
            vec2d b = (vec2d(cur.x - dx * part.y, cur.y - dy * part.y) - tileCoords.xy()) * tileCoords.z;
            mesh.addVertex(vec4f(a.x, a.y, 0, part.z));
            mesh.addVertex(vec4f(b.x, b.y, 0, part.w));
            prev = cur;
            cur = next;
            if (i < n - 2) {
                next = p->getXY(i + 2);
            }
        }
    } else {
        vec2d prev = vec2d(0.f, 0.f);;
        vec2d cur = p->getXY(0);
        vec2d next = p->getXY(1);
        float prevl = 0.0f;
        float nextl = (next - cur).length();
        for (int i = 0; i < n; ++i) {
            float dx, dy;
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
                float l = (dx*dx + dy*dy) * 0.5f;
                dx = dx / l;
                dy = dy / l;
            }
            vec2d a = (vec2d(cur.x + dy * part.x, cur.y + dx * part.x) - tileCoords.xy()) * tileCoords.z;
            vec2d b = (vec2d(cur.x + dy * part.y, cur.y - dx * part.y) - tileCoords.xy()) * tileCoords.z;
            mesh.addVertex(vec4f(a.x, a.y, 0, part.z));
            mesh.addVertex(vec4f(b.x, b.y, 0, part.w));
            prev = cur;
            prevl = nextl;
            cur = next;
            if (i < n - 2) {
                next = p->getXY(i + 2);
                nextl = (next - cur).length();
            }
        }
    }
    fb->draw(prog, mesh);
}

void GraphLayer::drawCurve(const vec3d &tileCoords, CurvePtr p, CurveData *data, float width, float cap, float scale,
    ptr<FrameBuffer> fb, ptr<Program> prog, Mesh<vec4f, unsigned int> &mesh, vec2d *nx, vec2d *ny, vec2d *lx, vec2d *ly)
{
    int n = p->getSize();
    if (width * scale > 2) {
        float w = width / 2;
        mesh.setMode(TRIANGLE_STRIP);
        mesh.clear();
        if (isDeformed()) {
            vec2d prev = vec2d(0.f, 0.f);
            vec2d cur = p->getXY(0);
            vec2d next = p->getXY(1);
            for (int i = 0; i < n; ++i) {
                float dx, dy;
                if (i == 0) {
                    float x = next.x - cur.x;
                    float y = next.y - cur.y;
                    dx = nx->x * x + ny->x * y;
                    dy = nx->y * x + ny->y * y;
                    float f = w / ((*lx) * dx + (*ly) * dy).length();
                    dx *= f;
                    dy *= f;
                } else if (i == n - 1) {
                    float x = cur.x - prev.x;
                    float y = cur.y - prev.y;
                    dx = nx->x * x + ny->x * y;
                    dy = nx->y * x + ny->y * y;
                    float f = w / ((*lx) * dx + (*ly) * dy).length();
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
                float s = p->getS(i);
                float curvl = data == NULL ? 0 : data->getCurvilinearLength(s);

                if (i == 0 && cap > 0) {
                    CurvePtr parent = p->getAncestor();
                    if (s == parent->getS(0) && p->getStart()->getCurveCount() == 1) {
                        float f = cap / width;
                        mesh.setMode(TRIANGLES);
                        mesh.clear();
                        vec2d a = (vec2d(cur.x + dx, cur.y + dy) - tileCoords.xy()) * tileCoords.z;
                        vec2d b = (vec2d(cur.x + dx * f, cur.y + dy * f) - tileCoords.xy()) * tileCoords.z;
                        vec2d c = (vec2d(cur.x - dx * f, cur.y - dy * f) - tileCoords.xy()) * tileCoords.z;
                        vec2d d = (vec2d(cur.x - dx, cur.y - dy) - tileCoords.xy()) * tileCoords.z;
                        mesh.addVertex(vec4f(a.x, a.y, curvl, w));
                        mesh.addVertex(vec4f(b.x, b.y, curvl, cap / 2));
                        mesh.addVertex(vec4f(c.x, c.y, curvl, cap / 2));
                        mesh.addVertex(vec4f(d.x, d.y, curvl, w));
                        double Dx = cur.x - prev.x;
                        double Dy = cur.y - prev.y;
                        float fd = ((*lx) * Dx + (*ly) * Dy).length();
                        double idx = w * Dx / fd * (width - cap) / width;
                        double idy = w * Dy / fd * (width - cap) / width;
                        a = (vec2d(cur.x + dx - idx, cur.y + dy - idy) - tileCoords.xy()) * tileCoords.z;
                        b = (vec2d(cur.x + dx * f - idx, cur.y + dy * f - idy) - tileCoords.xy()) * tileCoords.z;
                        c = (vec2d(cur.x - dx * f - idx, cur.y - dy * f - idy) - tileCoords.xy()) * tileCoords.z;
                        d = (vec2d(cur.x - dx - idx, cur.y - dy - idy) - tileCoords.xy()) * tileCoords.z;
                        mesh.addVertex(vec4f(a.x, a.y, curvl, w));
                        mesh.addVertex(vec4f(b.x, b.y, curvl, w));
                        mesh.addVertex(vec4f(c.x, c.y, curvl, w));
                        mesh.addVertex(vec4f(d.x, d.y, curvl, w));

                        mesh.addIndice(0); mesh.addIndice(1); mesh.addIndice(4);
                        mesh.addIndice(4); mesh.addIndice(1); mesh.addIndice(5);
                        mesh.addIndice(1); mesh.addIndice(2); mesh.addIndice(5);
                        mesh.addIndice(5); mesh.addIndice(2); mesh.addIndice(6);
                        mesh.addIndice(2); mesh.addIndice(3); mesh.addIndice(7);
                        mesh.addIndice(7); mesh.addIndice(6); mesh.addIndice(2);
                        fb->draw(prog, mesh);

                        mesh.setMode(TRIANGLE_STRIP);
                        mesh.clear();
                    }
                }

                vec2d a = (vec2d(cur.x + dx, cur.y + dy) - tileCoords.xy()) * tileCoords.z;
                vec2d b = (vec2d(cur.x - dx, cur.y - dy) - tileCoords.xy()) * tileCoords.z;
                mesh.addVertex(vec4f(a.x, a.y, curvl, -w));
                mesh.addVertex(vec4f(b.x, b.y, curvl, w));

                if (i == n - 1 && cap > 0) {
                    CurvePtr parent = p->getAncestor();
                    int pn = parent->getSize();
                    if (s == parent->getS(pn - 1) && p->getEnd()->getCurveCount() == 1) {
                        float f = cap / width;
                        fb->draw(prog, mesh);
                        mesh.setMode(TRIANGLES);
                        mesh.clear();
                        vec2d a = (vec2d(cur.x + dx, cur.y + dy) - tileCoords.xy()) * tileCoords.z;
                        vec2d b = (vec2d(cur.x + dx * f, cur.y + dy * f) - tileCoords.xy()) * tileCoords.z;
                        vec2d c = (vec2d(cur.x - dx * f, cur.y - dy * f) - tileCoords.xy()) * tileCoords.z;
                        vec2d d = (vec2d(cur.x - dx, cur.y - dy) - tileCoords.xy()) * tileCoords.z;
                        mesh.addVertex(vec4f(a.x, a.y, curvl, w));
                        mesh.addVertex(vec4f(b.x, b.y, curvl, cap / 2));
                        mesh.addVertex(vec4f(c.x, c.y, curvl, cap / 2));
                        mesh.addVertex(vec4f(d.x, d.y, curvl, w));
                        double Dx = cur.x - prev.x;
                        double Dy = cur.y - prev.y;
                        float fd = ((*lx) * Dx + (*ly) * Dy).length();
                        double idx = -w * Dx / fd * (width - cap) / width;
                        double idy = -w * Dy / fd * (width - cap) / width;
                        a = (vec2d(cur.x + dx - idx, cur.y + dy - idy) - tileCoords.xy()) * tileCoords.z;
                        b = (vec2d(cur.x + dx * f - idx, cur.y + dy * f - idy) - tileCoords.xy()) * tileCoords.z;
                        c = (vec2d(cur.x - dx * f - idx, cur.y - dy * f - idy) - tileCoords.xy()) * tileCoords.z;
                        d = (vec2d(cur.x - dx - idx, cur.y - dy- idy) - tileCoords.xy()) * tileCoords.z;
                        mesh.addVertex(vec4f(a.x, a.y, curvl, w));
                        mesh.addVertex(vec4f(b.x, b.y, curvl, w));
                        mesh.addVertex(vec4f(c.x, c.y, curvl, w));
                        mesh.addVertex(vec4f(d.x, d.y, curvl, w));

                        mesh.addIndice(0); mesh.addIndice(1); mesh.addIndice(4);
                        mesh.addIndice(4); mesh.addIndice(1); mesh.addIndice(5);
                        mesh.addIndice(1); mesh.addIndice(2); mesh.addIndice(5);
                        mesh.addIndice(5); mesh.addIndice(2); mesh.addIndice(6);
                        mesh.addIndice(2); mesh.addIndice(3); mesh.addIndice(7);
                        mesh.addIndice(7); mesh.addIndice(6); mesh.addIndice(2);
                    }
                }
                prev = cur;
                cur = next;
                if (i < n - 2) {
                    next = p->getXY(i + 2);
                }
            }
        } else {
            vec2d prev = vec2d(0.f, 0.f);
            vec2d cur = p->getXY(0);
            vec2d next = p->getXY(1);
            float prevl = 0.0f;
            float nextl = (next - cur).length();
            for (int i = 0; i < n; ++i) {
                float dx, dy;
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
                    float l = (dx*dx + dy*dy) * 0.5f;
                    dx = dx / l;
                    dy = dy / l;
                }
                dx = dx * w;
                dy = dy * w;
                float s = p->getS(i);
                float curvl = data == NULL ? 0 : data->getCurvilinearLength(s);

                if (i == 0 && cap > 0) {
                    CurvePtr parent = p->getAncestor();
                    if (s == parent->getS(0) && p->getStart()->getCurveCount() == 1) {
                        float f = cap / width;
                        mesh.setMode(TRIANGLES);
                        mesh.clear();
                        vec2d a = (vec2d(cur.x - dy, cur.y + dx) - tileCoords.xy()) * tileCoords.z;
                        vec2d b = (vec2d(cur.x - dy * f, cur.y + dx * f) - tileCoords.xy()) * tileCoords.z;
                        vec2d c = (vec2d(cur.x + dy * f, cur.y - dx * f) - tileCoords.xy()) * tileCoords.z;
                        vec2d d = (vec2d(cur.x + dy, cur.y - dx) - tileCoords.xy()) * tileCoords.z;
                        mesh.addVertex(vec4f(a.x, a.y, curvl, w));
                        mesh.addVertex(vec4f(b.x, b.y, curvl, cap / 2));
                        mesh.addVertex(vec4f(c.x, c.y, curvl, cap / 2));
                        mesh.addVertex(vec4f(d.x, d.y, curvl, w));
                        float idx = dx * (width - cap) / width;
                        float idy = dy * (width - cap) / width;
                        a = (vec2d(cur.x - dy - idx, cur.y + dx - idy) - tileCoords.xy()) * tileCoords.z;
                        b = (vec2d(cur.x - dy * f - idx, cur.y + dx * f - idy) - tileCoords.xy()) * tileCoords.z;
                        c = (vec2d(cur.x + dy * f - idx, cur.y - dx * f - idy) - tileCoords.xy()) * tileCoords.z;
                        d = (vec2d(cur.x + dy - idx, cur.y - dx - idy) - tileCoords.xy()) * tileCoords.z;
                        mesh.addVertex(vec4f(a.x, a.y, curvl, w));
                        mesh.addVertex(vec4f(b.x, b.y, curvl, w));
                        mesh.addVertex(vec4f(c.x, c.y, curvl, w));
                        mesh.addVertex(vec4f(d.x, d.y, curvl, w));

                        mesh.addIndice(0); mesh.addIndice(1); mesh.addIndice(4);
                        mesh.addIndice(4); mesh.addIndice(1); mesh.addIndice(5);
                        mesh.addIndice(1); mesh.addIndice(2); mesh.addIndice(5);
                        mesh.addIndice(5); mesh.addIndice(2); mesh.addIndice(6);
                        mesh.addIndice(2); mesh.addIndice(3); mesh.addIndice(7);
                        mesh.addIndice(7); mesh.addIndice(6); mesh.addIndice(2);
                        fb->draw(prog, mesh);

                        mesh.setMode(TRIANGLE_STRIP);
                        mesh.clear();
                    }
                }

                vec2d a = (vec2d(cur.x - dy, cur.y + dx) - tileCoords.xy()) * tileCoords.z;
                vec2d b = (vec2d(cur.x + dy, cur.y - dx) - tileCoords.xy()) * tileCoords.z;
                mesh.addVertex(vec4f(a.x, a.y, curvl, -w));
                mesh.addVertex(vec4f(b.x, b.y, curvl, w));

                if (i == n - 1 && cap > 0) {
                    CurvePtr parent = p->getAncestor();
                    int pn = parent->getSize();
                    if (s == parent->getS(pn - 1) && p->getEnd()->getCurveCount() == 1) {
                        float f = cap / width;
                        fb->draw(prog, mesh);
                        mesh.setMode(TRIANGLES);
                        mesh.clear();
                        vec2d a = (vec2d(cur.x - dy, cur.y + dx) - tileCoords.xy()) * tileCoords.z;
                        vec2d b = (vec2d(cur.x - dy * f, cur.y + dx * f) - tileCoords.xy()) * tileCoords.z;
                        vec2d c = (vec2d(cur.x + dy * f, cur.y - dx * f) - tileCoords.xy()) * tileCoords.z;
                        vec2d d = (vec2d(cur.x + dy, cur.y - dx) - tileCoords.xy()) * tileCoords.z;
                        mesh.addVertex(vec4f(a.x, a.y, curvl, w));
                        mesh.addVertex(vec4f(b.x, b.y, curvl, cap / 2));
                        mesh.addVertex(vec4f(c.x, c.y, curvl, cap / 2));
                        mesh.addVertex(vec4f(d.x, d.y, curvl, w));
                        float idx = -dx * (width - cap) / width;
                        float idy = -dy * (width - cap) / width;
                        a = (vec2d(cur.x - dy - idx, cur.y + dx - idy) - tileCoords.xy()) * tileCoords.z;
                        b = (vec2d(cur.x - dy * f - idx, cur.y + dx * f - idy) - tileCoords.xy()) * tileCoords.z;
                        c = (vec2d(cur.x + dy * f - idx, cur.y - dx * f - idy) - tileCoords.xy()) * tileCoords.z;
                        d = (vec2d(cur.x + dy - idx, cur.y - dx - idy) - tileCoords.xy()) * tileCoords.z;
                        mesh.addVertex(vec4f(a.x, a.y, curvl, w));
                        mesh.addVertex(vec4f(b.x, b.y, curvl, w));
                        mesh.addVertex(vec4f(c.x, c.y, curvl, w));
                        mesh.addVertex(vec4f(d.x, d.y, curvl, w));

                        mesh.addIndice(0); mesh.addIndice(1); mesh.addIndice(4);
                        mesh.addIndice(4); mesh.addIndice(1); mesh.addIndice(5);
                        mesh.addIndice(1); mesh.addIndice(2); mesh.addIndice(5);
                        mesh.addIndice(5); mesh.addIndice(2); mesh.addIndice(6);
                        mesh.addIndice(2); mesh.addIndice(3); mesh.addIndice(7);
                        mesh.addIndice(7); mesh.addIndice(6); mesh.addIndice(2);
                    }
                }

                prev = cur;
                prevl = nextl;
                cur = next;
                if (i < n - 2) {
                    next = p->getXY(i + 2);
                    nextl = (next - cur).length();
                }
            }
        }
        fb->draw(prog, mesh);
    } else {
        mesh.setMode(LINE_STRIP);
        mesh.clear();
        for (int i = 0; i < n; ++i) {
            vec2d cp = (p->getXY(i) - tileCoords.xy()) * tileCoords.z;
            mesh.addVertex(vec4f(cp.x, cp.y, 0, 0));
        }
        fb->draw(prog, mesh);
    }
}

void GraphLayer::drawCurve(const vec3d &tileCoords, CurvePtr p, CurveData *data, float l0, float l1, float width, float scale, ptr<FrameBuffer> fb, ptr<Program> prog, Mesh<vec4f, unsigned int> &mesh, vec2d *nx, vec2d *ny, vec2d *lx, vec2d *ly)
{
    int n = p->getSize();
    if (width * scale > 2) {
        float w = width / 2;
        mesh.setMode(TRIANGLE_STRIP);
        mesh.clear();

        if (isDeformed()) {
            vec2d prev = vec2d(0.f, 0.f);
            vec2d cur = p->getXY(0);
            vec2d next = p->getXY(1);

            float prevl = 0.0f;
            float nextl = (next - cur).length();
            for (int i = 0; i < n; ++i) {
                float dx, dy;
                if (i == 0) {
                    float x = next.x - cur.x;
                    float y = next.y - cur.y;
                    dx = nx->x * x + ny->x * y;
                    dy = nx->y * x + ny->y * y;
                    float f = w / ((*lx) * dx + (*ly) * dy).length();
                    dx *= f;
                    dy *= f;
                } else if (i == n - 1) {
                    float x = cur.x - prev.x;
                    float y = cur.y - prev.y;
                    dx = nx->x * x + ny->x * y;
                    dy = nx->y * x + ny->y * y;
                    float f = w / ((*lx) * dx + (*ly) * dy).length();
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
                float s = p->getS(i);
                float ltot = l1 - l0;
                float k = ((int(ltot)/5 + 1)*5)/ltot;
                float curvl = data->getCurvilinearLength(s);
                if (curvl < l0) {
                    float nextcurvl = curvl + nextl; //theoretically getCurvCoord(nexts)!
                    if (nextcurvl >= l0 && i < n - 1) {
                        float x = next.x - cur.x;
                        float y = next.y - cur.y;
                        dx = x / nextl;
                        dy = y / nextl;
                        float curx = cur.x + dx * (l0 - curvl);
                        float cury = cur.y + dy * (l0 - curvl);
                        dx = nx->x * x + ny->x * y;
                        dy = nx->y * x + ny->y * y;
                        float f = w / ((*lx) * dx + (*ly) * dy).length();
                        dx *= f;
                        dy *= f;
                        vec2d a = (vec2d(curx + dx, cury + dy) - tileCoords.xy()) * tileCoords.z;
                        vec2d b = (vec2d(curx - dx, cury - dy) - tileCoords.xy()) * tileCoords.z;
                        mesh.addVertex(vec4f(a.x, a.y, l0 * k, -w));
                        mesh.addVertex(vec4f(b.x, b.y, l0 * k, w));
                    }
                }
                if (curvl > l1) {
                    float prevcurvl = curvl - prevl; //theoretically getCurvCoord(prevs)!
                    if (prevcurvl <= l1 && i != 0) {
                        float x = cur.x - prev.x;
                        float y = cur.y - prev.y;
                        dx = x / prevl;
                        dy = y / prevl;
                        float curx = prev.x + dx * (l1 - prevcurvl);
                        float cury = prev.y + dy * (l1 - prevcurvl);
                        dx = nx->x * x + ny->x * y;
                        dy = nx->y * x + ny->y * y;
                        float f = w / ((*lx) * dx + (*ly) * dy).length();
                        dx *= f;
                        dy *= f;
                        vec2d a = (vec2d(curx + dx, cury + dy) - tileCoords.xy()) * tileCoords.z;
                        vec2d b = (vec2d(curx - dx, cury - dy) - tileCoords.xy()) * tileCoords.z;
                        mesh.addVertex(vec4f(a.x, a.y, l1 * k, -w));
                        mesh.addVertex(vec4f(b.x, b.y, l1 * k, w));
                        break;
                    }
                }
               if (curvl >= l0 && curvl <= l1) {
                    vec2d a = (vec2d(cur.x + dx, cur.y + dy) - tileCoords.xy()) * tileCoords.z;
                    vec2d b = (vec2d(cur.x - dx, cur.y - dy) - tileCoords.xy()) * tileCoords.z;
                    mesh.addVertex(vec4f(a.x, a.y, curvl * k, -w));
                    mesh.addVertex(vec4f(b.x, b.y, curvl * k, w));
                }
                prev = cur;
                prevl = nextl;
                cur = next;
                if (i < n - 2) {
                    next = p->getXY(i + 2);
                    nextl = (next - cur).length();
                }
            }
        } else {
            vec2d prev = vec2d(0.f, 0.f);
            vec2d cur = p->getXY(0);
            vec2d next = p->getXY(1);

            float prevl = 0.0f;
            float nextl = (next - cur).length();
            for (int i = 0; i < n; ++i) {
                float dx, dy;
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
                    float l = (dx*dx + dy*dy) * 0.5f;
                    dx = dx / l;
                    dy = dy / l;
                }
                dx = dx * w;
                dy = dy * w;
                float s = p->getS(i);
                float ltot = l1 - l0;
                float k = ((int(ltot)/5 + 1)*5)/ltot;
                float curvl = data->getCurvilinearLength(s);
                if (curvl < l0) {
                    float nextcurvl = curvl + nextl; //theoretically getCurvCoord(nexts)!
                    if (nextcurvl >= l0 && i < n - 1) {
                        dx = (next.x - cur.x) / nextl;
                        dy = (next.y - cur.y) / nextl;
                        float curx = cur.x + dx * (l0 - curvl);
                        float cury = cur.y + dy * (l0 - curvl);
                        dx = dx * w;
                        dy = dy * w;
                        vec2d a = (vec2d(curx - dy, cury + dx) - tileCoords.xy()) * tileCoords.z;
                        vec2d b = (vec2d(curx + dy, cury - dx) - tileCoords.xy()) * tileCoords.z;
                        mesh.addVertex(vec4f(a.x, a.y, l0 * k, -w));
                        mesh.addVertex(vec4f(b.x, b.y, l0 * k, w));
                    }
                }
                if (curvl > l1) {
                    float prevcurvl = curvl - prevl; //theoretically getCurvCoord(prevs)!
                    if (prevcurvl <= l1 && i != 0) {
                        dx = (cur.x - prev.x) / prevl;
                        dy = (cur.y - prev.y) / prevl;
                        float curx = prev.x + dx * (l1 - prevcurvl);
                        float cury = prev.y + dy * (l1 - prevcurvl);
                        dx = dx * w;
                        dy = dy * w;
                        vec2d a = (vec2d(curx - dy, cury + dx) - tileCoords.xy()) * tileCoords.z;
                        vec2d b = (vec2d(curx + dy, cury - dx) - tileCoords.xy()) * tileCoords.z;
                        mesh.addVertex(vec4f(a.x, a.y, l1 * k, -w));
                        mesh.addVertex(vec4f(b.x, b.y, l1 * k, w));
                        break;
                    }
                }
                if (curvl >= l0 && curvl <= l1) {
                    vec2d a = (vec2d(cur.x - dy, cur.y + dx) - tileCoords.xy()) * tileCoords.z;
                    vec2d b = (vec2d(cur.x + dy, cur.y - dx) - tileCoords.xy()) * tileCoords.z;
                    mesh.addVertex(vec4f(a.x, a.y, curvl * k, -w));
                    mesh.addVertex(vec4f(b.x, b.y, curvl * k, w));
                }
                prev = cur;
                prevl = nextl;
                cur = next;
                if (i < n - 2) {
                    next = p->getXY(i + 2);
                    nextl = (next - cur).length();
                }
            }
        }
    } else {
        mesh.setMode(LINE_STRIP);
        mesh.clear();
        for (int i = 0; i < n; ++i) {
            vec2d cp = (p->getXY(i) - tileCoords.xy()) * tileCoords.z;
            mesh.addVertex(vec4f(cp.x, cp.y, 0, 0));
        }
    }
    fb->draw(prog, mesh);
}

bool GraphLayer::extremity(CurvePtr curve, NodePtr p)
{
    if (p->getCurveCount() == 1) {
        return true;
    }
    for (int i = 0; i < p->getCurveCount(); ++i) {
        CurvePtr c = p->getCurve(i);
        if (c != curve) {
            return false;
        }
    }
    return true;
}

void GraphLayer::drawArea(const vec3d &tileCoords, AreaPtr a, Tesselator &tess)
{
    tess.beginContour();
    for (int j = 0; j < a->getCurveCount(); ++j) {
        int orientation;
        CurvePtr p = a->getCurve(j, orientation);
        if (orientation == 0) {
            for (int k = 0; k < p->getSize(); ++k) {
                vec2d cp = (p->getXY(k) - tileCoords.xy()) * tileCoords.z;
                tess.newVertex(cp.x, cp.y);
            }
        } else {
            for (int k = p->getSize() - 1; k >= 0; --k) {
                vec2d cp = (p->getXY(k) - tileCoords.xy()) * tileCoords.z;
                tess.newVertex(cp.x, cp.y);
            }
        }
    }
    tess.endContour();
}

}
