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

#include "proland/graph/FileReader.h"

namespace proland
{

FileReader::FileReader(const string &file, bool &isIndexed)
{
    in.open(file.c_str(), ifstream::binary);
    assert(in);
    isBinary = true;
    int i = this->read<int>();
    isBinary = false;
    isIndexed = false;
    if (i == 0 || i == 1) {
        isBinary = true;
        if (i == 1) {
            isIndexed = true;
        }
    } else {
        if ((char) i == '1') {
            isIndexed = true;
        }
    }
    if (!isBinary) {
        seekg(2, ios::beg);
    }
}

FileReader::~FileReader()
{
    in.close();
}

streampos FileReader::tellg()
{
    return in.tellg();
}

void FileReader::seekg(streamoff off, ios_base::seekdir dir)
{
    in.seekg(off, dir);
}

bool FileReader::error()
{
    return !in.good();
}

}
