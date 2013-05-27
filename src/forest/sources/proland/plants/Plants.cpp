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

#include "proland/plants/Plants.h"

#include "ork/resource/ResourceTemplate.h"

#include "proland/math/noise.h"

using namespace std;
using namespace ork;

namespace proland
{

static const float TWO_PI = (float) (M_PI*2);

#define K_SMALLEST_RANGE 0.000001f

// % of plane covered with disks when genereated with algorithm below
#define POISSON_COVERAGE 0.6826

/**
 * Allows fast-computing of the available area around a point, using
 * angles. Acquired from Qizhi Yu's implementation of his thesis, itself
 * acquired from Daniel Dunbar & Gred Humphreys in "A Spatial Data structure
 * for fast poisson disk sample generation".
 */
class RangeList
{
public:
    struct RangeEntry
    {
        float min;

        float max;

        RangeEntry() : min(0.0f), max(0.0f)
        {
        }
    };

    /**
     * Creates a new RangleList.
     */
    RangeList()
    {
        numRanges = 0;
        rangesSize = 8;
        ranges = new RangeEntry[rangesSize];
    }

    /**
     * Deletes this RangeList.
     */
    ~RangeList()
    {
        delete[] ranges;
    }

    /**
     * Returns the number of ranges in this list.
     */
    int getRangeCount() const
    {
        return numRanges;
    }

    /**
     * Returns the range entry at the given index.
     */
    const RangeEntry *getRange(int index) const
    {
        return ranges + index;
    }

    /**
     * Resets the list of range entries.
     *
     * @param min min angle to search from.
     * @param max max angle to search from.
     */
    void reset(float min, float max)
    {
        numRanges = 1;
        ranges[0].min = min;
        ranges[0].max = max;
    }

    /**
     * Removes an area from the available neighboring.
     *
     * @param min begining of the angle to remove.
     * @param max end of the angle to remove.
     */
    void subtract(float min, float max)
    {
        if (min > TWO_PI) {
            subtract(min - TWO_PI, max - TWO_PI);
        } else if (max < 0) {
            subtract(min + TWO_PI, max + TWO_PI);
        } else if (min < 0) {
            subtract(0, max);
            subtract(min + TWO_PI, TWO_PI);
        } else if (max > TWO_PI) {
            subtract(min, TWO_PI);
            subtract(0, max - TWO_PI);
        } else if (numRanges > 0) {
            int pos;
            if (min < ranges[0].min) {
                pos = -1;
            } else {
                int lo = 0;
                int mid = 0;
                int hi = numRanges;
                while (lo < hi - 1) {
                    mid = (lo + hi) >> 1;
                    if (ranges[mid].min < min) {
                        lo = mid;
                    } else {
                        hi = mid;
                    }
                }
                pos = lo;
            }
            if (pos == -1) {
                pos = 0;
            } else if (min < ranges[pos].max) {
                float c = ranges[pos].min;
                float d = ranges[pos].max;
                if (min - c < K_SMALLEST_RANGE) {
                    if (max < d) {
                        ranges[pos].min = max;
                    } else {
                        deleteRange(pos);
                    }
                } else {
                    ranges[pos].max = min;
                    if (max < d) {
                        insertRange(pos + 1, max, d);
                    }
                    pos++;
                }
            } else {
                if (pos < numRanges - 1 && max > ranges[pos + 1].min) {
                    pos++;
                } else {
                    return;
                }
            }
            while (pos < numRanges && max >= (ranges[pos].min)) {
                if (ranges[pos].max - max < K_SMALLEST_RANGE) {
                    deleteRange(pos);
                } else {
                    ranges[pos].min = max;
                    if (ranges[pos].max > max) {
                        break;
                    }
                }
            }
        }
    }

private:
    /**
     * List of entries corresponding to neighboring objects.
     */
    RangeEntry *ranges;

    /**
     * Number of entries.
     */
    int numRanges;

    /**
     * Size of #ranges.
     */
    int rangesSize;

    /**
     * Removes an entry.
     *
     * @param pos the index of the entry to remove.
     */
    void deleteRange(int pos)
    {
        if (pos < numRanges - 1) {
            memmove(&ranges[pos], &ranges[pos + 1], sizeof(*ranges) * (numRanges - (pos + 1)));
        }
        numRanges--;
    }

