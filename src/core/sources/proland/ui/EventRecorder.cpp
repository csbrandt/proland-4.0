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

#include "proland/ui/EventRecorder.h"

#include <fstream>

#include "ork/resource/ResourceTemplate.h"
#include "ork/render/FrameBuffer.h"

#include "proland/terrain/TerrainNode.h"

using namespace ork;

namespace proland
{

EventRecorder::Event::Event()
{
}

EventRecorder::Event::Event(double t, double dt, float groundHeight) : kind(DISPLAY)
{
    display.t = t;
    display.dt = dt;
    display.groundHeight = groundHeight;
}

EventRecorder::Event::Event(EventType kind, int m, int arg1, int arg2, int arg3, int arg4) : kind(kind)
{
    e.m = m;
    e.arg1 = arg1;
    e.arg2 = arg2;
    e.arg3 = arg3;
    e.arg4 = arg4;
}

EventRecorder::EventRecorder() : EventHandler("EventRecorder")
{
}

EventRecorder::EventRecorder(const char *eventFile, bool autoSave, const char *frames, ptr<Program> cursorProg, ptr<Texture2D> cursor, ptr<EventHandler> next) :
    EventHandler("EventRecorder")
{
    init(eventFile, autoSave, frames, cursorProg, cursor, next);
}

void EventRecorder::init(const char *eventFile, bool autoSave, const char *frames, ptr<Program> cursorProg, ptr<Texture2D> cursor, ptr<EventHandler> next)
{
    this->r = NULL;
    this->eventFile = eventFile;
    this->autoSave = autoSave;
    this->frames = frames;
    this->cursW = cursor->getWidth();
    this->cursH = cursor->getHeight();
    this->cursor = cursor;
    this->isRecording = false;
    this->isPlaying = false;
    this->next = next;
    this->cursorProg = cursorProg;

    cursorSamplerU = cursorProg->getUniformSampler("sourceSampler");
    rescaleU = cursorProg->getUniform4f("rescale");
    correctU = cursorProg->getUniform3f("correct");
}

EventRecorder::~EventRecorder()
{
//    delete cursor;
}

void EventRecorder::setEventFile(const char *eventFile)
{
    this->eventFile = eventFile;
}

void EventRecorder::redisplay(double t, double dt)
{
     if (isRecording) {
        recordedEvents.push_back(Event(t, dt, TerrainNode::nextGroundHeightAtCamera));
    } else if (isPlaying) {
        ostringstream s;
        bool replay = true;
        isPlaying = false;
        while (replay && lastPlayed < recordedEvents.size()) {
            Event e = recordedEvents[lastPlayed++];
            switch (e.kind) {
            case Event::DISPLAY:
                t = e.display.t;
                dt = e.display.dt;
                TerrainNode::nextGroundHeightAtCamera = e.display.groundHeight;
                replay = false;
                break;
            case Event::MOUSE:
                mouseClick((button) e.e.arg1, (state) e.e.arg2, (modifier) e.e.m, e.e.arg3, e.e.arg4);
                savedX = e.e.arg3;
                savedY = e.e.arg4;
                break;
            case Event::MOTION:
                mouseMotion(e.e.arg1, e.e.arg2);
                savedX = e.e.arg1;
                savedY = e.e.arg2;
                break;
            case Event::PASSIVEMOTION:
                mousePassiveMotion(e.e.arg1, e.e.arg2);
                savedX = e.e.arg1;
                savedY = e.e.arg2;
                break;
            case Event::WHEEL:
                mouseWheel((wheel) e.e.arg1, (modifier) e.e.m, e.e.arg2, e.e.arg3);
                savedX = e.e.arg2;
                savedY = e.e.arg3;
                break;
            case Event::KEYBOARD:
                if (e.e.arg4 == 0) {
                    keyTyped(e.e.arg1, (modifier) e.e.m, e.e.arg2, e.e.arg3);
                    if (e.e.arg1 == 27) {
                        ::exit(0);
                    }
                } else {
                    keyReleased(e.e.arg1, (modifier) e.e.m, e.e.arg2, e.e.arg3);
                }
                break;
            case Event::SPECIAL:
                if (e.e.arg4 == 0) {
                    specialKey((key) e.e.arg1, (modifier) e.e.m, e.e.arg2, e.e.arg3);
                } else {
                    specialKeyReleased((key) e.e.arg1, (modifier) e.e.m, e.e.arg2, e.e.arg3);
                }
                break;
            }
        }
        isPlaying = lastPlayed < recordedEvents.size();
    }

    if (next != NULL) {
        next->redisplay(t, dt);
    }

    if (isPlaying) {
        ptr<FrameBuffer> fb = FrameBuffer::getDefault();
        vec4<GLint> vp = fb->getViewport();

        fb->setBlend(true, ADD, SRC_ALPHA, ONE_MINUS_SRC_ALPHA, ADD, ZERO, ONE);
        fb->setColorMask(true, true, true, false);
        fb->setDepthMask(false);
        fb->setStencilMask(0, 0);

        correctU->set(vec3f(0.f, 0.f, 1.f));
        rescaleU->set(vec4f(2.0f * (float)(savedX + 0.5f * cursW) / vp.z - 1.0f, 2.0f * (float)(vp.w - savedY - 0.5f * cursH) / vp.w - 1.0f, cursW / (float)vp.z, cursH / (float)vp.w));

        cursorSamplerU->set(cursor);
        fb->drawQuad(cursorProg);

        fb->setBlend(false);
        fb->setColorMask(true, true, true, true);
        fb->setDepthMask(true);
        fb->setStencilMask(0xFFFFFFFF,0xFFFFFFFF);
    }

    if (isPlaying && saveVideo) {
        int frameCount = 0;
        if (lastSavedEvent >= 0) {
            double prevTime = recordedEvents[lastSavedEvent].display.t;
            double curTime = recordedEvents[lastPlayed - 1].display.t;
            assert(recordedEvents[lastSavedEvent].kind == Event::DISPLAY && prevTime > 0.0);
            assert(recordedEvents[lastPlayed - 1].kind == Event::DISPLAY && curTime > 0.0);
            frameCount = int(floor(curTime / 40000.0) - floor(prevTime / 40000.0));
        }
        // if the delay between two recorded frames is more than 1/25 of a second
        // the first one must be saved several times in the video to keep a video
        // framerate of 25 frames per second.
        for (int i = 0; i < frameCount; ++i) {
            char name[256];
            sprintf(name, frames, lastSavedFrame++);
            saveFrame(name);
        }
        lastSavedEvent = lastPlayed - 1;
    }
}

void EventRecorder::reshape(int x, int y)
{
    if (next != NULL) {
        next->reshape(x, y);
    }
}

void EventRecorder::idle(bool damaged)
{
    if (next != NULL) {
        next->idle(damaged);
    }
}

bool EventRecorder::mouseClick(button b, state s, modifier m, int x, int y)
{
    if (isRecording) {
        recordedEvents.push_back(Event(Event::MOUSE, (int) m, (int) b, (int) s, x, y));
        saveEvents();
    } else if (isPlaying) {
        return true;
    }
    return next != NULL && next->mouseClick(b, s, m, x, y);
}

bool EventRecorder::mouseMotion(int x, int y)
{
    if (isRecording) {
        recordedEvents.push_back(Event(Event::MOTION, 0, x, y));
    } else if (isPlaying) {
        return true;
    }
    return next != NULL && next->mouseMotion(x, y);
}

bool EventRecorder::mousePassiveMotion(int x, int y)
{
    if (isRecording) {
        recordedEvents.push_back(Event(Event::PASSIVEMOTION, 0, x, y));
    } else if (isPlaying) {
        return true;
    }
    return next != NULL && next->mousePassiveMotion(x, y);
}

bool EventRecorder::mouseWheel(wheel b, modifier m, int x, int y)
{
    if (isRecording) {
        recordedEvents.push_back(Event(Event::WHEEL, (int) m, (int) b, x, y));
    } else if (isPlaying) {
        return true;
    }
    return next != NULL && next->mouseWheel(b, m, x, y);
}

bool EventRecorder::keyTyped(unsigned char c, modifier m, int x, int y)
{
    if (c == 'p') {
        isPlaying = ! isPlaying;
        return true;
    }

    if (isRecording) {
        recordedEvents.push_back(Event(Event::KEYBOARD, (int) m, c, x, y, 0));
        saveEvents();
        if (c == 27) {
            specialKey(KEY_F12, m, x, y);
        }
    } else if (isPlaying) {
        if (c == 27) {
            ::exit(0);
        }
        return true;
    }
    return next != NULL && next->keyTyped(c, m, x, y);
}

bool EventRecorder::keyReleased(unsigned char c, modifier m, int x, int y)
{
    if (isRecording) {
        recordedEvents.push_back(Event(Event::KEYBOARD, (int) m, c, x, y, 1));
    } else if (isPlaying) {
        return true;
    }
    return next != NULL && next->keyReleased(c, m, x, y);
}

bool EventRecorder::specialKey(key k, modifier m, int x, int y)
{
    if (isRecording) {
        if (k == KEY_F12) {

            char stime[256];
            Timer::getDateTimeString(stime, 256);

            char name[256];
            sprintf(name, "record.%s.dat", stime);
            ofstream out(name, ofstream::binary);
            int n = recordedEvents.size();
            out.write((char*) &n, sizeof(int));
            for (int i = 0; i < n; ++i) {
                Event e = recordedEvents[i];
                out.write((char*) &e, sizeof(Event));
            }
            out.close();
            isRecording = false;
            return true;
        } else {
            recordedEvents.push_back(Event(Event::SPECIAL, (int) m, (int) k, x, y, 0));
            saveEvents();
        }
    } else if (isPlaying) {
        return true;
    }
    if (k == KEY_F11) {
        if (recordedEvents.empty() && eventFile != NULL) {
            ifstream in(eventFile, ifstream::binary);
            int n;
            in.read((char*) &n, sizeof(int));
            for (int i = 0; i < n; ++i) {
                Event e;
                in.read((char*) &e, sizeof(Event));
                recordedEvents.push_back(e);
            }
            in.close();
            getRecorded()->saveState();
        }
        if (!recordedEvents.empty()) {
            getRecorded()->restoreState();
            lastPlayed = 0;
            savedX = -cursW - 1;
            savedY = -cursH - 1;
            isPlaying = true;
            saveVideo = (m & SHIFT) != 0;
            lastSavedEvent = -1;
            lastSavedFrame = 0;
            if ((m & CTRL) != 0 && (m & SHIFT) == 0) {
                // quit after replay
                recordedEvents.push_back(Event(0.0, 0.0, 0.0));
                recordedEvents.push_back(Event(Event::KEYBOARD, 0, 27, 0, 0, 0));
            }
        }
        return true;
    }
    if (k == KEY_F12) {
        getRecorded()->saveState();
        recordedEvents.clear();
        isRecording = true;
        return true;
    }
    return next != NULL && next->specialKey(k, m, x, y);
}

bool EventRecorder::specialKeyReleased(key k, modifier m, int x, int y)
{
    if (isRecording) {
        recordedEvents.push_back(Event(Event::SPECIAL, (int) m, (int) k, x, y, 1));
    } else if (isPlaying) {
        return true;
    }
    return next != NULL && next->specialKeyReleased(k, m, x, y);
}

void EventRecorder::saveFrame(char *tga)
{
    ptr<FrameBuffer> fb = FrameBuffer::getDefault();

    vec4<GLint> vp = fb->getViewport();
    int w = vp.z;
    int h = vp.w;
    char *buf = new char[w * h * 3];

    fb->readPixels(0, 0, w, h, BGR, UNSIGNED_BYTE, Buffer::Parameters(), CPUBuffer(buf));
    FILE *g = fopen(tga, "wb");
    for (int i = 0; i < 12; ++i) {
        fputc(i == 2 ? 2 : 0, g);
    }
    fputc(w % 256, g);
    fputc(w / 256, g);
    fputc(h % 256, g);
    fputc(h / 256, g);
    fputc(24, g);
    fputc(0, g);
    fwrite(buf, w * h * 3, 1, g);
    fclose(g);

    delete[] buf;
}

void EventRecorder::saveEvents()
{
    if (autoSave) {
        ofstream out("events.dat", ofstream::binary);
        int n = recordedEvents.size();
        out.write((char*) &n, sizeof(int));
        for (int i = 0; i < n; ++i) {
            Event e = recordedEvents[i];
            out.write((char*) &e, sizeof(Event));
        }
        out.close();
    }
}

void EventRecorder::swap(ptr<EventRecorder> o)
{
    std::swap(r, o->r);
    std::swap(frames, o->frames);
    std::swap(cursW, o->cursW);
    std::swap(cursH, o->cursH);
    std::swap(cursorProg, o->cursorProg);
    std::swap(cursor, o->cursor);
    std::swap(isRecording, o->isRecording);
    std::swap(isPlaying, o->isPlaying);
    std::swap(saveVideo, o->saveVideo);
    std::swap(lastSavedEvent, o->lastSavedEvent);
    std::swap(lastSavedFrame, o->lastSavedFrame);
    std::swap(savedX, o->savedX);
    std::swap(savedY, o->savedY);
    std::swap(lastPlayed, o->lastPlayed);
    std::swap(recordedEvents, o->recordedEvents);
    std::swap(next, o->next);
}

class EventRecorderResource : public ResourceTemplate<100, EventRecorder>
{
public:
    string recorded;

