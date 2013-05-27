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

#include "proland/rivers/HydroFlowTile.h"

#include "pmath.h"

#include "ork/core/Logger.h"
#include "ork/core/Timer.h"

#include "proland/math/seg2.h"
#include "proland/math/geometry.h"
#include "proland/graph/Curve.h"

namespace proland
{

// timers -> debug
static int nbHydroDatas = 0;

static Timer* swTotalH = NULL;

static Timer* swGetEdges = NULL;

static Timer* swInRiver = NULL;

static Timer* swDistances = NULL;

static Timer* swGetPotential = NULL;

static Timer* swLoop = NULL;

static Timer* sw1 = NULL;

static Timer* sw2 = NULL;

static Timer* sw3 = NULL;

//static Timer* sw4 = NULL;

//static Timer* sw5 = NULL;

//static Timer* sw6 = NULL;

static int sw1Count;

static int getVelocityCount;

/**
 * Computes signed distance between a segment ab to a point p.
 */
static float signedSegmentDistSq(vec2d a, vec2d b, const vec2d p)
{
    float res = seg2d(a, b).segmentDistSq(p);
    if (cross(b - a, p - a) < 0.0f) {
        res = -res;
    }
    return res;
}

HydroFlowTile::DistCell::DistCell()
{
    coords = vec3d(0.0f, 0.0f, 0.0f);
}

HydroFlowTile::DistCell::DistCell(vec3d coords)
{
    this->coords = coords;
    center = vec2d(coords.x + coords.z / 2.0f, coords.y + coords.z / 2.0f);
}

HydroFlowTile::DistCell::~DistCell()
{
    for (int i = 0; i < MAX_BANK_NUMBER; ++i) {
        edges[i].clear();
    }
    bankIds.clear();
    riverIds.clear();
}

HydroFlowTile::HydroFlowTile(float ox, float oy, float size, float inter_power, int cacheSize, float searchRadiusFactor) :
    FlowTile(ox, oy, size)
{
    version = (unsigned int) -1;
    maxWidth = 0.f;
    this->inter_power = inter_power;
    this->searchRadiusFactor = searchRadiusFactor;
    this->cacheSize = cacheSize;
    this->potentials = new float[cacheSize * cacheSize];
    for (int i = 0; i < cacheSize * cacheSize; i++) {
        potentials[i] = INFINITY;
    }

    this->DISTANCES = new float[MAX_BANK_NUMBER];
    this->CLOSESTEDGESIDS = new int[MAX_BANK_NUMBER];
    for (int i = 0; i< MAX_BANK_NUMBER; i++) {
        DISTANCES[i] = INFINITY;
        CLOSESTEDGESIDS[i] = -1;
    }
    this->distCells = new DistCell[MAX_NUM_DIST_CELLS * MAX_NUM_DIST_CELLS];

    sw1Count = 0;
    getVelocityCount = 0;
    if (nbHydroDatas == 0) {
        swTotalH = new Timer();
        swGetEdges = new Timer();
        swInRiver = new Timer();
        swDistances = new Timer();
        swGetPotential = new Timer();
        swLoop = new Timer();
        sw1 = new Timer();
        sw2 = new Timer();
        sw3 = new Timer();
    }
    nbHydroDatas++;

    numDistCells = 0;
}

HydroFlowTile::~HydroFlowTile()
{
    nbHydroDatas--;
    if (nbHydroDatas == 0) {
        cout<<"====FLOWDATA Performance report===:"<<endl;
        float total = 1 / swTotalH->getAvgTime();
        cout<<"Total: "<<total<<" s/frame  "<<1/total<<" frame/s"<<endl;
        printf("== swGetEdges\t\t %6.4f%%\t  %f\n", swGetEdges->getAvgTime()/total*100, swGetEdges->getAvgTime());
        printf("== swInRiver\t\t %6.4f%%\t  %f\n", swInRiver->getAvgTime()/total*100, swInRiver->getAvgTime());
        printf("== swComputePotentials\t %6.4f%%\t %f\n", swLoop->getAvgTime()/total*100, swLoop->getAvgTime());
        printf("=== swDistances\t\t  %6.4f%%\t  %f\n", 4.0f * swDistances->getAvgTime()/total*100, 4.0f * swDistances->getAvgTime());
        printf("=== swGetPotential\t  %6.4f%%\t  %f\n", 4.0f * swGetPotential->getAvgTime()/total*100, 4.0f * swGetPotential->getAvgTime());
        printf("==sw1 %f (total)\n", sw1->getAvgTime());
        delete swTotalH;
        delete swGetEdges;
        delete swInRiver;
        delete swDistances;
        delete swGetPotential;
        delete swLoop;
        delete sw1;
        delete sw2 ;
        delete sw3 ;
//        cout << sw1.GetAvgTime() << endl;
//        printf("==sw2 %f (get4Potentials)\n", sw2.GetAvgTime());
//        printf("==sw3 %f (distances + potential)\n", sw3.GetAvgTime());

//        cout<<"== swGetEdges "<< swGetEdges.GetAvgTime()/total*100<<"%"<<endl;
//        cout<<"== swInRiver "<< swInRiver.GetAvgTime()/total*100<<"%"<<endl;
//        cout<<"== swDistances "<< 4*swDistances.GetAvgTime()/total*100<<"%"<<endl;
//        cout<<"== swGetPotential "<< 4*swGetPotential.GetAvgTime()/total*100<<"%"<<endl;
//        cout<<"== sw1 "<< (sw1Count / getVelocityCount) * sw1.GetAvgTime() * 100 / total <<endl;
    }
//    cout<<"== sw2 "<< sw2.GetAvgTime() * 1000<<endl;
//    cout<<"== sw3 "<< sw3.GetAvgTime() * 1000<<endl;
//    cout<<"== sw4 "<< sw4.GetAvgTime() * 1000<<endl;
//    cout<<"== sw5 "<< sw5.GetAvgTime() * 1000<<endl;
//    cout<<"== sw6 "<< sw6.GetAvgTime() * 1000<<endl;

    banks.clear();
    delete[] DISTANCES;
    delete[] CLOSESTEDGESIDS;
    delete[] potentials;
    delete[] distCells;
}

void HydroFlowTile::addBanks(vector<ptr<HydroCurve> > &curves, float maxWidth)
{
    this->maxWidth = max(this->maxWidth, maxWidth);
    maxSearchDist = max(maxWidth * searchRadiusFactor, size / MAX_NUM_DIST_CELLS);
    float cellSize = maxSearchDist;
    numDistCells = (int) ceil(size / cellSize);

    for (int i = 0; i < numDistCells; i++) {
        for (int j = 0; j < numDistCells; j++) {
            distCells[i + j * numDistCells] = DistCell(vec3d(ox + i * cellSize, oy + j * cellSize, cellSize));
        }
    }

    for (int j = 0; j < numDistCells * numDistCells; j++) {
        vec3d center = distCells[j].coords;
        distCells[j].bounds = box2d(center.x - cellSize, center.x + center.z + cellSize, center.y - cellSize, center.y + center.z + cellSize);
    }
    set<CurveId> riverIds;

    box2f bounds;
    for (vector<ptr<HydroCurve> >::iterator it = curves.begin(); it != curves.end(); it ++) {
        ptr<HydroCurve> h = *it;
        if (h->getType() == HydroCurve::BANK) {
            continue;
        }
        int bankId = (int) banks.size();
        if (bankId >= MAX_BANK_NUMBER) {
            break;
        }
        vec2d prev = h->getXY(0);
        vec2d cur;
        for (int i = 1; i < h->getSize(); i++) {
            cur = h->getXY(i);
            for (int j = 0; j < numDistCells * numDistCells; j++) {
            //for (int j = 0; j < NUM_DIST_CELLS * NUM_DIST_CELLS; j++) {
                if (clipSegment(distCells[j].bounds, prev, cur)) {
                    distCells[j].edges[bankId].push_back(i - 1);
                    distCells[j].bankIds.insert(bankId);
                }
            }
            prev = cur;
        }
        banks.push_back(h);
    }
    for (vector<ptr<HydroCurve> >::iterator it = curves.begin(); it != curves.end(); it ++) {
        ptr<HydroCurve> h = *it;
        if (h->getType() != HydroCurve::BANK) {
            continue;
        }
        int bankId = (int) banks.size();
        if (bankId >= MAX_BANK_NUMBER) {
            break;
        }
        vec2d prev = h->getXY(0);
        vec2d cur;
        bool inside = false;
        for (int i = 1; i < h->getSize(); i++) {
            cur = h->getXY(i);
            for (int j = 0; j < numDistCells * numDistCells; j++) {
            //for (int j = 0; j < NUM_DIST_CELLS * NUM_DIST_CELLS; j++) {
                if (clipSegment(distCells[j].bounds, prev, cur)) {
                    distCells[j].edges[bankId].push_back(i - 1);
                    distCells[j].bankIds.insert(bankId);
                    inside = true;
                }
            }
            prev = cur;
        }
        if (inside) {
            banks.push_back(h);
            riversToBanks[h->getRiver()].push_back(bankId);
        }
    }
}

bool HydroFlowTile::isInRiver(vec2d &pos, DistCell *distCell, int &riverId)
{
    float widthSq, dist;
    int bankId, edgeId;
    ptr<HydroCurve> h = NULL;

    for (set<int>::iterator i = distCell->bankIds.begin(); i != distCell->bankIds.end(); i++) {
        bankId = *i;
        h = banks[bankId];
        assert(h != NULL);
        if (h->getType() == HydroCurve::BANK || (int) distCell->edges[bankId].size() == 0) {
            continue;
        }

        widthSq = h->getWidth() * h->getWidth() / 4.0f; //(w/2)^2
        for (vector<int>::iterator j = distCell->edges[bankId].begin(); j != distCell->edges[bankId].end(); j++) {
            edgeId = *j;
            dist = seg2d(h->getXY(edgeId), h->getXY(edgeId + 1)).segmentDistSq(pos);
            if (dist < widthSq) {
                riverId = bankId;
                return true;
            }
        }
    }
    return false;
}

void HydroFlowTile::getDistancesToBanks(vec2d &pos, DistCell *distCell, set<int> &bankIds, map<int, float> &distances)
{
    float distance, potential;
    int edgeId, curId, bankId;
    set<int> ids;
    ptr<HydroCurve> h = NULL;
    map<float, int> potentials;
    map<float, int>::iterator pit;

    float epsilon = 0.0001f, error;
    distances.clear();

    for (set<int>::iterator it = bankIds.begin(); it != bankIds.end(); it++) {
        curId = *it;
        h = banks[curId];
        if (h->getType() != HydroCurve::BANK) {
            continue;
        }
        potential = h->getPotential();
        pit = potentials.find(potential);
        if (pit != potentials.end()) {
            float curWidth = h->getWidth();
            float bankWidth = banks[pit->second]->getWidth();
            if (bankWidth >= curWidth) {
                bankId = pit->second;
            } else {
                bankId = curId;
                DISTANCES[bankId] = DISTANCES[pit->second];
                DISTANCES[pit->second] = INFINITY;
                ids.erase(pit->second);
                ids.insert(bankId);
                potentials[potential] = bankId;
            }
        } else {
            bankId = curId;
            potentials[potential] = bankId;
        }
        for (vector<int>::iterator it2 = distCell->edges[curId].begin(); it2 != distCell->edges[curId].end(); it2++) {
            edgeId = *it2;
            distance = signedSegmentDistSq(h->getXY(edgeId), h->getXY(edgeId + 1), pos);

            error = abs((abs(distance) - abs(DISTANCES[bankId])) / distance); //compute relative error between the two distances.

            if (error < epsilon) {//if the two distances are the same (i.e. linked by a node) we check if we are inside a river.
                if (distance < 0.f) {
                    distance = DISTANCES[bankId];
                }
            }
            if (abs(DISTANCES[bankId]) > abs(distance) || error < epsilon) {
               // printf("%d -> %f:%f (%f)\n", edgeId, DISTANCES[bankId], distance, error);
                ids.insert(bankId);
                DISTANCES[bankId] = distance;
            }
        }
    }

    bool ok = true;
    for (set<int>::iterator i = ids.begin(); i != ids.end(); i++) {
        bankId = (*i);
        if (ok) {
            if (DISTANCES[bankId] < 0.f) {
                distances.clear();
                ok = false;
            } else {
                distances[bankId] = sqrt(DISTANCES[bankId]);
            }
        }
        DISTANCES[bankId] = INFINITY;
        CLOSESTEDGESIDS[bankId] = -1;
    }
}

float smooth_func(float t)
{
    return 6 * pow(t, 5) - 15 * pow(t, 4) + 10 * pow(t, 3);
}

void HydroFlowTile::getPotential(vec2d &pos, map<int, float> &distances, float &potential, int &type)
{
    if ((int) distances.size() < 2) {
        type = FlowTile::OUTSIDE;
        return;
    }

    float frac_d = 0.0f;
    float frac_n = 0.0f;

    vector<pair<float, pair<float, float> > >weights (distances.size());
    int j = 0;

    float w = maxWidth * searchRadiusFactor;
    ptr<HydroCurve> h;
    float m = 0.f;
    for (map<int, float>::iterator i = distances.begin(); i != distances.end(); i++) {
        h = banks[i->first];
        float p = h->getPotential();
        float d = i->second;

        if (d > maxWidth) {
            d = maxWidth;
        }
        m = max(h->getWidth(), m);
        weights[j++] = make_pair(d, make_pair(w, p));
    }

    for (int i = 0; i < (int) weights.size(); i++) {
        pair<float, pair<float, float> > k = weights[i];
        float d = k.first;
        float w = k.second.first;
        float p = k.second.second;
        if (w == 0.0f) {
            continue;
        }
        w = m * searchRadiusFactor;

        float s = smooth_func(1.0f - d / w);
        float prod = 2.0f;
        for (int j = 0; j < (int) weights.size(); j++) {
            if (i != j) {
                prod *= pow(weights[j].first, inter_power);
            }
        }

        float t = prod * s;

        frac_d += t;
        frac_n += t * p;
    }

    potential = frac_n / frac_d;
    type = FlowTile::INSIDE;
}

void HydroFlowTile::getLinkedEdges(vec2d &pos, DistCell *distCell, int riverId, set<int> &bankIds)
{
    ptr<HydroCurve> h = banks[riverId]->getAncestor().cast<HydroCurve>();
    NodePtr start = h->getStart();
    NodePtr end = h->getEnd();

    for (int i = 0; i < start->getCurveCount(); i++) {
        ptr<HydroCurve> c = start->getCurve(i).cast<HydroCurve>();
        vector<int> v = riversToBanks.find(c->getId())->second;
        for (vector<int>::iterator it = v.begin(); it != v.end(); it++) {
            if (distCell->bankIds.find(*it) != distCell->bankIds.end()) {
                bankIds.insert(*it);
            }
        }
    }

    for (int i = 0; i < end->getCurveCount(); i++) {
        ptr<HydroCurve> c = end->getCurve(i).cast<HydroCurve>();
        vector<int> v = riversToBanks.find(c->getAncestorId())->second;
        for (vector<int>::iterator it = v.begin(); it != v.end(); it++) {
            if (distCell->bankIds.find(*it) != distCell->bankIds.end()) {
                bankIds.insert(*it);
            }
        }
    }
}

void HydroFlowTile::getFourPotentials(vec2d &pos, vec4f &res, int &type)
{
//#define PRINT_DEBUG
    if (pos.x < ox || pos.x > ox + size || pos.y < oy || pos.y > oy + size) {
        type = FlowTile::ON_SKY;
        #ifdef PRINT_DEBUG
        printf("OUT OF QUAD : %f:%f (%f:%f:%f)\n", pos.x, pos.y, ox, oy, size);
        #endif
        return;
    }

    if (numDistCells == 0 || (int) banks.size() == 0) {
        type = FlowTile::OUTSIDE;
        #ifdef PRINT_DEBUG
        printf("NO CURVES IN QUAD %f:%f (%f:%f:%f)\n", pos.x, pos.y, ox, oy, size);
        #endif
        return;
    }

    float arrayCellSize = size / (cacheSize);
    int arrayX = (int) ((pos.x - ox ) * (cacheSize - 1) / size);
    int arrayY = (int) ((pos.y - oy ) * (cacheSize - 1) / size);

    vec2d chkPnts[4];
    int indices[4];

    for (int j = 0; j < 2; j++) {
        for (int i = 0; i < 2; i++) {
            indices[i + j * 2] = arrayX + i + (arrayY + j) * cacheSize;
            res[i + j * 2] = potentials[indices[i + j * 2]];
            chkPnts[i + j * 2] = vec2d(ox, oy) + vec2d((arrayX + i) * arrayCellSize, (arrayY + j) * arrayCellSize);
        }
    }
    bool hasNegativeInf = 0;
    bool hasInfinite = false;
    for (int i = 0; i < 4; i++) {
        float value = potentials[indices[i]];
        if (!isFinite(value) && value < 0) {
            hasNegativeInf = true;
            #ifdef PRINT_DEBUG
            printf("HAS NEGATIVE VALUE %f:%f@%f:%f (%f:%f:%f) %d:%d:(%d) (%f)-> (%d:%d)\n", pos.x, pos.y, chkPnts[i].x, chkPnts[i].y, ox, oy, size, cacheSize, arrayX, arrayY, arrayCellSize, arrayX + (i + 1) % 2, arrayY + i % 2);
            #endif
            break;
        }
        if (!isFinite(value)) {
            hasInfinite = true;
        }
    }
    if (hasNegativeInf) {
        type = FlowTile::OUTSIDE;
        return;
    }
    if (!hasInfinite) {
        type = FlowTile::INSIDE;
        return;
    }
    swGetEdges->start();

    set<int> bankIds;
    int riverId;
    map<int, float> distances; //maps distances for each BANK. We only keep the closest distance for each Bank.

    assert(numDistCells <= 8);
    float cellSize = size / numDistCells;
    int x = (int) ((pos.x - ox) / cellSize);
    int y = (int) ((pos.y - oy) / cellSize);
    DistCell *d = &distCells[x + y * numDistCells];
    assert(d != NULL);

    int bankCount = (int)d->bankIds.size();

    swGetEdges->end();
    if (bankCount < 3) { // if there isn't at least 1 river and a boundary.
        type = FlowTile::OUTSIDE;
        for (int i = 0; i < 4; i++) {
            potentials[indices[i]] = -INFINITY;
        }
        #ifdef PRINT_DEBUG
        printf("NOT ENOUGH EDGES IN QUAD : %f:%f (%f:%f:%f)-> %d (%d:%d) (%d:%d) (%f:%f:%f:%f)\n", pos.x, pos.y, ox, oy, size, bankCount, x, y, numDistCells, (int) banks.size(), d->bounds.xmin, d->bounds.ymin, d->bounds.xmax, d->bounds.ymax);
        #endif
        return;
    }

    swInRiver->start();
    bool inside = isInRiver(pos, d, riverId);
    swInRiver->end();

    if (!inside) {
        type = FlowTile::OUTSIDE;
        for (int i = 0; i < 4; i++) {
            potentials[indices[i]] = -INFINITY;
        }
        #ifdef PRINT_DEBUG
        printf("NOT INSIDE: %f:%f (%d:%d)\n", pos.x, pos.y, x, y);
        #endif
        return;
    }
    getLinkedEdges(pos, d, riverId, bankIds);
    if (bankIds.size() < 2) {
        type = FlowTile::OUTSIDE;
        #ifdef PRINT_DEBUG
        printf("NOT ENOUGTH EDGES : %f:%f -> %d(%d) (%d:%d)\n", pos.x, pos.y, (int) bankIds.size(), (int) bankIds.size() == 0 ? -1 : banks[*(bankIds.begin())]->getAncestorId().id, x, y);
        #endif
        return;
    } else {
    }

    swLoop->start();
    for(int i = 0; i < 4; i++) {
        if (isFinite(potentials[indices[i]])) {
            res[i] = potentials[indices[i]];
            continue;
        }
        swDistances->start();
        getDistancesToBanks(chkPnts[i], d, bankIds, distances);
        swDistances->end();

        swGetPotential->start();

        getPotential(chkPnts[i], distances, res[i], type);
        swGetPotential->end();
        if (type >= FlowTile::OUTSIDE) {
            potentials[indices[i]] = -INFINITY;
            #ifdef PRINT_DEBUG
            printf("INVALID POTENTIAL %f:%f (%d : %d)\n", pos.x, pos.y, (int) bankIds.size(), (int) distances.size());
            #endif
            break;
        }
        potentials[indices[i]] = res[i];
        if (!isFinite(res[i]) && Logger::DEBUG_LOGGER != NULL) {
            Logger::DEBUG_LOGGER->logf("RIVERS", "found a pb %d :%f:%f\n", type, chkPnts[i].x, chkPnts[i].y);
        }
    }
    swLoop->end();
}

void HydroFlowTile::getVelocity(vec2d &pos, vec2d &velocity, int &type)
{
    swTotalH->start();
    getVelocityCount++;
    vec4f p = vec4f(0.f, 0.f, 0.f, 0.f);
    sw1->start();
    getFourPotentials(pos, p, type);
    sw1->end();
    if (type > FlowTile::INSIDE) {
        velocity = vec2d(0, 0);
    } else {
        float pot = max(1.0f, size / cacheSize);
        velocity.y = (p[3] - p[2] + p[1] - p[0]) / (4.0f * pot);
        velocity.x = - (p[2] - p[0] + p[3] - p[1]) / (4.0f * pot);
        if (!isFinite(velocity.x + velocity.y)) {
            if (Logger::DEBUG_LOGGER != NULL) {
                Logger::DEBUG_LOGGER->logf("RIVERS","INVALID VELOCITY @%f:%f : %f:%f : %f:%f:%f:%f\n", pos.x, pos.y, velocity.x, velocity.y, p[0], p[1], p[2], p[3]);
            }
            velocity = vec2d(0.f, 0.f);
            type = FlowTile::OUTSIDE;
        }
    }
    swTotalH->end();
}

void HydroFlowTile::print()
{
    printf("FLOWDATA:%f:%f:%f\n", ox, oy, size);
    printf("Banks: %d\n", (int) banks.size());
    for (int i = 0; i <(int) banks.size(); i++)
    {
        ptr<HydroCurve> b = banks[i];
        printf("%d-> %d(%d):%f:%d (%d:%d)\n", i, b->getId().id, b->getAncestor()->getId().id, b->getPotential(), b->getType(), b->getRiver().id, b->getSize());
    }
    printf("LINKS: %d\n", (int) banks.size());
    for (map<CurveId, vector<int> > ::iterator i = riversToBanks.begin(); i != riversToBanks.end(); i++) {
        printf("%d: ",i->first.id);
        for (int j = 0; j < (int)i->second.size(); j++) {
            printf("%d ", i->second[j]);
        }
        printf("\n");
    }

    printf("DIST CELLS : [%dx%d]\nmaxSearchDist:%f\n", MAX_NUM_DIST_CELLS, MAX_NUM_DIST_CELLS, maxSearchDist);
    for (int i = 0; i < MAX_NUM_DIST_CELLS; i++) {
        for (int j = 0; j < MAX_NUM_DIST_CELLS; j++) {
            DistCell c = distCells[i * MAX_NUM_DIST_CELLS + j];
            printf("%f:%f:%f-> (%f:%f:%f:%f)[", c.coords.x, c.coords.y, c.coords.z, c.bounds.xmin, c.bounds.ymin, c.bounds.xmax, c.bounds.ymax);
            for (set<int>::iterator k = c.bankIds.begin(); k != c.bankIds.end(); k++) {
                int rank = *k;
                printf("%3d(%d):[%d->%d]", rank, banks[rank]->getAncestor()->getId().id, *(c.edges[rank].begin()), *(c.edges[rank].rbegin()));
            }
            printf("]\n");
        }
        printf("\n");
    }
}

}