    /**
     * Adds an entry at a given position.
     *
     * @param pos an index.
     * @param min begining of the angle to remove.
     * @param max end of the angle to remove.
     */
    void insertRange(int pos, float min, float max)
    {
        if (numRanges == rangesSize) {
            rangesSize++;
            RangeEntry *tmp = new RangeEntry[rangesSize];
            memcpy(tmp, ranges, numRanges * sizeof(*tmp));
            delete[] ranges;
            ranges = tmp;
        }

        assert(pos < rangesSize);
        if (pos < numRanges) {
            memmove(&ranges[pos + 1], &ranges[pos], sizeof(*ranges) * (numRanges - pos));
        }
        ranges[pos].min = min;
        ranges[pos].max = max;
        numRanges++;
    }
};

class PlantsGrid
{
public:
    PlantsGrid(float radius, int maxParticlesPerCell)
    {
        this->radius = radius;
        this->maxParticlesPerCell = maxParticlesPerCell;
        gridSize.x = (int) (1.0 / radius);
        gridSize.y = (int) (1.0 / radius);
        cellSizes = new int[gridSize.x * gridSize.y];
        cellContents = new vec2f[gridSize.x * gridSize.y * maxParticlesPerCell];
        int n = gridSize.x * gridSize.y;
        for (int i = 0; i < n; ++i) {
            cellSizes[i] = 0;
        }
    }

    ~PlantsGrid()
    {
        delete[] cellSizes;
        delete[] cellContents;
    }

    vec2i getGridSize() const
    {
        return gridSize;
    }

    vec2i getCell(const vec2f &p)
    {
        int i = (int) floor(p.x * gridSize.x);
        int j = (int) floor(p.y * gridSize.y);
        return vec2i(i, j);
    }

    int getCellSize(const vec2i &cell)
    {
        assert(cellSizes != NULL && cell.x >= 0 && cell.x < gridSize.x && cell.y >= 0 && cell.y < gridSize.y);
        return cellSizes[cell.x + cell.y * gridSize.x];
    }

    vec2f *getCellContent(const vec2i &cell)
    {
        assert(cellSizes != NULL && cell.x >= 0 && cell.x < gridSize.x && cell.y >= 0 && cell.y < gridSize.y);
        return cellContents + (cell.x + cell.y * gridSize.x) * maxParticlesPerCell;
    }

    void addParticle(const vec2f &p)
    {
        assert(cellSizes != NULL);
        vec2i cmin = getCell(p - vec2f(radius, radius));
        vec2i cmax = getCell(p + vec2f(radius, radius));

        cmin.x = max(0, cmin.x);
        cmin.y = max(0, cmin.y);
        cmax.x = min(gridSize.x - 1, cmax.x);
        cmax.y = min(gridSize.y - 1, cmax.y);

        for (int j = cmin.y; j <= cmax.y; ++j) {
            for (int i = cmin.x; i <= cmax.x; ++i) {
                int index = i + j * gridSize.x;
                int size = cellSizes[index];
                if (size < maxParticlesPerCell) {
                    cellSizes[index] = size + 1;
                    cellContents[size + index * maxParticlesPerCell] = p;
                } else {
                    // overflow!
                    assert(false);
                }
            }
        }
    }

private:
    float radius;

    int maxParticlesPerCell;

    vec2i gridSize;

    int *cellSizes;

