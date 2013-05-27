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

#ifndef _PROLAND_DRAW_TERRAIN_TASK_H_
#define _PROLAND_DRAW_TERRAIN_TASK_H_

#include "ork/scenegraph/AbstractTask.h"
#include "proland/terrain/TerrainNode.h"
#include "proland/terrain/TileSampler.h"

using namespace ork;

namespace proland
{

/**
 * An AbstractTask to draw a %terrain. This task draws a mesh for each visible
 * leaf quad of the %terrain quadtree, using the current program and framebuffers.
 * Before drawing each quad, this task calls TileSampler::setTile on each
 * TileSampler associated with the TerrainNode in its owner SceneNode. It
 * also calls the Deformation#setUniforms methods using the %terrain deformation.
 * @ingroup terrain
 * @authors Eric Bruneton, Antoine Begault, Guillaume Piolat
 */
PROLAND_API class DrawTerrainTask : public AbstractTask
{
public:
    /**
     * Creates a new DrawTerrainTask.
     *
     * @param terrain the %terrain to be drawn. The first part of this "node.name"
     *      qualified name specifies the scene node containing the TerrainNode
     *      field. The second part specifies the name of this TerrainNode field.
     * @param mesh the mesh to be used to draw each quad. The first part of this
     *      "node.name" qualified name specifies the scene node containing the mesh
     *      field. The second part specifies the name of this mesh field.
     * @param culling true to draw only visible leaf quads, false to draw all
     *      leaf quads.
     */
    DrawTerrainTask(const QualifiedName &terrain, const QualifiedName &mesh, bool culling);

    /**
     * Deletes this DrawTerrainTask.
     */
    virtual ~DrawTerrainTask();

    virtual ptr<Task> getTask(ptr<Object> context);

protected:
    /**
     * Creates an uninitialized DrawTerrainTask.
     */
    DrawTerrainTask();

    /**
     * Initializes this DrawTerrainTask.
     *
     * @param terrain the %terrain to be drawn. The first part of this "node.name"
     *      qualified name specifies the scene node containing the TerrainNode
     *      field. The second part specifies the name of this TerrainNode field.
     * @param mesh the mesh to be used to draw each quad. The first part of this
     *      "node.name" qualified name specifies the scene node containing the mesh
     *      field. The second part specifies the name of this mesh field.
     * @param culling true to draw only visible leaf quads, false to draw all
     *      leaf quads.
     */
    void init(const QualifiedName &terrain, const QualifiedName &mesh, bool culling);

    void swap(ptr<DrawTerrainTask> t);

private:
    /**
     * The %terrain to be drawn. The first part of this "node.name"
     * qualified name specifies the scene node containing the TerrainNode
     * field. The second part specifies the name of this TerrainNode field.
     */
    QualifiedName terrain;

    /**
     * The mesh to be drawn for each %terrain quad. The first part of this
     * "node.name" qualified name specifies the scene node containing the mesh
     * field. The second part specifies the name of this mesh field.
     */
    QualifiedName mesh;

    /**
     * True to draw only visible leaf quads, false to draw all leaf quads.
     */
    bool culling;

    /**
     * A Task to draw a %terrain.
     */
    class Impl : public Task
    {
    public:
        /**
         * The SceneNode describing the %terrain position and its associated
         * data (via TileSampler fields).
         */
        ptr<SceneNode> n;

        /**
         * The TerrainNode describing the %terrain and its quadtree.
         */
        ptr<TerrainNode> t;

        /**
         * The mesh to be drawn for each leaf quad.
         */
        ptr<MeshBuffers> m;

        /**
         * True to draw only visible leaf quads, false to draw all leaf quads.
         */
        bool culling;

        /**
         * True if one the TileSampler associated with this terrain
         * uses the asynchronous mode.
         */
        bool async;

        /**
         * Number of primitives (triangles, lines, etc) per *quarter* of
         * the grid mesh. Used to draw only parts of the mesh to fill holes
         * when using asynchronous mode.
         */
        int gridSize;

        /**
         * Creates a new Impl.
         *
         * @param n the SceneNode describing the %terrain position.
         * @param t the TerrainNode describing the %terrain and its quadtree.
         * @param m the mesh to be drawn for each leaf quad.
         * @param culling true to draw only visible leaf quads.
         */
        Impl(ptr<SceneNode> n, ptr<TerrainNode> t, ptr<MeshBuffers> m, bool culling);

        /**
         * Deletes this Impl.
         */
        virtual ~Impl();

        virtual bool run();

        /**
         * Finds the quads whose associated tiles are ready (this may not be
         * the case of all quads if async is true).
         *
         * @param q the %terrain quadtree to be drawn.
         * @param uniforms the TileSampler associated with the %terrain.
         */
        void findDrawableQuads(ptr<TerrainQuad> q, const std::vector< ptr<TileSampler> > &uniforms);

        /**
         * Draw the mesh #m for the leaf quads of the given quadtree. Before drawing each
         * quad, this method calls Deformation#setUniforms for this quad, and calls
         * TileSampler#setTile on each TileSampler the given uniforms vector.
         *
         * @param q the %terrain quadtree to be drawn.
         * @param uniforms the TileSampler associated with the %terrain.
         */
        void drawQuad(ptr<TerrainQuad> q, const std::vector< ptr<TileSampler> > &uniforms);
    };
};

}

#endif
