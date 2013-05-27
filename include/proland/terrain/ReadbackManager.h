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

#ifndef _PROLAND_READBACK_MANAGER_H_
#define _PROLAND_READBACK_MANAGER_H_

#include "ork/render/FrameBuffer.h"

using namespace ork;

namespace proland
{

/**
 * A manager for asynchronous readbacks from a framebuffer. Asynchronous means
 * that readbacks are non blocking: a read operation returns immediately
 * with an empty result, and the actual result is passed via a callback
 * function when it becomes available (in practice n frames after the read was
 * started, where n is user defined).
 * @ingroup terrain
 * @authors Eric Bruneton, Antoine Begault
*/
PROLAND_API class ReadbackManager : public Object
{
public:
    /**
     * A callback function called when a readback is done; see ReadbackManager.
     */
    class Callback : public Object
    {
    public:
        /**
         * Creates a new callback object.
         */
        Callback() : Object("ReadbackManager::Callback")
        {
        }

        /**
         * Called when a readback is finished.
         *
         * @param data the data that has been read.
         */
        virtual void dataRead(volatile void *data) = 0;
    };

    /**
     * Creates a new readback manager.
     *
     * @param maxReadbackPerFrame maximum number of readbacks that can be
     * started per frame.
     * @param readbackDelay number of frames between the start of a readback
     * and its end.
     * @param bufferSize maximum number of bytes per readback.
     */
    ReadbackManager(int maxReadbackPerFrame, int readbackDelay, int bufferSize);

    /**
     * Destroys this readback manager.
     */
    virtual ~ReadbackManager();

    /**
     * Returns true if a new readback can be started for the current frame.
     */
    bool canReadback();

    /**
     * Starts a new readback and returns immediately. Returns true if the
     * readback has effectively started (see #canReadback()).
     *
     * @param fb the framebuffer from which the data must be read. Data will
     * be read from the last buffer specified with FrameBuffer#setReadBuffer
     * for this framebuffer.
     * @param x x coordinate of the lower left corner of the region to be read.
     * @param y x coordinate of the lower left corner of the region to be read.
     * @param w width of the region to be read.
     * @param h height the region to be read.
     * @param f the components to be read.
     * @param t the type to be used to store the read data.
     * @param cb the function to be called when the readback is finished.
     */
    bool readback(ptr<FrameBuffer> fb, int x, int y, int w, int h, TextureFormat f, PixelType t, ptr<Callback> cb);

    /**
     * Informs this manager that a new frame has started.
     */
    void newFrame();

private:
    int maxReadbackPerFrame;
    int readbackDelay;
    int *readCount;
    ptr<GPUBuffer> **toRead;
    ptr<Callback> **toReadCallbacks;
    int bufferSize;
};

}

#endif
