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

#include "proland/producer/TileStorage.h"

#include <pthread.h>

using namespace std;

namespace proland
{

TileStorage::Slot::Slot(TileStorage *owner) :
    producerTask(NULL), owner(owner)
{
    mutex = new pthread_mutex_t;
    pthread_mutex_init((pthread_mutex_t*) mutex, NULL);
}

TileStorage::Slot::~Slot()
{
    pthread_mutex_destroy((pthread_mutex_t*) mutex);
    delete (pthread_mutex_t*) mutex;
}

TileStorage *TileStorage::Slot::getOwner()
{
    return owner;
}

void TileStorage::Slot::lock(bool lock)
{
    if (lock) {
        pthread_mutex_lock((pthread_mutex_t*) mutex);
    } else {
        pthread_mutex_unlock((pthread_mutex_t*) mutex);
    }
}

TileStorage::TileStorage(int tileSize, int capacity) :
    Object("TileStorage")
{
    init(tileSize, capacity);
}

TileStorage::TileStorage() : Object("TileStorage")
{
}

void TileStorage::init(int tileSize, int capacity)
{
    this->tileSize = tileSize;
    this->capacity = capacity;
}

TileStorage::~TileStorage()
{
    list<Slot*>::iterator i = freeSlots.begin();
    while (i != freeSlots.end()) {
        delete *i;
        i++;
    }
    freeSlots.clear();
}

TileStorage::Slot *TileStorage::newSlot()
{
    list<Slot*>::iterator i = freeSlots.begin();
    if (i != freeSlots.end()) {
        Slot* s = *i;
        freeSlots.erase(i);
        return s;
    } else {
        return NULL;
    }
}

void TileStorage::deleteSlot(TileStorage::Slot *t)
{
    freeSlots.push_back(t);
}

int TileStorage::getTileSize()
{
    return tileSize;
}

int TileStorage::getCapacity()
{
    return capacity;
}

int TileStorage::getFreeSlots()
{
    return (int) freeSlots.size();
}

}
