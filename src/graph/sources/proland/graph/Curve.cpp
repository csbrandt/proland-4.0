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

#include "proland/graph/Curve.h"

#include "proland/graph/BasicCurvePart.h"
#include "proland/graph/Area.h"

namespace proland
{

Curve::Curve(Graph* owner) :
    Object("Curve"), owner(owner), parent(NULL), type(0), width(0), s0(-1), s1(-1), l(-1), start(NULL), end(NULL), bounds(NULL)

{
    this->id.id = NULL_ID;
    this->area1.id = NULL_ID;
    this->area2.id = NULL_ID;
    this->vertices = new vector<Vertex> ();
}

Curve::Curve(Graph* owner, CurvePtr c, NodePtr s, NodePtr e) :
    Object("Curve"), owner(owner == NULL ? c->owner : owner), parent(NULL), l(-1), start(s), end(e), bounds(NULL)
{
    this->id.id = NULL_ID;
    this->area1.id = NULL_ID;
    this->area2.id = NULL_ID;
    this->vertices = new vector<Vertex> ();
    if (c != NULL) {
        type = c->getType();
        width = c->getWidth();
        s0 = c->getS0();
        s1 = c->getS1();
        for (int i = 0; i < c->getSize() - 2; i++) {
            vertices->push_back((*c->vertices)[i]);
        }
    } else {
        type = 0;
        width = 0.0f;
        s0 = 0;
        s1 = 1;
    }
}

Curve::~Curve()
{
    delete vertices;
    if (bounds != NULL) {
        delete bounds;
        bounds = NULL;
    }
    if (id.id == NULL_ID) {
        clear();
    }
    parent = NULL;
}

void Curve::clear()
{
    if (getStart() != NULL) {
        start->removeCurve(getId());
    }
    if (getEnd() != NULL && start != end) {
        end->removeCurve(getId());
    }
    start = NULL;
    end = NULL;
}

void Curve::print() const {
    printf("%d %f %d\n", getSize(), getWidth(), getType());
    vec2d v;
    for (int i = 0; i < getSize(); i++) {
        v = getXY(i);
        printf("%f %f %d %f %f\n", v.x, v.y, getIsControl(i), getS(i), getL(i));
    }
}

CurveId Curve::getId() const
{
    CurveId i;
    i.ref = (Curve *)this;
    return i;
}

CurvePtr Curve::getParent() const
{
    return parent;
}

CurvePtr Curve::getAncestor() const
{
    if (parent == NULL) {
        return (Curve*) this;
    } else {
        return parent->getAncestor();
    }
}

CurveId Curve::getParentId() const
{
    if (parent != NULL) {
        return parent->getId();
    }
    CurveId cid;
    cid.id = NULL_ID;
    return cid;

}

Vertex Curve::getVertex(int i) const
{
    if (i == 0) {
        return Vertex(getStart()->getPos(), s0, 0, false);
    }
    if (i < getSize() - 1) {
        return (*vertices)[i - 1];
    }
    return Vertex(getEnd()->getPos(), s1, l, false);
}

Vertex Curve::getVertex(NodePtr s, int offset) const
{
    int i = s == getStart() ? offset : getSize() - 1 - offset;
    return getVertex(i);
}

int Curve::getVertex(vec2d p) const
{
    for (int i = 0; i < getSize(); i++) {
        if (getXY(i) == p) {
            return i;
        }
    }
    return -1;
}

bool Curve::getIsControl(int i) const
{
    if (i > 0 && i < getSize() - 1) {
        return (*vertices)[i - 1].isControl;
    }
    return false;
}

bool Curve::getIsSmooth(int offset, vec2d &a, vec2d &b) const
{
    if (offset == 0 || offset == getSize() - 1) {
        return false;
    }

    vec2d p = getXY(offset - 1);
    vec2d q = getXY(offset);
    vec2d r = getXY(offset + 1);
    float d = ((p + r) * 0.5 - q).squaredLength();
    if (d < 0.1f && getIsControl(offset - 1) && getIsControl(offset + 1)) {
        return true;
    }

    a = q - (r - p) * 0.10f;
    b = q + (r - p) * 0.10f;
    return false;
}

float Curve::getS(int i) const
{
    if (i == 0) {
        return s0;
    }
    if (i < getSize() - 1) {
        return (*vertices)[i - 1].s;
    }
    return s1;
}

float Curve::getL(int i) const
{
    if (i == 0) {
        return 0;
    }
    if (i < getSize() - 1) {
        return (*vertices)[i - 1].l;
    }
    return l;
}

box2d Curve::getBounds() const
{
    if (bounds == NULL) {

        double xmin = INFINITY;
        double xmax = -INFINITY;
        double ymin = INFINITY;
        double ymax = -INFINITY;
        int n = getSize();
        for (int i = 0; i < n; ++i) {
            vec2d p = getXY(i);
            xmin = min(xmin, p.x);
            xmax = max(xmax, p.x);
            ymin = min(ymin, p.y);
            ymax = max(ymax, p.y);
        }
        bounds = new box2d(xmin, xmax, ymin, ymax);
    }
    return *bounds;
}

vec2d Curve::getXY(int i) const
{
    if (i == 0) {
        return getStart()->getPos();
    } else if (i < getSize() - 1) {
        return vec2d((*vertices)[i - 1].x, (*vertices)[i - 1].y);
    } else {
        return getEnd()->getPos();
    }
}

vec2d Curve::getXY(NodePtr start, int offset) const
{
    int i = start == getStart() ? offset : getSize() - 1 - offset;
    return getXY(i);
}

bool Curve::getIsControl(NodePtr start, int offset) const
{
    int i = start == getStart() ? offset : getSize() - 1 - offset;
    return getIsControl(i);
}

float Curve::getS(NodePtr start, int offset) const
{
    int i = start == getStart() ? offset : getSize() - 1 - offset;
    return getS(i);
}

float Curve::getL(NodePtr start, int offset) const
{
    int i = start == getStart() ? offset : getSize() - 1 - offset;
    return getL(i);
}

NodePtr Curve::getStart() const
{
    return start;
}

NodePtr Curve::getEnd() const
{
    return end;
}

AreaPtr Curve::getArea1() const
{
    if (area1.id == NULL_ID) {
        return NULL;
    }
    return owner->getArea(area1);
}

AreaPtr Curve::getArea2() const
{
    if (area2.id == NULL_ID) {
        return NULL;
    }
    return owner->getArea(area2);
}

NodePtr Curve::getOpposite(const NodePtr n) const
{
    NodePtr s = getStart();
    NodePtr e = getEnd();
    assert(n == s || n == e);
    return n == s ? e : s;
}

CurvePtr Curve::getNext(const NodePtr n, const std::set<CurveId> &excludedCurves, bool reverse) const
{
    vector<CurvePtr> ncurves;
    for (int i = 0; i < n->getCurveCount(); ++i) {
        CurvePtr nc = n->getCurve(i);
        if (nc != this && excludedCurves.find(nc->getId()) == excludedCurves.end()) {
            ncurves.push_back(nc);
        }
    }
    vec2d o = n->getPos();
    vec2d prev = getXY(n, 1);
    for (int i = 0; i < (int) ncurves.size(); ++i) {
        for (int j = i + 1; j < (int) ncurves.size(); ++j) {
            CurvePtr pi = ncurves[i];
            CurvePtr pj = ncurves[j];
            vec2d npi = pi->getXY(n, 1);
            vec2d npj = pj->getXY(n, 1);
            float ai = angle(prev - o, npi - o);
            float aj = angle(prev - o, npj - o);
            if (aj < ai) {
                ncurves[i] = pj;
                ncurves[j] = pi;
            }
        }
    }
    CurvePtr res = ncurves.size() > 0 ? reverse ? ncurves[ncurves.size() - 1] : ncurves[0] : NULL;
    if (getStart() == getEnd() && excludedCurves.find(getId()) == excludedCurves.end()) {
        if (res != NULL) {//ncurves.size() > 0) {
            CurvePtr c = reverse ? ncurves[ncurves.size() - 1] : ncurves[0];
            vec2d v = c->getXY(getStart(), 1);
            vec2d prev = getXY(1);
            vec2d next = getXY(getSize() - 2);
            float ai = angle(prev - o, next - o);
            float aj = angle(prev - o, v - o);
            if ((reverse && ai > aj) || (!reverse && ai < aj)) {
                res = (Curve *) this;
            }
        } else {
            res = (Curve*) this;
        }
    }
    return res;
}

float Curve::getCurvilinearLength(float s, vec2d *p, vec2d *n) const
{
    //assert(s >= s0 && s <= s1);
    if (s <= s0) {
        if (p != NULL) {
            *p = getStart()->getPos();
            if (n != NULL) {
                vec2d a = getXY(0);
                vec2d b = getXY(1);
                *n = vec2d(a.y - b.y, b.x - a.x);
            }
        }
        return 0;
    }
    if (s >= s1) {
        if (p != NULL) {
            *p = getEnd()->getPos();
            if (n != NULL) {
                vec2d a = getXY(getSize() - 2);
                vec2d b = getXY(getSize() - 1);
                *n = vec2d(a.y - b.y, b.x - a.x);
            }
        }
        return l;
    }
    int i0 = 0;
    int i1 = getSize() - 1;
    float s0 = this->s0;
    float s1 = this->s1;
    float l0 = 0;
    float l1 = this->l;
    while (i1 > i0 + 1) {
        int im = (i0 + i1) / 2;
        float sm = getS(im);
        float lm = getL(im);
        if (s < sm) {
            i1 = im;
            s1 = sm;
            l1 = lm;
        } else {
            i0 = im;
            s0 = sm;
            l0 = lm;
        }
    }
    float c = s1 == s0 ? s0 : (s - s0) / (s1 - s0);
    if (p != NULL) {
        vec2d a = getXY(i0);
        vec2d b = getXY(i1);
        *p = a + (b - a) * c;
        if (n != NULL) {
            *n = vec2d(a.y - b.y, b.x - a.x);
        }
    }
    return l0 + c * (l1 - l0);
}

float Curve::getCurvilinearCoordinate(float l, vec2d *p, vec2d *n) const
{
    if (l <= 0) {
        if (p != NULL) {
            *p = getStart()->getPos();
            if (n != NULL) {
                vec2d a = getXY(0);
                vec2d b = getXY(1);
                *n = vec2d(a.y - b.y, b.x - a.x);
            }
        }
        return this->s0;
    }
    if (l >= this->l) {
        if (p != NULL) {
            *p = getEnd()->getPos();
            if (n != NULL) {
                vec2d a = getXY(getSize() - 2);
                vec2d b = getXY(getSize() - 1);
                *n = vec2d(a.y - b.y, b.x - a.x);
            }
        }
        return this->s1;
    }
    int i0 = 0;
    int i1 = getSize() - 1;
    float s0 = this->s0;
    float s1 = this->s1;
    float l0 = 0;
    float l1 = this->l;
    while (i1 > i0 + 1) {
        int im = (i0 + i1) / 2;
        float sm = getS(im);
        float lm = getL(im);
        if (l < lm) {
            i1 = im;
            s1 = sm;
            l1 = lm;
        } else {
            i0 = im;
            s0 = sm;
            l0 = lm;
        }
    }
    float c = l1 == l0 ? l0 : (l - l0) / (l1 - l0);
    if (p != NULL) {
        vec2d a = getXY(i0);
        vec2d b = getXY(i1);
        *p = a + (b - a) * c;
        if (n != NULL) {
            *n = vec2d(a.y - b.y, b.x - a.x);
        }
    }
    return s0 + c * (s1 - s0);
}

Curve::position Curve::getRectanglePosition(float width, float cap, const box2d &r, double *coords) const
{
    int n = getSize();
    float w = width / 2;
    box2d b = getBounds().enlarge(w);
    if (r.xmin > b.xmax || r.xmax < b.xmin || r.ymin > b.ymax || r.ymax < b.ymin) {
        return OUTSIDE;
    }
    if (width > max(r.xmax - r.xmin, r.ymax - r.ymin)) {
        for (int i = 0; i < n - 1; ++i) {
            vec2d a = getXY(i);
            vec2d b = getXY(i + 1);
            seg2d ab = seg2d(a, b);
            if (cap != 0) {
                if (i == 0) {
                    ab = seg2d(a - ab.ab / ab.ab.length() * cap, b);
                }
                if (i == n - 2) {
                    ab = seg2d(ab.a, b + ab.ab / ab.ab.length() * cap);
                }
            }
            if (ab.contains(vec2d(r.xmin, r.ymin), w) && ab.contains(vec2d(r.xmax, r.ymin), w) && ab.contains(vec2d(
                    r.xmin, r.ymax), w) && ab.contains(vec2d(r.xmax, r.ymax), w)) {
                if (coords != NULL) {
                    coords[0] = a.x;
                    coords[1] = a.y;
                    coords[2] = b.x - a.x;
                    coords[3] = b.y - a.y;
                    coords[4] = getS(i);
                    coords[5] = getS(i + 1);
                }
                return INSIDE;
            }
        }
    }
    box2d t = r.enlarge(w);
    for (int i = 0; i < n - 1; ++i) {
        seg2d ab = seg2d(getXY(i), getXY(i + 1));
        if (cap != 0) {
            if (i == 0) {
                ab = seg2d(ab.a - ab.ab / ab.ab.length() * cap, ab.a + ab.ab);
            }
            if (i == n - 2) {
                ab = seg2d(ab.a, ab.a + ab.ab + ab.ab / ab.ab.length() * cap);
            }
        }
        if (ab.intersects(t)) {
            return INTERSECT;
        }
    }
    return OUTSIDE;
}

Curve::position Curve::getTrianglePosition(float width, float cap, const vec2d* t, const box2d &r, double *coords) const
{
    int n = getSize();
    float w = width / 2;
    box2d b = getBounds().enlarge(w);
    if (r.xmin > b.xmax || r.xmax < b.xmin || r.ymin > b.ymax || r.ymax < b.ymin) {
        return OUTSIDE;
    }
    if (width > max(r.xmax - r.xmin, r.ymax - r.ymin)) {
        for (int i = 0; i < n - 1; ++i) {
            vec2d a = getXY(i);
            vec2d b = getXY(i + 1);
            seg2d ab = seg2d(a, b);
            if (cap != 0) {
                if (i == 0) {
                    ab = seg2d(a - ab.ab / ab.ab.length() * cap, b);
                }
                if (i == n - 2) {
                    ab = seg2d(ab.a, b + ab.ab / ab.ab.length() * cap);
                }
            }
            if (ab.contains(t[0], w) && ab.contains(t[1], w) && ab.contains(t[2], w)) {
                if (coords != NULL) {
                    coords[0] = a.x;
                    coords[1] = a.y;
                    coords[2] = b.x - a.x;
                    coords[3] = b.y - a.y;
                    coords[4] = getS(i);
                    coords[5] = getS(i + 1);
                }
                return INSIDE;
            }
        }
    }
    vec2d t1, t2, t3;
    t1 = (t[0] - t[1]) / (t[0] - t[1]).length();
    t2 = (t[1] - t[2]) / (t[1] - t[2]).length();
    t3 = (t[2] - t[0]) / (t[2] - t[0]).length();
    vec2d tt[3] = { t[0] + (t1 - t3) * w, t[1] + (t2 - t1) * w, t[2] + (t3 - t2) * w };
    for (int i = 0; i < n - 1; ++i) {
        seg2d ab = seg2d(getXY(i), getXY(i + 1));
        if (cap != 0) {
            if (i == 0) {
                ab = seg2d(ab.a - ab.ab / ab.ab.length() * cap, ab.a + ab.ab);
            }
            if (i == n - 2) {
                ab = seg2d(ab.a, ab.a + ab.ab + ab.ab / ab.ab.length() * cap);
            }
        }
        if ((ab.intersects(seg2d(tt[0], tt[1]))) || (ab.intersects(seg2d(tt[1], tt[2]))) || (ab.intersects(seg2d(tt[0],
                tt[2])))) {
            return INTERSECT;
        }
    }
    return OUTSIDE;
}

bool Curve::isInside(const vec2d& p) const
{
    int n = getSize();
    float w = width / 2;
    box2d b = getBounds().enlarge(w);
    if (p.x > b.xmax || p.x < b.xmin || p.y > b.ymax || p.y < b.ymin) {
        return false;
    }
    for (int i = 0; i < n - 1; ++i) {
        vec2d a = getXY(i);
        vec2d b = getXY(i + 1);
        seg2d ab = seg2d(a, b);
        if (ab.contains(p, w)) {
            return true;
        }
    }
    return false;
}

bool Curve::isDirect() const
{
    float ymin = 0.0f;
    int min = -1;
    for (int i = 0; i < getSize(); ++i) {
        vec2d p = getXY(i);
        if (min == -1 || p.y < ymin) {
            min = i;
            ymin = p.y;
        }
    }
    int pred = min > 0 ? min - 1 : getSize() - 1;
    int succ = min < getSize() - 1 ? min + 1 : 1;
    vec2d pp = getXY(pred);
    vec2d pc = getXY(min);
    vec2d ps = getXY(succ);
    return cross(ps - pc, pp - pc) > 0;
}

void Curve::setIsControl(int i, bool c)
{
    if (i <= 0 || i >= getSize() - 1) {
        return;
    }
    if ((getIsControl(i - 1) && (getIsControl(i - 2) || getIsControl(i + 1))) || (getIsControl(i + 1) && (getIsControl(i + 2)))) {
        return;
    }
    (*vertices)[i - 1].isControl = c;
}

void Curve::setS(int i, float s)
{
    if (i == 0) {
        s0 = s;
    } else if (i < getSize() - 1) {
        (*vertices)[i - 1].s = s;
    } else {
        s1 = s;
    }
}

void Curve::setType(int type)
{
    this->type = type;
}

void Curve::setWidth(float width)
{
    this->width = width;
}

void Curve::setOwner(Graph* owner)
{
    this->owner = owner;
}

#define LIMIT 10

class FlatteningCurveIterator
{
public:
    double squareflat;
    double curx;
    double cury;
    double curs;
    vector<Vertex> *result;

