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

#ifndef _PROLAND_CURVEDATAFACTORY_H_
#define _PROLAND_CURVEDATAFACTORY_H_

#include "proland/producer/TileProducer.h"
#include "proland/graph/GraphListener.h"
#include "proland/graph/producer/CurveData.h"

namespace proland
{

/**
 * CurveData Factory. Handles creation, access and destruction of CurveDatas, and allows to instantiate different types of CurveData.
 * @ingroup producer
 * @author Antoine Begault
 */
PROLAND_API class CurveDataFactory : public GraphListener
{
public:
    /**
     * Creates a new CurveDataFactory.
     */
    CurveDataFactory();

    /**
     * Creates a new CurveDataFactory.
     *
     * @param producer the GraphProducer used to generate Curves and their flattened version.
     */
    CurveDataFactory(ptr<TileProducer> producer);

    /**
     * Deletes this CurveDataFactory.
     */
    virtual ~CurveDataFactory();

    /**
     * Returns a new CurveData, whose type depends on the class that implements CurveDataFactory.
     *
     * @param id the CurveId of the described Curve.
     * @param flattenCurve flattened version of the described Curve.
     */
    virtual CurveData *newCurveData(CurveId id, CurvePtr flattenCurve);

    /**
     * See GraphListener#graphChanged().
     */
    virtual void graphChanged();

    /**
     * Returns the CurveData corresponding to the given Curve.
     * Handles a count of references.
     *
     * @param c a Curve.
     */
    CurveData *getCurveData(CurvePtr c);

    /**
     * Releases the CurveData corresponding to the given CurveId. The reference count is decreased, and if it is equal to zero,  the CurveData is deleted.
     * It will then call GraphProducer#putFlattenCurve().
     *
     * @param id the CurveId of an existing CurveData.
     */
    void putCurveData(CurveId id);

    /**
     * Returns the CurveData corresponding to the given Curve. Doesn't change the reference count.
     *
     * @param c a Curve.
     */
    CurveData *findCurveData(CurvePtr c);

    /**
     * Releases every used CurveData corresponding to a given tile.
     *
     * @param level level of the Tile.
     * @param x x coordinate of the Tile.
     * @param y y coordinate of the Tile.
     */
    void releaseCurveData(int level, int tx, int ty);

    /**
     * Adds a list of Curves used by a tile of this factory. They will require a call to #releaseCurveData.
     *
     * @param level level of the Tile.
     * @param x x coordinate of the Tile.
     * @param y y coordinate of the Tile.
     * @param curveDatas list of Ids corresponding to the used CurveData.
     */
    void addUsedCurveDatas(int level, int tx, int ty, set<CurveId> curveDatas);

    /**
     * Deletes every CurveData.
     */
    void clear();

    bool hasCurveData(CurveId id) {
        return curveDatas.find(id) != curveDatas.end();
    }

protected:
    /**
     * Initializes CurveDataFactory fields.
     *
     * @param producer the GraphProducer used to generate Curves and their flattened version.
     */
    void init(ptr<TileProducer> producer);

    virtual void swap(CurveDataFactory* factory);

    /**
     * The GraphProducer used to generate Curves and their flattened version.
     */
    ptr<TileProducer> producer;

    /**
     * List of every CurveData and their associated CurveId.
     */
    map<CurveId, CurveData*> curveDatas;

    /**
     * Reference count for each CurveData.
     */
    map<CurveData*, int> usedDataCount;

    /**
     * Maps every tile to the CurveDatas it uses.
     */
    map<TileCache::Tile::Id, set<CurveId> > usedDatas;
};

}

#endif
