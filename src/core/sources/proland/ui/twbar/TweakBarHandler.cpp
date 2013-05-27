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

#include "proland/ui/twbar/TweakBarHandler.h"

namespace proland
{

TweakBarHandler::TweakBarHandler() : Object("TweakBarHandler")
{
}

TweakBarHandler::TweakBarHandler(const char *name, ptr<EventHandler> eventHandler, bool active) : Object("TweakBarHandler")
{
    init(name, eventHandler, active);
}

void TweakBarHandler::init(const char *name, ptr<EventHandler> eventHandler, bool active)
{
    this->name = name;
    this->eventHandler = eventHandler;
    this->active = active;
    this->needUpdate = false;
}

TweakBarHandler::~TweakBarHandler()
{
}

const char* TweakBarHandler::getName()
{
    return name;
}

bool TweakBarHandler::isActive()
{
    return active;
}

void TweakBarHandler::setActive(bool active)
{
    this->active = active;
}

void TweakBarHandler::redisplay(double t, double dt, bool &needUpdate)
{
    needUpdate = this->needUpdate;
    this->needUpdate = false;
    if (eventHandler != NULL) {
        eventHandler->redisplay(t, dt);
    }
}

void TweakBarHandler::reshape(int x, int y, bool &needUpdate)
{
    needUpdate = false;
    if (eventHandler != NULL) {
        eventHandler->reshape(x, y);
    }
}

void TweakBarHandler::idle(bool damaged, bool &needUpdate)
{
    needUpdate = false;
    if (eventHandler != NULL) {
        eventHandler->idle(damaged);
    }
}

bool TweakBarHandler::mouseClick(EventHandler::button b, EventHandler::state s, EventHandler::modifier m, int x, int y, bool &needUpdate)
{
    needUpdate = false;
    if (eventHandler != NULL) {
        return eventHandler->mouseClick(b, s, m, x, y);
    }
    return false;
}

bool TweakBarHandler::mouseWheel(EventHandler::wheel b, EventHandler::modifier m, int x, int y, bool &needUpdate)
{
    needUpdate = false;
    if (eventHandler != NULL) {
        return eventHandler->mouseWheel(b, m, x, y);
    }
    return false;
}

bool TweakBarHandler::mouseMotion(int x, int y, bool &needUpdate)
{
    needUpdate = false;
    if (eventHandler != NULL) {
        return eventHandler->mouseMotion(x, y);
    }
    return false;
}

bool TweakBarHandler::mousePassiveMotion(int x, int y, bool &needUpdate)
{
    needUpdate = false;
    if (eventHandler != NULL) {
        return eventHandler->mousePassiveMotion(x, y);
    }
    return false;
}

bool TweakBarHandler::keyTyped(unsigned char c, EventHandler::modifier m, int x, int y, bool &needUpdate)
{
    needUpdate = false;
    if (eventHandler != NULL) {
        return eventHandler->keyTyped(c, m, x, y);
    }
    return false;
}

bool TweakBarHandler::keyReleased(unsigned char c, EventHandler::modifier m, int x, int y, bool &needUpdate)
{
    needUpdate = false;
    if (eventHandler != NULL) {
        return eventHandler->keyReleased(c, m, x, y);
    }
    return false;
}

bool TweakBarHandler::specialKey(EventHandler::key  k, EventHandler::modifier m, int x, int y, bool &needUpdate)
{
    needUpdate = false;
    if (eventHandler != NULL) {
        return eventHandler->specialKey(k, m, x, y);
    }
    return false;
}

bool TweakBarHandler::specialKeyReleased(EventHandler::key  k, EventHandler::modifier m, int x, int y, bool &needUpdate)
{
    needUpdate = false;
    if (eventHandler != NULL) {
        return eventHandler->specialKeyReleased(k, m, x, y);
    }
    return false;
}

void TweakBarHandler::swap(ptr<TweakBarHandler> t)
{
    std::swap(eventHandler, t->eventHandler);
    std::swap(name, t->name);
    needUpdate = true;
}

}