    EventRecorderResource(ptr<ResourceManager> manager, const string &name, ptr<ResourceDescriptor> desc, const TiXmlElement *e = NULL) :
        ResourceTemplate<100, EventRecorder> (manager, name, desc)
    {
        e = e == NULL ? desc->descriptor : e;
        checkParameters(desc, e, "name,recorded,eventFile,autoSave,videoDirectory,cursorTexture,next,");

        recorded = getParameter(desc, e, "recorded");
        const char *eventFile = e->Attribute("eventFile");
        bool autoSave = e->Attribute("autoSave") != NULL && strcmp(e->Attribute("autoSave"), "true") == 0;
        const char *frames = e->Attribute("videoDirectory");
        ptr<Program> prog = manager->loadResource("copyShader;").cast<Program>();
        ptr<Texture2D> cursor = manager->loadResource(getParameter(desc, e, "cursorTexture")).cast<Texture2D>();
        ptr<EventHandler> next = manager->loadResource(getParameter(desc, e, "next")).cast<EventHandler>();

        init(eventFile, autoSave, frames, prog, cursor, next);
    }

    virtual Recordable *getRecorded()
    {
        if (r == NULL) {
            r = dynamic_cast<Recordable*>(manager->loadResource(recorded).get());
        }
        return r;
    }
};

extern const char eventRecorder[] = "eventRecorder";

static ResourceFactory::Type<eventRecorder, EventRecorderResource> EventRecorderType;

}
