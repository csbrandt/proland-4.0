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

#ifndef _PROLAND_FILEREADER_H_
#define _PROLAND_FILEREADER_H_

#include <fstream>
#include <iostream>

#include "ork/core/Object.h"

using namespace std;

namespace proland
{

/**
 * FileReader handles file inputs for graph loading.
 * Handles binary & ascii.
 * @ingroup graph
 * @author Antoine Begault
 */
PROLAND_API class FileReader
{
public:
    /**
     * Creates a new FileReader.
     *
     * @param file the path/name of the file to read.
     * @param isIndexed after function call, will be true if the magic number was 1. False otherwise.
     */
    FileReader(const string &file, bool &isIndexed);

    /**
     * Deletes this FileReader.
     */
    ~FileReader();

    /**
     * Templated read Method.
     * Example : read<int>().
     *
     * @return a T value read from the input file stream at get pointer.
     */
    template <typename T> T read()
    {
        T t;
        if (isBinary) {
            in.read((char*) &t, sizeof(T));
        } else {
            in >> t;
        }
        return t;
    }

    /**
     * Returns the position of the get pointer.
     */
    streampos tellg();

    /**
     * Sets the position of the get pointer.
     * The get pointer determines the next location to be read in the source
     * associated to the stream.
     *
     * @param off integral value of type streamoff representing the offset
     *     to be applied relative to an absolute position specified in the
     *     dir parameter.
     * @param dir seeking direction. It is an object of type ios_base::seekdir
     *     that specifies an absolute position from where the offset parameter
     *     off is applied. It can take any of the following member constant
     *     values : ios_base::beg | ios_base::cur | ios_base::end.
     */
    void seekg(streamoff off, ios_base::seekdir dir);

    /**
     * Returns true if an error occured while reading. The errors corresponds
     * to ifstream errors.
     */
    bool error();

private:
    /**
     * The input filestream.
     */
    ifstream in;

    /**
     * If true, file will be read as binary. Otherwise, ASCII.
     */
    bool isBinary;
};

}

#endif