    vec2f *cellContents;
};

Plants::Plants() : Object("Plants")
{
}

Plants::Plants(int minLevel, int maxLevel, int minDensity, int maxDensity, int tileCacheSize, float maxDistance) : Object("Plants")
{
    init(minLevel, maxLevel, minDensity, maxDensity, maxDistance, tileCacheSize);
}

void Plants::init(int minLevel, int maxLevel, int minDensity, int maxDensity, int tileCacheSize, float maxDistance)
{
    this->minLevel = minLevel;
    this->maxLevel = maxLevel;
    this->minDensity = minDensity;
    this->maxDensity = maxDensity;
    this->tileCacheSize = tileCacheSize;
    this->maxDistance = maxDistance;
}

Plants::~Plants()
{
}

int Plants::getMinLevel()
{
    return minLevel;
}

int Plants::getMaxLevel()
{
    return maxLevel;
}

int Plants::getMaxDensity()
{
    return maxDensity;
}

int Plants::getTileCacheSize()
{
    return tileCacheSize;
}

float Plants::getMaxDistance()
{
    return maxDistance;
}

int Plants::getPatternCount()
{
    return int(patterns.size());
}

float Plants::getPoissonRadius()
{
    return 1.0 / sqrt(0.5 * (minDensity + maxDensity) * M_PI / POISSON_COVERAGE);
}

ptr<MeshBuffers> Plants::getPattern(int index)
{
    return patterns[index];
}

void Plants::addPattern(ptr<MeshBuffers> pattern)
{
    patterns.push_back(pattern);
}

void Plants::setMaxDistance(float maxDistance)
{
    this->maxDistance = maxDistance;
    if (renderProg->getUniform1f("maxTreeDistance") != NULL) {
        renderProg->getUniform1f("maxTreeDistance")->set(maxDistance);
    }
}

void Plants::swap(ptr<Plants> p)
{
    std::swap(selectProg, p->selectProg);
    std::swap(renderProg, p->renderProg);
    std::swap(minLevel, p->minLevel);
    std::swap(maxLevel, p->maxLevel);
    std::swap(maxDensity, p->maxDensity);
    std::swap(tileCacheSize, p->tileCacheSize);
    std::swap(maxDistance, p->maxDistance);
    std::swap(patterns, p->patterns);
}

class PlantsResource : public ResourceTemplate<40, Plants>
{
public:
    long rand;

    RangeList *ranges;

    PlantsGrid *grid;

    float radius;

    float randUnsignedInt() {
        union {
            float f;
            unsigned int ui;
        } val;
        val.ui = (lrandom(&rand) & 0xFFFF) | ((lrandom(&rand) & 0xFFFF) << 16);
        return val.f;
    }

    void generatePattern(ptr< Mesh<vec3f, unsigned short> > &pattern)
    {
        ranges = new RangeList();
        grid = new PlantsGrid(4.0f * radius, 64);

        vector<vec2f> candidates;

        vec2f p(0.5, 0.5);
        candidates.push_back(p);
        pattern->addVertex(vec3f(p.x, p.y, randUnsignedInt()));
        grid->addParticle(p);

        while (!candidates.empty()) {
            // selects a candidate at random
            int c = lrandom(&rand) % (int) candidates.size();
            p = candidates[c];
            // removes this candidate from the list
            candidates[c] = candidates[int(candidates.size()) - 1];
            candidates.pop_back();

            ranges->reset(0.0f, 2.0f * M_PI);
            findNeighborRanges(p);

            while (ranges->getRangeCount() != 0) {
                // selects a range at random
                const RangeList::RangeEntry *re = ranges->getRange(lrandom(&rand) % ranges->getRangeCount());
                // selects a point at random in this range
                float angle = re->min + (re->max - re->min) * frandom(&rand);
                ranges->subtract(angle - M_PI / 3.0f, angle + M_PI / 3.0f);

                vec2f pt = p + vec2f(cos(angle), sin(angle)) * 2.0f * radius;
                if (pt.x >= 0.0 && pt.x < 1.0 && pt.y >= 0.0 && pt.y < 1.0) {
                    candidates.push_back(pt);
                    pattern->addVertex(vec3f(pt.x, pt.y, randUnsignedInt()));
                    grid->addParticle(pt);
                }
            }
        }

        delete ranges;
        delete grid;
    }

    void generateRandomPattern(ptr< Mesh<vec3f, unsigned short> > &pattern, int n) {
        for (int i = 0; i < n; ++i) {
            float x = frandom(&rand);
            float y = frandom(&rand);
            pattern->addVertex(vec3f(x, y, randUnsignedInt()));
        }
    }

