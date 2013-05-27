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

#ifndef _PROLAND_FILEWRITER_H_
#define _PROLAND_FILEWRITER_H_

#include <fstream>
#include <iostream>

#include "ork/core/Object.h"

using namespace std;

namespace proland
{

/**
 * FileWriter handles file outputs for graph saving.
 * Handles binary & ascii.
 * @ingroup graph
 * @author Antoine Begault
 */
PROLAND_API class FileWriter
{
public:
    /**
     * Creates a new FileWriter.
     *
     * @param file the path/name of the file to write into.
     * @param binary If true, will write in binary mode. Otherwise, in ASCII.
     */
    FileWriter(const string &file, bool binary = true);

    /**
     * Deletes this FileWriter.
     */
    ~FileWriter();

    /**
     * Write method. Allows to write all sorts of data in the file.
     * @param t the data to write.
     */
    template <typename T> void write(T t)
    {
        if (isBinary) {
            out.write((char*) &t, sizeof(T));
        } else {
            out << t << ' ';
        }
    }

    /**
     * Writes the magic Number into the file. Magic Number determines
     * if the file is indexed or not.
     *
     * @param i the magic number. If 0 -> basic saving. If 1, indexed saving.
     */
    void magicNumber(int i)
    {
        out.write((char *)&i,sizeof(int));
    }

    /**
     * Returns the position of the put pointer.
     */
    streampos tellp();

    /**
     * Sets the position of the put pointer.
     *
     * @param off integral value of type streamoff representing the offset
     *      to be applied relative to an absolute position specified in the
     *      dir parameter.
     * @param dir seeking direction. It is an object of type ios_base::seekdir
     *      that specifies an absolute position from where the offset parameter
     *      off is applied. It can take any of the following member constant
     *      values: ios_base::beg | ios_base::cur | ios_base::end.
     */
    void seekp(streamoff off, ios_base::seekdir dir);

    /**
     * Sets a new field width for the output stream.
     */
    void width(streamsize wide) ;

private:
    /**
     * Output filestream.
     */
    ofstream out;

    /**
     * If true, the writer is in binary mode.
     */
    bool isBinary;
};

}

#endif
