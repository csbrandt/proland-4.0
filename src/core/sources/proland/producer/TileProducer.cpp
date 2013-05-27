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

#include <pthread.h>

#include "proland/producer/TileProducer.h"

#include "ork/core/Logger.h"
#include "ork/scenegraph/SceneManager.h"
#include "proland/producer/GPUTileStorage.h"

using namespace std;

namespace proland
{

/**
 * The Task that produces the tiles for a TileProducer.
 */
class CreateTile : public Task
{
public:
    /**
     * The task graph that contains this task to store its dependencies. May be
     * NULL for CreateTile tasks without dependencies.
     */
    TaskGraph* parent;

    /**
     * The TileProducer that created this task.
     */
    TileProducer *owner;

    /**
     * The level of the tile to create.
     */
    int level;

    /**
     * The quadtree x coordinate of the tile to create.
     */
    int tx;

    /**
     * The quadtree y coordinate of the tile to create.
     */
    int ty;

    /**
     * Where the created tile data must be stored.
     */
    TileStorage::Slot *data;

    /**
     * Cache last result from getContext.
     */
    mutable void *cachedContext;

    /**
     * True is the tiles needed to create this tile have been acquired with
     * TileProducer#getTile. False if they are not or have been released with
     * TileProducer#putTile.
     */
    bool initialized;

    /**
     * Creates a new CreateTile Task.
     */
    CreateTile(TileProducer *owner, int level, int tx, int ty, TileStorage::Slot *data, unsigned deadline) :
        Task(owner->taskType, owner->isGpuProducer(), deadline), parent(NULL), owner(owner), level(level), tx(tx), ty(ty), data(data), initialized(true)
    {
        // the task to produce 'data' is 'this'
        data->lock(true);
        data->producerTask = this;
        data->lock(false);
    }

    virtual ~CreateTile()
    {
        // releases the tiles used to create this tile, if necessary
        stop();
        if (owner != NULL) {
            owner->removeCreateTile(this);
            if (owner->cache != NULL) {
                // cache is NULL if the owner producer is being deleted
                // in this case it is not necessary to update the cache
                owner->cache->createTileTaskDeleted(owner->getId(), level, tx, ty);
            }
        }
        if (parent != NULL) {
            // this object can not be deleted if 'parent' is still referenced (because
           // then it has itself references to this object). Hence we now that 'parent'
           // is no longer referenced at this point, and can be deleted.
           delete parent;
        }
    }

    /**
     * Overrides Task#getContext.
     */
    virtual void *getContext() const
    {
        // combines the context returned by the producer with the producer type
        // to ensure that two tasks from two producers with the same context
        // but with different types return different contexts.

        if (owner != NULL) { // the owner exists
            cachedContext = (void*) ((size_t) typeid(*owner).name() + (size_t) owner->getContext());
            return cachedContext;
        } else {
            // the owner may have been destroyed, this is a workaround
            assert(cachedContext != NULL);
            return cachedContext; // return last successful result
        }
    }

    /**
     * Overrides Task#init.
     */
    virtual void init(set<Task*> &initialized)
    {
        if (!isDone() && owner != NULL) {
            // acquires the tiles used to create this tile, if necessary
            start();
        }
    }

    /**
     * Acquires the tiles needed to create this tile with TileProducer#getTile,
     * if this is not already done.
     */
    void start()
    {
        if (!initialized) {
            if (parent != NULL) {
                // as we will reconstruct the content of #parent in
                // startCreateTile, we clear it first; in fact we remove all
                // dependencies first, and we remove unused tasks after the
                // reconstruction (so as to avoid adding a task just after
                // it is has been removed)
                parent->clearDependencies();
            }

            // acquires the tiles needed to create this tile, and completes the
            // tasks graph with the corresponding tasks and task dependencies
            owner->startCreateTile(level, tx, ty, getDeadline(), this, parent);

            if (parent != NULL) {
                // removes no longer used tasks; these are all the tasks without
                // successors, except this task
                TaskGraph::TaskIterator i = parent->getLastTasks();
                while (i.hasNext()) {
                    ptr<Task> t = i.next();
                    if (t != this) {
                        parent->removeTask(t);
                    }
                }
            }
            initialized = true;
        }
    }

