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

#ifndef _PROLAND_LAZYAREA_H_
#define _PROLAND_LAZYAREA_H_

#include "proland/graph/Area.h"
#include "proland/graph/LazyGraph.h"

namespace proland
{

/**
 * An Area is described by 1 or more curves. It may contain a subgraph.
 * This is used to describe pastures, lakes...
 * This is the 'Lazy' version of Areas. Allows real time loading for LazyGraphs.
 * Can be deleted and reloaded at any time depending on the needs.
 * @ingroup graph
 * @author Antoine Begault, Guillaume Piolat
 */
PROLAND_API class LazyArea : public Area
{
public:
    /**
     * Creates a new LazyArea.
     *
     * @param owner the graph containing  this area.
     * @param id this area's id (determined by LazyGraph).
     */
    LazyArea(Graph* owner, AreaId id);

    /**
     * Deletes this LazyArea.
     */
    virtual ~LazyArea();

    /**
     * Returns this area's Id.
     * For LazyArea, an AreaId is a unique number assigned by the owner Graph,
     * (in opposition to Areas, for which Ids are a direct reference to the Area).
     */
    virtual AreaId getId() const;

    /**
     * Returns the parent Area of this Area.
     * Should allways return NULL, because LazyGraph are only used on top of
     * the graph, and thus have no parent.
     */
    virtual AreaPtr getParent() const;

    /**
     * Returns a curve.
     *
     * @param i the rank of the curve to be returned.
     * @return the i'th curve describing this area, or NULL if i is out of range.
     */
    virtual CurvePtr getCurve(int i) const;

    /**
     * Returns a curve.
     *
     * @param i the rank of the curve to be returned.
     * @param orientation will contain the orientation of the i'th curve in this area.
     * @return the i'th curve describing this area, or NULL if i is out of range.
     */
    virtual CurvePtr getCurve(int i, int& orientation) const;

    /**
     * Returns the number of curves forming this area.
     */
    virtual int getCurveCount() const;

    /**
     * Sets orientation for a given curve.
     *
     * @param i rank of the curve to modify.
     * @param orientation the new orientation of the curve.
     */
    virtual void setOrientation(int i, int orientation);

    /**
     * Inverts a given curve in this area.
     *
     * @param cid the curveId of the curve to invert.
     */
    virtual void invertCurve(CurveId cid);

    /**
     * Adds a curve to this area.
     *
     * @param id the added curve's Id.
     * @param orientation the orientation to add the curve.
     */
    virtual void addCurve(CurveId id, int orientation);

protected:
    /**
     * Calls the LazyGraph#releaseArea() method.
     * See Object#doRelease().
     */
    virtual void doRelease();

private:
    /**
     * The parent Area's id. If parentId == id, there's no parent.
     */
    AreaId parentId;

    /**
     * The list of the curves describing this area
     */
    vector< pair<CurveId, int> > curveIds;

    /**
     * Sets the parent Id.
     * See #setParent().
     *
     * @param id the new parent's Id.
     */
    virtual void setParentId(AreaId id);

    /**
     * Switch 2 curves of this area.
     *
     * @param id1 the first curve's id.
     * @param id2 the second curve's id.
     */
    virtual void switchCurves(int curve1, int curve2);

    /**
     * Removes a curve from the curves list.
     *
     * @param the index of the curve to remove.
     */
    virtual void removeCurve(int index);

    /**
     * Same as #addCurve(), but doesn't notify the Owner Graph about a change.
     */
    void loadCurve(CurveId id, int orientation);

    friend class LazyGraph;
};

}

#endif
