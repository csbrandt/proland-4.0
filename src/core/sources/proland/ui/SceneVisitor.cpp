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

#include "proland/ui/SceneVisitor.h"

#include "proland/terrain/TileSampler.h"

using namespace std;

namespace proland
{

SceneVisitor::SceneVisitor() : Object("SceneVisitor")
{
}

SceneVisitor::~SceneVisitor()
{
}

ptr<SceneVisitor> SceneVisitor::visitNode(ptr<SceneNode> node)
{
    return this;
}

ptr<SceneVisitor> SceneVisitor::visitNodeValue(ptr<Value> value)
{
    return this;
}

ptr<SceneVisitor> SceneVisitor::visitProducer(ptr<TileProducer> producer)
{
    return this;
}

ptr<SceneVisitor> SceneVisitor::visitLayer(ptr<TileLayer> producer)
{
    return this;
}

ptr<SceneVisitor> SceneVisitor::visitNodeField(string &name, ptr<Object> field)
{
    return this;
}

ptr<SceneVisitor> SceneVisitor::visitNodeMethod(string &name, ptr<Method> method)
{
    return this;
}

ptr<SceneVisitor> SceneVisitor::visitCache(ptr<TileCache> cache)
{
    return this;
}

void SceneVisitor::visitEnd()
{
}

void SceneVisitor::accept(ptr<SceneNode> root)
{
    set< ptr<TileCache> > caches;
    accept(root, caches);
    set< ptr<TileCache> >::iterator i = caches.begin();
    while (i != caches.end()) {
        visitCache(*i);
        i++;
    }
}

void SceneVisitor::accept(ptr<SceneNode> n, set< ptr<TileCache> > &caches)
{
    ptr<SceneVisitor> v = visitNode(n);
    SceneNode::ValueIterator u = n->getValues();
    while (u.hasNext()) {
        ptr<Value> value = u.next();
        v->visitNodeValue(value);
    }
    SceneNode::FieldIterator f = n->getFields();
    while (f.hasNext()) {
        string name;
        ptr<Object> field = f.next(name);
        ptr<SceneVisitor> w = v->visitNodeField(name, field);
        if (w != NULL) {
            ptr<TileSampler> ts = field.cast<TileSampler>();
            if (ts != NULL) {
                w->accept(ts->get(), caches);
            }
            w->visitEnd();
        }
    }
    SceneNode::MethodIterator m = n->getMethods();
    while (m.hasNext()) {
        string name;
        ptr<Method> method = m.next(name);
        v->visitNodeMethod(name, method);
    }
    for (unsigned int i = 0; i < n->getChildrenCount(); ++i) {
        v->accept(n->getChild(i), caches);
    }
    v->visitEnd();
}

void SceneVisitor::accept(ptr<TileProducer> p, set< ptr<TileCache> > &caches)
{
    caches.insert(p->getCache());
    ptr<SceneVisitor> v = visitProducer(p);
    if (v != NULL) {
        for (int i = 0; i < p->getLayerCount(); ++i) {
            v->accept(p->getLayer(i), caches);
        }
        vector< ptr<TileProducer> > producers;
        p->getReferencedProducers(producers);
        for (unsigned int i = 0; i < producers.size(); ++i) {
            v->accept(producers[i], caches);
        }
        v->visitEnd();
    }
}

void SceneVisitor::accept(ptr<TileLayer> l, set< ptr<TileCache> > &caches)
{
    ptr<SceneVisitor> v = visitLayer(l);
    if (v != NULL) {
        vector< ptr<TileProducer> > producers;
        l->getReferencedProducers(producers);
        for (unsigned int i = 0; i < producers.size(); ++i) {
            v->accept(producers[i], caches);
        }
        v->visitEnd();
    }
}

}