    FlatteningCurveIterator(float flatness, vector<Vertex> *result) :
        squareflat(flatness * flatness), result(result)
    {
    }

    virtual ~FlatteningCurveIterator()
    {
    }

    void moveTo(double x1, double y1, double s1)
    {
        curx = x1;
        cury = y1;
        curs = s1;
    }

    void lineTo(double x2, double y2, double s2, bool isControl, bool isEnd)
    {
        if (!isEnd) {
            Vertex v = Vertex(vec2d(x2, y2), s2, -1, isControl);
            result->push_back(v);
            curx = x2;
            cury = y2;
            curs = s2;
        }
    }

    double quadCurveFlatnessSq(double x1, double y1, double ctrlx, double ctrly, double x2, double y2)
    {
        seg2d ab = seg2d(vec2d(x1, y1), vec2d(x2, y2));
        return ab.segmentDistSq(vec2d(ctrlx, ctrly));
    }

    void quadTo(double ctrlx, double ctrly, double ctrls, double x2, double y2, double s2, int level, bool isEnd)
    {
        double x1 = curx;
        double y1 = cury;
        double s1 = curs;
        if (level >= LIMIT || quadCurveFlatnessSq(x1, y1, ctrlx, ctrly, x2, y2) < squareflat) {
            lineTo(ctrlx, ctrly, ctrls, true, false);
            lineTo(x2, y2, s2, false, isEnd);
        } else {
            double nx1 = (x1 + ctrlx) / 2.0f;
            double ny1 = (y1 + ctrly) / 2.0f;
            double ns1 = (s1 + ctrls) / 2.0f;
            double nx2 = (x2 + ctrlx) / 2.0f;
            double ny2 = (y2 + ctrly) / 2.0f;
            double ns2 = (s2 + ctrls) / 2.0f;
            ctrlx = (nx1 + nx2) / 2.0f;
            ctrly = (ny1 + ny2) / 2.0f;
            ctrls = (ns1 + ns2) / 2.0f;
            quadTo(nx1, ny1, ns1, ctrlx, ctrly, ctrls, level + 1, false);
            quadTo(nx2, ny2, ns2, x2, y2, s2, level + 1, isEnd);
        }
    }

