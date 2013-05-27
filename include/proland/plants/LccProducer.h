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

#ifndef _PROLAND_LCC_PRODUCER_H_
#define _PROLAND_LCC_PRODUCER_H_

#include "proland/producer/TileProducer.h"
#include "proland/terrain/Deformation.h"
#include "proland/plants/Plants.h"

using namespace std;

using namespace ork;

namespace proland
{

/**
 * TODO.
 * @ingroup plants
 * @author Eric Bruneton
 */
PROLAND_API class LccProducer : public TileProducer
{
public:
    LccProducer(ptr<TileProducer> delegate, ptr<Plants> plants, ptr<Texture2D> lccTexture,
        ptr<Program> copy, ptr<Program> dots, int maxLevel, bool deform);

    virtual ~LccProducer();

    ptr<TileProducer> getDelegate();

    virtual void setRootQuadSize(float size);

    virtual int getBorder();

    virtual bool hasTile(int level, int tx, int ty);

    virtual TileCache::Tile* findTile(int level, int tx, int ty, bool includeCache = false, bool done = false);

    virtual TileCache::Tile* getTile(int level, int tx, int ty, unsigned int deadline);

    vec4f getGpuTileCoords(int level, int tx, int ty, TileCache::Tile **tile);

    virtual bool prefetchTile(int level, int tx, int ty);

    virtual void putTile(TileCache::Tile *t);

    virtual void invalidateTiles();

    virtual void invalidateTile(int level, int tx, int ty);

    virtual void update(ptr<SceneManager> scene);

    virtual void getReferencedProducers(vector< ptr<TileProducer> > &producers) const;

protected:
    ptr<FrameBuffer> frameBuffer;

    ptr<Texture2D> lccTexture;

    ptr<Program> copy;

    ptr<Program> dots;

    LccProducer();

    void init(ptr<TileProducer> delegate, ptr<Plants> plants, ptr<Texture2D> lccTexture,
        ptr<Program> copy, ptr<Program> dots, int maxLevel, bool deform);

    virtual void *getContext() const;

    virtual ptr<Task> startCreateTile(int level, int tx, int ty, unsigned int deadline, ptr<Task> task, ptr<TaskGraph> owner);

    virtual void beginCreateTile();

    virtual bool doCreateTile(int level, int tx, int ty, TileStorage::Slot *data);

    virtual void endCreateTile();

    virtual void stopCreateTile(int level, int tx, int ty);

private:
    ptr<TileProducer> delegate;

    ptr<Plants> plants;

    int maxLevel;

    bool deform;

    float lastTreeDensity;

    float lastFov;

    ptr<Uniform1f> densityU;

    ptr<UniformSampler> sourceSamplerU;

    ptr<Uniform4f> sourceOSLU;

    ptr<Uniform4f> tileOffsetU;

    ptr<UniformMatrix2f> tileDeformU;

    ptr<Uniform4f> tileClipU;

    ptr<UniformSampler> densitySamplerU;

    ptr<Uniform4f> densityOSLU;

    ptr<Deformation> deformation;

    static static_ptr<FrameBuffer> old;
};

}

#endif
