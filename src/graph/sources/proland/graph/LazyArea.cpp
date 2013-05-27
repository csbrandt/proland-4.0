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

#include "proland/graph/LazyArea.h"

namespace proland
{

LazyArea::LazyArea(Graph* owner, AreaId id) :
    Area(owner)
{
    parentId.id = NULL_ID;
    this->id = id;
}

LazyArea::~LazyArea()
{
    if (owner != NULL) {
        dynamic_cast<LazyGraph*>(owner)->deleteArea(id);
    }
}

AreaId LazyArea::getId() const
{
    return id;
}

AreaPtr LazyArea::getParent() const
{
    return NULL;
    //if (parent == NULL) {
    //    parent = owner->getArea(parentId);
    //}
    //return parent;
}

CurvePtr LazyArea::getCurve(int i) const
{
    if (curves[i].first == NULL) {
        curves[i] = pair<CurvePtr, int>(owner->getCurve(curveIds[i].first), curveIds[i].second);
    }
    return curves[i].first;
}

CurvePtr LazyArea::getCurve(int i, int& orientation) const
{
    if (curves[i].first == NULL) {
        curves[i] = pair<CurvePtr, int>(owner->getCurve(curveIds[i].first), curveIds[i].second);
    }
    orientation = curveIds[i].second;
    return curves[i].first;
}

int LazyArea::getCurveCount() const
{
    return curveIds.size();
}

void LazyArea::setOrientation(int i, int orientation)
{
    dynamic_cast<LazyGraph*>(owner)->getAreaCache()->add(this, true);
    curveIds[i].second = orientation;
    //curves[i].second = orientation;
}

void LazyArea::invertCurve(CurveId cid)
{
    dynamic_cast<LazyGraph*>(owner)->getAreaCache()->add(this, true);
    vector< pair<CurveId, int> >::iterator it = curveIds.begin();
    while (it != curveIds.end()) {
        if (it->first == cid) {
            break;
        }
        it++;
    }
    assert(it != curveIds.end());
    it->second = it->second == 0;
    Area::invertCurve(cid);
}

void LazyArea::loadCurve(CurveId id, int orientation)
{
    curveIds.push_back(make_pair<CurveId, int>(id, orientation));
    curves.push_back(make_pair<CurvePtr, int>(NULL, 0));
}

void LazyArea::addCurve(CurveId id, int orientation)
{
    dynamic_cast<LazyGraph*>(owner)->getAreaCache()->add(this, true);
    curveIds.push_back(make_pair<CurveId, int>(id, orientation));
    curves.push_back(make_pair<CurvePtr, int>((CurvePtr) NULL, 0));
}

void LazyArea::switchCurves(int curve1, int curve2)
{
    dynamic_cast<LazyGraph*>(owner)->getAreaCache()->add(this, true);
    pair<CurveId, int> cq = curveIds[curve1];
    curveIds[curve1] = curveIds[curve2];
    curveIds[curve2] = cq;
    if (curves[curve1].first != NULL || curves[curve2].first != NULL) {
        pair<CurvePtr, int> cq = curves[curve1];
        curves[curve1] = curves[curve2];
        curves[curve2] = cq;
    }
}

void LazyArea::removeCurve(int index)
{
    dynamic_cast<LazyGraph*>(owner)->getAreaCache()->add(this, true);
    curves.erase(curves.begin() + index);
    curveIds.erase(curveIds.begin() + index);
}

void LazyArea::doRelease()
{
    if (owner != NULL) {
        dynamic_cast<LazyGraph*>(owner)->releaseArea(id);
        for (int i = 0; i < (int) curves.size(); i++) {
            curves[i] = pair<CurvePtr, int>((CurvePtr)NULL, 0);
        }
    } else {
        delete this;
    }
}

void LazyArea::setParentId(AreaId id)
{
    this->parentId = id;
}

}