    void findNeighborRanges(const vec2f &p)
    {
        vec2i gridSize = grid->getGridSize();
        vec2i cell = grid->getCell(p);

        float rangeSqrD = 16.0f * radius * radius;

        int n = grid->getCellSize(cell);
        vec2f *neighbors = grid->getCellContent(cell);
        int count = 0;
        for (int j = 0; j < n; ++j) {
            vec2f ns = neighbors[j];
            if (ns == p) {
                continue;
            }
            vec2f v = ns - p;
            float sqrD = v.squaredLength();
            if (sqrD < rangeSqrD) {
                float dist = sqrt(sqrD);
                float angle = atan2(v.y, v.x);
                float theta = safe_acos(0.25f * dist / radius);
                count++;
                ranges->subtract(angle - theta, angle + theta);
            }
        }
    }

    PlantsResource(ptr<ResourceManager> manager, const string &name, ptr<ResourceDescriptor> desc, const TiXmlElement *e = NULL) :
        ResourceTemplate<40, Plants>(manager, name, desc)
    {
        e = e == NULL ? desc->descriptor : e;
        checkParameters(desc, e, "name,selectProg,shadowProg,renderProg,minLevel,maxLevel,tileCacheSize,maxDistance,lodDistance,minDensity,maxDensity,patternCount,");
        int minLevel;
        int maxLevel;
        int tileCacheSize;
        float maxDistance;
        float lodDistance;
        int minDensity;
        int maxDensity;
        int patternCount;
        getIntParameter(desc, e, "minLevel", &minLevel);
        getIntParameter(desc, e, "maxLevel", &maxLevel);
        getIntParameter(desc, e, "tileCacheSize", &tileCacheSize);
        getFloatParameter(desc, e, "maxDistance", &maxDistance);
        getFloatParameter(desc, e, "lodDistance", &lodDistance);
        getIntParameter(desc, e, "minDensity", &minDensity);
        getIntParameter(desc, e, "maxDensity", &maxDensity);
        getIntParameter(desc, e, "patternCount", &patternCount);

        selectProg = manager->loadResource(getParameter(desc, e, "selectProg")).cast<Program>();
        if (e->Attribute("shadowProg") != NULL) {
            shadowProg = manager->loadResource(getParameter(desc, e, "shadowProg")).cast<Program>();
        }
        renderProg = manager->loadResource(getParameter(desc, e, "renderProg")).cast<Program>();

        if (renderProg->getUniform1f("maxTreeDistance") != NULL) {
            renderProg->getUniform1f("maxTreeDistance")->set(maxDistance);
        }

        rand = 1234567;
        int minVertices = 2 * maxDensity;
        int maxVertices = 0;
        for (int i = 0; i < patternCount; ++i) {
            ptr< Mesh<vec3f, unsigned short> > pattern;
            int density = minDensity + int((maxDensity - minDensity) * frandom(&rand));
            pattern = new Mesh<vec3f, unsigned short>(POINTS, GPU_STATIC, density, 0);
            pattern->addAttributeType(0, 3, A32F, false);

            radius = 1.0 / sqrt(density * M_PI / POISSON_COVERAGE);
            generatePattern(pattern);
            //generateRandomPattern(pattern, minDensity);

            addPattern(pattern->getBuffers());
            minVertices = min(minVertices, pattern->getVertexCount());
            maxVertices = max(maxVertices, pattern->getVertexCount());
        }

        init(minLevel, maxLevel, minVertices, maxVertices, tileCacheSize, maxDistance);
    }

    virtual bool prepareUpdate()
    {
        bool changed = false;
        if (Resource::prepareUpdate()) {
            changed = true;
        } else if (dynamic_cast<Resource*>(selectProg.get())->changed()) {
            changed = true;
        } else if (dynamic_cast<Resource*>(renderProg.get())->changed()) {
            changed = true;
        } else if (shadowProg != NULL && dynamic_cast<Resource*>(shadowProg.get())->changed()) {
            changed = true;
        }

        if (changed) {
            oldValue = NULL;
            try {
                oldValue = new PlantsResource(manager, name, newDesc == NULL ? desc : newDesc);
            } catch (...) {
            }
            if (oldValue != NULL) {
                swap(oldValue);
                return true;
            }
            return false;
        }
        return true;
    }
};

extern const char plants[] = "plants";

static ResourceFactory::Type<plants, PlantsResource> PlantsType;

}