    double cubicCurveFlatnessSq(double x1, double y1, double ctrlx1, double ctrly1, double ctrlx2, double ctrly2, double x2,
            double y2)
    {
        seg2d ab = seg2d(vec2d(x1, y1), vec2d(x2, y2));
        double d1 = ab.segmentDistSq(vec2d(ctrlx1, ctrly1));
        double d2 = ab.segmentDistSq(vec2d(ctrlx2, ctrly2));
        return max(d1, d2);
    }

    void curveTo(double ctrlx1, double ctrly1, double ctrls1, double ctrlx2, double ctrly2, double ctrls2, double x2,
            double y2, double s2, int level, bool isEnd)
    {
        double x1 = curx;
        double y1 = cury;
        double s1 = curs;
        if (level >= LIMIT || cubicCurveFlatnessSq(x1, y1, ctrlx1, ctrly1, ctrlx2, ctrly2, x2, y2) < squareflat) {
            lineTo(ctrlx1, ctrly1, ctrls1, true, false);
            lineTo(ctrlx2, ctrly2, ctrls2, true, false);
            lineTo(x2, y2, s2, false, isEnd);
        } else {
            double nx1 = (x1 + ctrlx1) / 2.0f;
            double ny1 = (y1 + ctrly1) / 2.0f;
            double ns1 = (s1 + ctrls1) / 2.0f;
            double nx2 = (x2 + ctrlx2) / 2.0f;
            double ny2 = (y2 + ctrly2) / 2.0f;
            double ns2 = (s2 + ctrls2) / 2.0f;
            double centerx = (ctrlx1 + ctrlx2) / 2.0f;
            double centery = (ctrly1 + ctrly2) / 2.0f;
            double centers = (ctrls1 + ctrls2) / 2.0f;
            ctrlx1 = (nx1 + centerx) / 2.0f;
            ctrly1 = (ny1 + centery) / 2.0f;
            ctrls1 = (ns1 + centers) / 2.0f;
            ctrlx2 = (nx2 + centerx) / 2.0f;
            ctrly2 = (ny2 + centery) / 2.0f;
            ctrls2 = (ns2 + centers) / 2.0f;
            centerx = (ctrlx1 + ctrlx2) / 2.0f;
            centery = (ctrly1 + ctrly2) / 2.0f;
            centers = (ctrls1 + ctrls2) / 2.0f;
            curveTo(nx1, ny1, ns1, ctrlx1, ctrly1, ctrls1, centerx, centery, centers, level + 1, false);
            curveTo(ctrlx2, ctrly2, ctrls2, nx2, ny2, ns2, x2, y2, s2, level + 1, isEnd);
        }
    }
};

void Curve::flatten(float squareFlatness)
{
    vector<Vertex> *newVertices = new vector<Vertex> ();

    Vertex p = getVertex(0);
    FlatteningCurveIterator iterator(squareFlatness, newVertices);
    iterator.moveTo(p.x, p.y, p.s);
    int n = getSize();

    for (int i = 1; i < n; ++i) {
        p = getVertex(i);
        if (p.isControl) {
            Vertex q = getVertex(++i);
            if (q.isControl) {
                Vertex r = getVertex(++i);
                bool isEnd = i == n - 1;
                iterator.curveTo(p.x, p.y, p.s, q.x, q.y, q.s, r.x, r.y, r.s, 0, isEnd);
            } else {
                bool isEnd = i == n - 1;
                iterator.quadTo(p.x, p.y, p.s, q.x, q.y, q.s, 0, isEnd);
            }
        } else {
            bool isEnd = i == n - 1;
            iterator.lineTo(p.x, p.y, p.s, false, isEnd);
        }
    }

    resetBounds();

    delete vertices;

    this->vertices = newVertices;
}

void Curve::computeCurvilinearCoordinates()
{
    s0 = 0;
    s1 = getSize() - 1.0f;

    for (int i = 0; i < getSize() - 2; i++) {
        (*vertices)[i].s = float(i) + 1.0f;
    }
}

float Curve::computeCurvilinearLength()
{
    float l = 0;
    vec2d a = getStart()->getPos();
    for (int i = 0; i < getSize() - 2; i++) {
        vec2d b = vec2d((*vertices)[i].x, (*vertices)[i].y);
        l += (b - a).length();
        (*vertices)[i].l = l;
        a = b;
    }
    l += (getEnd()->getPos() - a).length();
    this->l = l;
    return l;
}

void Curve::addVertex(NodeId id, bool isEnd)
{
    if (start == NULL || isEnd == 0) {
        start = owner->getNode(id);
    } else {
        end = owner->getNode(id);
    }
}
void Curve::addVertex(vec2d pt, int rank, bool isControl)
{
    Vertex cp = Vertex(pt.x, pt.y, (float)rank + 1, isControl);
    if (rank > (int)vertices->size()) {
        vertices->push_back(cp);
    } else {
        vertices->insert(vertices->begin() + rank, cp);
    }
}

void Curve::addVertex(double x, double y, float s, bool isControl)
{
    Vertex cp = Vertex(x, y, s, isControl);
    vertices->push_back(cp);
}

void Curve::addVertex(const vec2d &pt, float s, float l, bool isControl)
{
    Vertex cp = Vertex(pt, s, l, isControl);
    vertices->push_back(cp);
}

void Curve::addVertex(const Vertex &pt)
{
    Vertex cp = Vertex(pt, pt.s, pt.l, pt.isControl);
    vertices->push_back(pt);
}

void Curve::addVertices(const vector<vec2d> &v)
{
    start = owner->newNode(v[0]);
    int ind = ((int) v.size()) - 1;
    if (v[0] == v[ind]) {
        end = start;
    } else {
        end = owner->newNode(v[ind]);
    }
    start->addCurve(getId());
    end->addCurve(getId());
    for (int i = 1; i < (int)v.size() - 1; i++) {
        this->vertices->push_back(Vertex(v[i].x, v[i].y, float(i), false));
    }
}

void Curve::removeVertex(int i)
{
    if (i > 0 && i < getSize() - 1) {
        vertices->erase(vertices->begin() + i - 1);
    }
}

void Curve::check()
{
    for (int i = 0; i < getSize() - 1; ++i)
    {
        vec2d a = getXY(i); // verify that adjacent point are unique
        vec2d b = getXY(i + 1);
        assert(a != b);
    }
}

void Curve::removeDuplicateVertices()
{
    vector<int> pointsTodelete;

    for (int i = 0; i < getSize() - 1; ++i)
    {
        vec2d a = getXY(i); // verify that adjacent point are unique
        vec2d b = getXY(i + 1);
        if(a == b) {
            if (i < getSize() - 2) {
                pointsTodelete.push_back(i + 1);
                if (i == getSize() - 2) break; // can't delete the end
            } else
            {
                pointsTodelete.push_back(i); // delete last vertex
            }
        }
    }

    for (size_t i = 0; i < pointsTodelete.size(); ++i)
    {

    }
}

/**
 * Merge vertices in Curves which are too near from one another.
 */
void Curve::decimate(float minDistanceThreshold)
{

    // sucessively merge nearest vertices until finished
    while(true)
    {
        float minLength = INFINITY;
        int minIndex = -1;
        vec2d newPos;

        for (int i = 0; i < getSize() - 1; ++i)
        {
            vec2d a = getXY(i);
            vec2d b = getXY(i + 1);
            float dist = (a - b).length();

            if (dist < minLength)
            {
                minIndex = i;
                minLength = dist;
                newPos = (a + b) * 0.5f;
            }
        }

        if (minLength < minDistanceThreshold)
        {
            int toDeleteIndex = minIndex == 0 ? 1 : minIndex;
            removeVertex(toDeleteIndex);
        }
        else
        {
            return; // no points to delete
        }

        if (getSize() <= 2) return; // only start and end remains
    }
    check();
}

void Curve::setXY(int i, const vec2d &p)
{
    if (i == 0) {
        getStart()->setPos(p);
    } else if (i < (getSize() - 1)) {
        (*vertices)[i - 1].x = p.x;
        (*vertices)[i - 1].y = p.y;
    } else {
        getEnd()->setPos(p);
    }
    resetBounds();
}

void Curve::resetBounds() const
{
    if (bounds != NULL) {
        delete bounds;
        bounds = NULL;
    }
    if (area1.id != NULL_ID) {
        getArea1()->resetBounds();
    }
    if (area2.id != NULL_ID) {
        getArea2()->resetBounds();
    }
}

void Curve::addArea(AreaId a)
{
    if (area1.id == NULL_ID || area1 == a) {
        area1 = a;
    } else {
        assert(area2.id == NULL_ID || area2 == a);
        area2 = a;
    }
}

void Curve::removeArea(AreaId a)
{
    if (area1 == a) {
        area1 = area2;
        area2.id = NULL_ID;
    } else {
        assert(area2 == a);
        area2.id = NULL_ID;
    }
}

void Curve::invert()
{
    NodePtr tmp = start;
    start = end;
    end = tmp;
    vector<Vertex> *v = new vector<Vertex>();
    v->insert(v->begin(), vertices->rbegin(), vertices->rend());
    delete vertices;
    vertices = v;
    if (getArea1() != NULL) {
        getArea1()->invertCurve(getId());
        if (getArea2() != NULL) {
            getArea2()->invertCurve(getId());
        }
    }
}

bool Curve::equals(Curve *c, set<NodeId> &visited) const
{
    if (c == NULL) {
        return false;
    }

    if (width != c->getWidth() || type != c->getType() || getSize() != c->getSize()) {
        return false;
    }

    for (int i = 0; i < getSize(); i++) {
        if (getXY(i) != c->getXY(i) && getXY(i) != c->getXY(c->getEnd(), i)) {
            return false;
        }
    }
    visited.insert(getStart()->getId());
    visited.insert(getEnd()->getId());

    if (getArea1() != NULL) {
        if (c->getArea1() == NULL) {
            return false;
        }

        if (getArea2() != NULL) {
            if (c->getArea2() == NULL) {
                return false;
            }
        } else {
            if (c->getArea2() != NULL) {
                return false;
            }
        }
    } else {
        if (c->getArea1() != NULL) {
            return false;
        }
    }
    return true;
}

}
