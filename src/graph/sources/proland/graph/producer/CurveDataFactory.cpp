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

#include "proland/graph/producer/CurveDataFactory.h"

#include "proland/graph/producer/GraphProducer.h"

namespace proland
{

CurveDataFactory::CurveDataFactory()
{
}

CurveDataFactory::CurveDataFactory(ptr<TileProducer> producer)
{
    init(producer);
}

void CurveDataFactory::init(ptr<TileProducer> producer)
{
    assert(producer != NULL);
    this->producer = producer;
    producer.cast<GraphProducer>()->getRoot()->addListener(this);
}

CurveDataFactory::~CurveDataFactory()
{
    assert(producer != NULL);
    producer.cast<GraphProducer>()->getRoot()->removeListener(this);
    producer = NULL;
    clear();
}

CurveData *CurveDataFactory::newCurveData(CurveId id, CurvePtr flattenCurve)
{
    return new CurveData(id, flattenCurve);
}

void CurveDataFactory::graphChanged()
{
    assert(producer != NULL);
    Graph::Changes changes = producer.cast<GraphProducer>()->getRoot()->changes;
    set<CurveId>::const_iterator end = changes.addedCurves.end();
    set<CurveId>::const_iterator i = changes.addedCurves.begin();
    while (i != end) {
        CurveId id = (*i);
        map<CurveId, CurveData*>::iterator j = curveDatas.find(id);
        if (j != curveDatas.end()) {
            CurveData *d = j->second;
            CurvePtr c = producer.cast<GraphProducer>()->getRoot()->getCurve(id);
            CurveData *data = newCurveData(id, producer.cast<GraphProducer>()->getFlattenCurve(c));
            usedDataCount[data] = usedDataCount[d];
            usedDataCount.erase(usedDataCount.find(d));
            curveDatas.erase(j);
            delete d;
            curveDatas.insert(pair<CurveId, CurveData*>(id, data));
        }
        i++;
    }

    end = changes.removedCurves.end();
    i = changes.removedCurves.begin();
    while (i != end) {
        CurveId id = *i++;
        if (changes.addedCurves.find(id) == changes.addedCurves.end()) {
            map<CurveId, CurveData*>::iterator j = curveDatas.find(id);
            if (j != curveDatas.end()) {
                CurveData *d = j->second;
                usedDataCount.erase(usedDataCount.find(d));
                curveDatas.erase(j);
                delete d;
            }
        }
    }
}

void CurveDataFactory::clear()
{
    for(map<CurveId, CurveData*>::iterator i = curveDatas.begin(); i != curveDatas.end(); i++) {
        delete i->second;
    }
    curveDatas.clear();
    usedDataCount.clear();
    usedDatas.clear();
}

CurveData *CurveDataFactory::getCurveData(CurvePtr c)
{
    assert(producer != NULL);
    CurvePtr pc = c->getAncestor();
    CurveId id = pc->getId();
    map<CurveId, CurveData*>::iterator i = curveDatas.find(id);
    if (i == curveDatas.end()) {
        CurveData *data = newCurveData(id, producer.cast<GraphProducer>()->getFlattenCurve(pc));
        curveDatas.insert(pair<CurveId, CurveData*>(id, data));
        usedDataCount.insert(pair<CurveData*, int>(data, 1));
        return data;
    } else {
        usedDataCount[i->second] = usedDataCount[i->second] + 1;
        return i->second;
    }
}

void CurveDataFactory::putCurveData(CurveId id)
{
    assert(producer != NULL);
    map<CurveId, CurveData*>::iterator i = curveDatas.find(id);
    if (i == curveDatas.end()) {
        //printf("couldn't put curveData for curve : %d\n", id.id);
        return;
    }

    usedDataCount[i->second] = usedDataCount[i->second] - 1;
    if (usedDataCount[i->second] == 0) {
        usedDataCount.erase(i->second);
        delete i->second;
        producer.cast<GraphProducer>()->putFlattenCurve(id);
        curveDatas.erase(id);
    }
}

CurveData *CurveDataFactory::findCurveData(CurvePtr c)
{
    CurvePtr pc = c->getAncestor();
    CurveId id = pc->getId();
    map<CurveId, CurveData*>::iterator i = curveDatas.find(id);
    if (i == curveDatas.end()) {
        printf("couldn't find curveData for curve : %d\n", id.id);
    }
    assert(i != curveDatas.end());
    return i->second;
}

void CurveDataFactory::addUsedCurveDatas(int level, int tx, int ty, set<CurveId> curveDatas)
{
    TileCache::Tile::Id id = TileCache::Tile::getId(level, tx, ty);
    usedDatas.insert(make_pair(id, curveDatas));
}

void CurveDataFactory::releaseCurveData(int level, int tx, int ty)
{
    TileCache::Tile::Id id = TileCache::Tile::getId(level, tx, ty);
    map<TileCache::Tile::Id, set<CurveId> >::iterator curveIds = usedDatas.find(id);
    if (curveIds != usedDatas.end()) {
        for(set<CurveId>::iterator i = curveIds->second.begin(); i != curveIds->second.end(); i++) {
            putCurveData(*i);
        }
        curveIds->second.clear();
        usedDatas.erase(curveIds);
    }
}

void CurveDataFactory::swap(CurveDataFactory* factory)
{
    std::swap(usedDatas, factory->usedDatas);
    std::swap(producer, factory->producer);
    std::swap(curveDatas, factory->curveDatas);
    std::swap(usedDataCount, factory->usedDataCount);
}

}
