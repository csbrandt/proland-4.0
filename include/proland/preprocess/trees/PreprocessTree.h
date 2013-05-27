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

#ifndef _PROLAND_PREPROCESS_TREE_
#define _PROLAND_PREPROCESS_TREE_

#include <vector>

#include "ork/render/Mesh.h"
#include "ork/render/Texture2D.h"
#include "ork/render/Texture2DArray.h"

using namespace std;
using namespace ork;

namespace proland
{

/**
 * A mesh with a texture, to describe a (part of) tree model.
 *
 * @ingroup preprocess
 * @author Eric Bruneton
 */
PROLAND_API class TreeMesh
{
public:
    struct Vertex
    {
        vec3f pos;

        vec2f uv;
    };

    ptr< Mesh<Vertex, unsigned int> > mesh;

    ptr<Texture2D> texture;

    TreeMesh(ptr< Mesh<Vertex, unsigned int> > mesh, ptr<Texture2D> texture);
};

/**
 * TODO.
 */
typedef void (*loadTreeMeshFunction)(vector<TreeMesh> &tree);

/**
 * TODO.
 */
typedef ptr<Texture2DArray> (*loadTreeViewsFunction)();

/**
 * Precomputes the textures for the given tree model.
 *
 * @param loadTree function to load the 3D tree model (whose bounding box
 *      must be -1:1 x -1:1 x -1:1).
 * @param output the folder where to write the generated textures.
 */
PROLAND_API void preprocessTree(loadTreeMeshFunction loadTree, int n, int w, const char *output);

/**
 * TODO.
 */
PROLAND_API void preprocessTreeTables(float minRadius, float maxRadius, float treeHeight, float treeTau, int nViews, loadTreeViewsFunction loadTree, const char *output);

/**
 * TODO.
 */
PROLAND_API void mergeTreeTables(const char* input1, const char* input2, const char *output);

/**
 * TODO.
 */
PROLAND_API void preprocessMultisample(const char *output);

}

#endif
