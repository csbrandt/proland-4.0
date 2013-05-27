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

#include "proland/graph/FileWriter.h"

namespace proland
{

FileWriter::FileWriter(const string &file, bool binary)
{
    if (binary) {
        out.open(file.c_str(), ofstream::binary);
    } else {
        out.open(file.c_str());
    }

    assert(out);
    out.precision(3);
    out.setf(ios_base::fixed, ios_base::floatfield);
    isBinary = binary;
}

FileWriter::~FileWriter()
{
    out.close();
}

streampos FileWriter::tellp()
{
    return out.tellp();
}

void FileWriter::seekp(streamoff off, ios_base::seekdir dir)
{
    out.seekp(off, dir);
}

void FileWriter::width(streamsize wide) {
    out.width(wide);
}

}