    /**
     * Overrides Task#begin.
     */
    virtual void begin()
    {
        assert(!isDone());
        owner->beginCreateTile();
    }

    /**
     * Overrides Task#run.
     */
    bool run()
    {
        bool changes = true;
        assert(!isDone());
        data->lock(true);
        if (data->producerTask == this) {
            // since the creation of this CreateTile task,
            // where data->producerTask was set to this, it is possible that
            // 'data' was reaffected to another tile (if this task is a
            // prefetch task, its tile is not yet used and can then be evicted
            // from the cache between the creation and the execution of the
            // task). In this case we do not execute the task, otherwise it
            // could override data already produced for the reaffected tile.
            changes = owner->doCreateTile(level, tx, ty, data);
            data->id = TileCache::Tile::getTId(owner->getId(), level, tx, ty);
        }
        data->lock(false);
        return changes;
    }

    /**
     * Overrides Task#end.
     */
    virtual void end()
    {
        owner->endCreateTile();
    }

    /**
     * Releases the tiles used to create this tile with TileProducer#putTile,
     * if this is not already done.
     */
    void stop()
    {
        if (initialized) {
            if (owner != NULL) {
                owner->stopCreateTile(level, tx, ty);
            }
            initialized = false;
        }
    }

    void setIsDone(bool done, unsigned int t, reason r)
    {
        Task::setIsDone(done, t, r);
        if (done) {
            // releases the tiles used to create this tile, if necessary
            stop();
        } else if (r == DATA_NEEDED) {
            // the task will need to be reexecuted soon (this is not the case
            // if the reason is DATA_CHANGED - when invalidating tiles, see
            // TileCache#invalidateTiles - for instance for unused tiles)

            // setIsDone(false, DATA_NEEDED) is called from TileCache#getTile
            // and TileCache#prefetchTile, after the tile has been put in the
            // used or unused tiles list. Hence we are sure to find the tile
            // for this task with a findTile
            TileCache::Tile *t = owner->findTile(level, tx, ty, true);
            assert(t != NULL);
            // we get the data storage from the tile. This data storage can be
            // different from the current one if this task is reused from the
            // TileCache#deletedTiles list (this happens if an unused tile T is
            // deleted, its storage reused for another tile, and if T is needed
            // again. If this task was not deleted in between, T is recreated
            // using a new storage but reusing this task which still refers to
            // the old storage).
            data = t->data;
            // we need to update the producer task for this new storage
            data->lock(true);
            data->producerTask = this;
            data->lock(false);
            // the task is about to be executed, so we must acquire the tiles
            // needed for this task.
            start();
        }
    }

    const type_info *getTypeInfo()
    {
        return &typeid(*owner);
    }
};


/**
 * A TaskGraph for use with CreateTile. This subclass makes sure that
 * this task graph is not deleted before its internal root sub task
 * of type CreateTile.
 */
class CreateTileTaskGraph : public TaskGraph
{
public:
    /**
     * The TileProducer that created this task.
     */
    TileProducer *owner;

    /**
     * The CreateTile task that is the 'root' of this task graph.
     */
    CreateTile *root;

    /**
     * Saved dependencies of #root to restore them in #restore,
     * when #root has been removed from this graph in #doRelease.
     */
    set< ptr<Task> > rootDependencies;

    CreateTileTaskGraph(TileProducer *owner) : TaskGraph(), owner(owner)
    {
    }

    virtual ~CreateTileTaskGraph()
    {
        if (owner != NULL) {
            owner->removeCreateTile(this);
        }
    }

