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

#ifndef _PROLAND_SCENEVISITOR_H_
#define _PROLAND_SCENEVISITOR_H_

#include "ork/render/Uniform.h"
#include "ork/scenegraph/SceneNode.h"
#include "proland/producer/TileProducer.h"

using namespace ork;

namespace proland
{

/**
 * @defgroup proland_ui ui
 * Provides classes to build a user interface to control and %edit a scene.
 * @ingroup proland
 */

/**
 * A visitor to visit a scene graph.
 * @ingroup proland_ui
 * @authors Eric Bruneton, Antoine Begault
 */
PROLAND_API class SceneVisitor : public Object
{
public:
    /**
     * Creates a new SceneVisitor.
     */
    SceneVisitor();

    /**
     * Deletes this SceneVisitor.
     */
    virtual ~SceneVisitor();

    /**
     * Visits a SceneNode.
     * The default implementation of this method does nothing.
     *
     * @param node the SceneNode to visit.
     */
    virtual ptr<SceneVisitor> visitNode(ptr<SceneNode> node);

    /**
     * Visits a SceneNode.
     * The default implementation of this method does nothing.
     *
     * @param node the SceneNode to visit.
     */
    virtual ptr<SceneVisitor> visitNodeValue(ptr<Value> value);

    /**
     * Visits a TileProducer.
     * The default implementation of this method does nothing.
     *
     * @param producer the TileProducer to visit.
     */
    virtual ptr<SceneVisitor> visitProducer(ptr<TileProducer> producer);

    /**
     * Visits a TileLayer.
     * The default implementation of this method does nothing.
     *
     * @param layer the TileLayer to visit.
     */
    virtual ptr<SceneVisitor> visitLayer(ptr<TileLayer> layer);

    /**
     * Visits a SceneNode field.
     * The default implementation of this method does nothing.
     *
     * @param name the field's name.
     * @param field the field's value.
     */
    virtual ptr<SceneVisitor> visitNodeField(string &name, ptr<Object> field);

    /**
     * Visits a Method.
     * The default implementation of this method does nothing.
     *
     * @param name the Method's name.
     * @param method the Method to visit.
     */
    virtual ptr<SceneVisitor> visitNodeMethod(string &name, ptr<Method> method);

    /**
     * Visits a TileCache.
     * The default implementation of this method does nothing.
     *
     * @param cache the TileCache to visit.
     */
    virtual ptr<SceneVisitor> visitCache(ptr<TileCache> cache);

    /**
     * Ends the visits of a SceneNode, a Uniform, a TileProducer or a TileLayer.
     * The default implementation of this method does nothing.
     */
    virtual void visitEnd();

    /**
     * Makes this visitor visit the given scene graph. This method calls the
     * visitXxx methods for each scene node, uniform, scene node field, scene
     * node method, tile producer, tile layer, and tile cache encountered
     * during the recursive exploration of the given scene graph.
     *
     * @param root the scene graph to visit.
     */
    void accept(ptr<SceneNode> root);

private:
    /**
     * Makes this visitor visit the given scene node, and all its subnodes.
     *
     * @param n the SceneNode to visit.
     * @param[in,out] caches the TileCache found during this visit.
     */
    void accept(ptr<SceneNode> n, set< ptr<TileCache> > &caches);

    /**
     * Makes this visitor visit the given tile producer, and all its
     * referenced producers, recursively.
     *
     * @param p the TileProducer to visit.
     * @param[in,out] caches the TileCache found during this visit.
     */
    void accept(ptr<TileProducer> p, set< ptr<TileCache> > &caches);

    /**
     * Makes this visitor visit the given tile layer, and all its
     * referenced producers, recursively.
     *
     * @param l the TileLayer to visit.
     * @param[in,out] caches the TileCache found during this visit.
     */
    void accept(ptr<TileLayer> l, set< ptr<TileCache> > &caches);
};

}

#endif