    virtual void doRelease()
    {
        // we do not delete this task graph right now, because we need
        // it as long as its root primitive task may be used (it has
        // a weeak pointer to this graph via its 'parent' field; also
        // if the graph was destroyed but not its root primitive task,
        // rebuilding a new graph in TileProducer#createTile would not
        // be possible - it woudld require to call startCreateTile, which
        // would redo calls to TileCache#getTile, thus locking these
        // tiles forever in the cache).
        ptr<Task> root = this->root;
        // removes all strong pointers to root so that it can be deleted
        // when it is no longer referenced (except by this graph)
        cleanup();
        // removes all dependencies on other tasks but save them
        // before in rootDependencies
        removeAndGetDependencies(root, rootDependencies);
        // finally removes 'root' itself
        removeTask(root);
    }

    void restore()
    {
        // if this method is called then this object has not been deleted
        // which means that #root has not been deleted (because the
        // destructor of #root destructs this object). Hence we now that
        // the weak pointer #root still points to a valid object.
        addTask(root);
        set< ptr<Task> >::iterator i = rootDependencies.begin();
        while (i != rootDependencies.end()) {
            ptr<Task> dst = *i;
            addDependency(root, dst);
            ++i;
        }
    }
};

TileProducer::TileProducer(const char* type, const char *taskType, ptr<TileCache> cache, bool gpuProducer) :
    Object(type), taskType((char *)taskType)
{
    init(cache, gpuProducer);
}

TileProducer::TileProducer(const char* type, const char *taskType) :
    Object(type), taskType((char *)taskType)
{
}

void TileProducer::init(ptr<TileCache> cache, bool gpuProducer)
{
    assert(cache != NULL);
    this->cache = cache;
    this->gpuProducer = gpuProducer;
    this->rootQuadSize = 0.0;
    this->id = cache->nextProducerId++;
    cache->producers.insert(make_pair(id, this));
    tileMap = NULL;
    mutex = new pthread_mutex_t;
    pthread_mutex_init((pthread_mutex_t*) mutex, NULL);
}

TileProducer::~TileProducer()
{
    assert(cache != NULL);
    map<int, TileProducer*>::iterator i = cache->producers.find(id);
    assert(i != cache->producers.end());
    cache->producers.erase(i);
    for (int i = 0; i < (int) tasks.size(); i++) {
        CreateTile *t = dynamic_cast<CreateTile*>(tasks[i]);
        if (t != NULL) {
            t->owner = NULL;
        } else {
            dynamic_cast<CreateTileTaskGraph*>(tasks[i])->owner = NULL;
        }
    }
    layers.clear();
    if (tileMap != NULL) {
        delete[] tileMap;
    }
    pthread_mutex_destroy((pthread_mutex_t*) mutex);
    delete (pthread_mutex_t*) mutex;
}

float TileProducer::getRootQuadSize()
{
    return rootQuadSize;
}

void TileProducer::setRootQuadSize(float size)
{
    rootQuadSize = size;
    for (int i = 0; i < (int) layers.size(); i++) {
        layers[i]->setCache(cache, id);
        layers[i]->setTileSize(cache->getStorage()->getTileSize(), getBorder(), getRootQuadSize());
    }
}

int TileProducer::getId()
{
    return id;
}

ptr<TileCache> TileProducer::getCache()
{
    return cache;
}

bool TileProducer::isGpuProducer()
{
    return gpuProducer;
}

int TileProducer::getBorder()
{
    return 0;
}

bool TileProducer::hasTile(int level, int tx, int ty)
{
    return true;
}

bool TileProducer::hasChildren(int level, int tx, int ty)
{
    return hasTile(level + 1, 2 * tx, 2 * ty);
}

TileCache::Tile* TileProducer::findTile(int level, int tx, int ty, bool includeCache, bool done)
{
    TileCache::Tile *t = cache->findTile(id, level, tx, ty, includeCache);
    if (done && t != NULL && !t->task->isDone()) {
        t = NULL;
    }
    return t;
}

TileCache::Tile* TileProducer::getTile(int level, int tx, int ty, unsigned int deadline)
{
    int users = 0;
    TileCache::Tile *t = cache->getTile(id, level, tx, ty, deadline, &users);
    if (users == 0) {
        for (unsigned int i = 0; i < layers.size(); i++) {
            layers[i]->useTile(level, tx, ty, deadline);
        }
    }
    return t;
}

vec4f TileProducer::getGpuTileCoords(int level, int tx, int ty, TileCache::Tile **tile)
{
    assert(isGpuProducer());
    ptr<TileStorage> storage = getCache()->getStorage();
    int s = storage->getTileSize();
    int b = getBorder();
    float dx = 0;
    float dy = 0;
    float dd = 1;
    float ds0 = (s / 2) * 2.0f - 2.0f * b;
    float ds = ds0;
    while (!hasTile(level, tx, ty)) {
        dx += (tx % 2) * dd;
        dy += (ty % 2) * dd;
        dd *= 2;
        ds /= 2;
        level -= 1;
        tx /= 2;
        ty /= 2;
        assert(level >= 0);
    }
    TileCache::Tile *t = *tile == NULL ? findTile(level, tx, ty, true, true) : NULL;
    while (*tile == NULL ? t == NULL : level != (*tile)->level) {
        dx += (tx % 2) * dd;
        dy += (ty % 2) * dd;
        dd *= 2;
        ds /= 2;
        level -= 1;
        tx /= 2;
        ty /= 2;
        assert(level >= 0);
        t = *tile == NULL ? findTile(level, tx, ty, true, true) : NULL;
    }
    dx = dx * ((s / 2) * 2 - 2 * b) / dd;
    dy = dy * ((s / 2) * 2 - 2 * b) / dd;
    if (*tile == NULL) {
        *tile = t;
    } else {
        t = *tile;
    }

    GPUTileStorage::GPUSlot *gput = dynamic_cast<GPUTileStorage::GPUSlot*>(t->getData());
    assert(gput != NULL);

    float w = gput->getWidth();
    float h = gput->getHeight();
    assert(w == h);

    if (s%2 == 0) {
        return vec4f((dx + b) / w, (dy + b) / h, float(gput->l), ds / w);
    } else {
        return vec4f((dx + b + 0.5f) / w, (dy + b + 0.5f) / h, float(gput->l), ds / w);
    }
}

bool TileProducer::prefetchTile(int level, int tx, int ty)
{
    if (cache->getScheduler() != NULL && cache->getScheduler()->supportsPrefetch(isGpuProducer())) {
        ptr<Task> task = cache->prefetchTile(id, level, tx, ty);
        if (task != NULL) {
            cache->getScheduler()->schedule(task);
            return true;
        }
    }
    for (unsigned int i = 0; i < layers.size(); i++) {
        layers[i]->prefetchTile(level, tx, ty);
    }
    return false;
}

void TileProducer::putTile(TileCache::Tile *t)
{
    if (cache->putTile(t) == 0) {
        for (unsigned int i = 0; i < layers.size(); i++) {
            layers[i]->unuseTile(t->level, t->tx, t->ty);
        }
    }
}

void TileProducer::invalidateTile(int level, int tx, int ty)
{
    getCache()->invalidateTile(getId(), level, tx, ty);
}

void TileProducer::invalidateTiles()
{
    cache->invalidateTiles(id);
}

void TileProducer::update(ptr<SceneManager> scene)
{
}

bool TileProducer::updateTileMap(float splitDistance, vec2f camera, int maxLevel)
{
    assert(isGpuProducer());
    ptr<GPUTileStorage> gpuStorage = getCache()->getStorage().cast<GPUTileStorage> ();
    ptr<Texture2D> tileMapT = gpuStorage->getTileMap();
    if (tileMapT == NULL) {
        return false;
    }
    if (tileMap == NULL) {
        tileMap = new unsigned char[2 * tileMapT->getWidth()];
    }
    assert(rootQuadSize != 0.0);
    camera.x += rootQuadSize / 2;
    camera.y += rootQuadSize / 2;
    int k = (int) ceil(splitDistance);

    if (k > 2) {
        bool collisions = false;
        assert(tileMapT->getWidth() >= 4093);
        for (int i = 0; i < tileMapT->getWidth(); ++i) {
            tileMap[2 * i] = 0;
            tileMap[2 * i + 1] = 0;
        }
        for (int l = 0; l <= maxLevel; ++l) {
            float tileSize = rootQuadSize / (1 << l);
            int tx0 = (int) floor(camera.x / (2 * tileSize));
            int ty0 = (int) floor(camera.y / (2 * tileSize));
            for (int ty = 2 * (ty0 - k); ty <= 2 * (ty0 + k) + 1; ++ty) {
                for (int tx = 2 * (tx0 - k); tx <= 2 * (tx0 + k) + 1; ++tx) {
                    GPUTileStorage::GPUSlot *gpuData = NULL;
                    TileCache::Tile* t;
                    if (tx >= 0 && tx < (1 << l) && ty >= 0 && ty < (1 << l)) {
                        t = findTile(l, tx, ty);
                    } else {
                        t = NULL;
                    }
                    if (t != NULL) {
                        TileStorage::Slot* data = t->getData(false);
                        if (data != NULL) {
                            gpuData = dynamic_cast<GPUTileStorage::GPUSlot*> (data);
                        }
                        // hash code key for (level,tx,ty) tile
                        // TODO hash code key collisions not handled!
                        int key = (tx + ty * (1 << l) + ((1 << (2 * l)) - 1) / 3) % 4093;

                        int x, y;
                        if (gpuData == NULL) {
                            x = 0;
                            y = 0;
                        } else {
                            x = gpuData->l % 256;
                            y = gpuData->l / 256 + 1;
                        }
                        if (y != 0 && tileMap[2 * key + 1] != 0) {
                            collisions = true;
                        }
                        tileMap[2 * key] = x;
                        tileMap[2 * key + 1] = y;
                    }
                }
            }
        }
        if (collisions && Logger::WARNING_LOGGER != NULL) {
            Logger::WARNING_LOGGER->log("CACHE", "TILEMAP COLLISIONS DETECTED (NOT SUPPORTED YET)");
        }
        tileMapT->setSubImage(0, 0, id, tileMapT->getWidth(), 1, RG, UNSIGNED_BYTE, Buffer::Parameters(), CPUBuffer(tileMap));
        return true;
    }

    int n = 0;
    for (int l = 0; l <= maxLevel; ++l) {
        float tileSize = rootQuadSize / (1 << l);
        int tx0 = (int) floor(camera.x / (2 * tileSize));
        int ty0 = (int) floor(camera.y / (2 * tileSize));
        for (int ty = 2 * (ty0 - k); ty <= 2 * (ty0 + k) + 1; ++ty) {
            for (int tx = 2 * (tx0 - k); tx <= 2 * (tx0 + k) + 1; ++tx) {
                GPUTileStorage::GPUSlot *gpuData = NULL;
                TileCache::Tile* t;
                if (tx >= 0 && tx < (1 << l) && ty >= 0 && ty < (1 << l)) {
                    t = findTile(l, tx, ty);
                } else {
                    t = NULL;
                }
                if (t != NULL) {
                    TileStorage::Slot* data = t->getData(false);
                    if (data != NULL) {
                        gpuData = dynamic_cast<GPUTileStorage::GPUSlot*> (data);
                    }
                }
                if (n < tileMapT->getWidth()) {
                    int x, y;
                    if (gpuData == NULL) {
                        x = 0;
                        y = 0;
                    } else {
                        x = gpuData->l % 256;
                        y = gpuData->l / 256 + 1;
                    }
                    tileMap[2 * n] = x;
                    tileMap[2 * n + 1] = y;
                    ++n;
                }
            }
        }
    }
    tileMapT->setSubImage(0, 0, id, n, 1, RG, UNSIGNED_BYTE, Buffer::Parameters(), CPUBuffer(tileMap));
    return true;
}

void TileProducer::swap(ptr<TileProducer> p)
{
    cache->invalidateTiles(id);
    p->cache->invalidateTiles(p->id);
    std::swap(taskType, p->taskType);
    std::swap(cache, p->cache);
    std::swap(gpuProducer, p->gpuProducer);
    //std::swap(id, p->id);
    //std::swap(rootQuadSize, p->rootQuadSize);
    std::swap(tileMap, p->tileMap);
}

void* TileProducer::getContext() const
{
    return NULL;
}

void TileProducer::getReferencedProducers(vector< ptr<TileProducer> > &producers) const
{
}

int TileProducer::getLayerCount() const
{
    return int(layers.size());
}

ptr<TileLayer> TileProducer::getLayer(int index) const
{
    return layers[index];
}

bool TileProducer::hasLayers() const
{
    return layers.size() > 0;
}

void TileProducer::addLayer(ptr<TileLayer> l)
{
    layers.push_back(l);
}

ptr<Task> TileProducer::startCreateTile(int level, int tx, int ty, unsigned int deadline, ptr<Task> task, ptr<TaskGraph> owner)
{
    for (int i = 0; i < (int) layers.size(); i++) {
        layers[i]->startCreateTile(level, tx, ty, deadline, task, owner);
    }
    return owner == NULL ? task : owner.cast<Task>();
}

void TileProducer::beginCreateTile()
{
    for (int i = 0; i < (int) layers.size(); i++) {
        layers[i]->beginCreateTile();
    }
}

bool TileProducer::doCreateTile(int level, int tx, int ty, TileStorage::Slot * data)
{
    bool changes = false;
    for (int i = 0; i < (int) layers.size(); i++) {
        if (layers[i]->isEnabled()) {
            changes |= layers[i]->doCreateTile(level, tx, ty, data);
        }
    }
    return changes;
}

void TileProducer::endCreateTile()
{
    for (int i = 0; i < (int) layers.size(); i++) {
        layers[i]->endCreateTile();
    }
}

void TileProducer::stopCreateTile(int level, int tx, int ty)
{
    for (int i = 0; i < (int) layers.size(); i++) {
        layers[i]->stopCreateTile(level, tx, ty);
    }
}

ptr<Task> TileProducer::createTile(int level, int tx, int ty, TileStorage::Slot *data, unsigned int deadline, ptr<Task> old)
{
    assert(data != NULL);
    if (old != NULL) {
        ptr<CreateTileTaskGraph> r = old.cast<CreateTileTaskGraph>();
        if (r != NULL) {
            r->restore();
        } else {
            assert(old.cast<CreateTile>() != NULL);
        }
        return old;
    }
    ptr<CreateTile> t = new CreateTile(this, level, tx, ty, data, deadline);
    ptr<Task> r = startCreateTile(level, tx, ty, deadline, t, NULL);
    pthread_mutex_lock((pthread_mutex_t*) mutex);
    tasks.push_back(t.get());
    if (r.get() != t.get()) {
        assert(r.cast<CreateTileTaskGraph>() != NULL);
        tasks.push_back(r.get());
    }
    pthread_mutex_unlock((pthread_mutex_t*) mutex);
    return r;
}

ptr<TaskGraph> TileProducer::createTaskGraph(ptr<Task> task)
{
    ptr<CreateTile> t = task.cast<CreateTile>();
    assert(t != NULL);
    ptr<CreateTileTaskGraph> r = new CreateTileTaskGraph(this);
    r->addTask(t);
    r->root = t.get();
    t->parent = r.get();
    return r;
}

void TileProducer::removeCreateTile(Task *t)
{
    pthread_mutex_lock((pthread_mutex_t*) mutex);
    vector<Task*>::iterator i = find(tasks.begin(), tasks.end(), t);
    assert(i != tasks.end());
    tasks.erase(i);
    pthread_mutex_unlock((pthread_mutex_t*) mutex);
}

}
