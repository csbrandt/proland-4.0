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

#include "proland/graph/producer/Tesselator.h"

#include <GL/glu.h>

#ifdef _WIN32
#define CALLBACK __stdcall
typedef void (CALLBACK *TessCallBack)();
#else
#define CALLBACK
typedef void (*TessCallBack)();
#endif

#ifdef _LP64
typedef long _int;
#else
typedef int _int;
#endif

extern "C"
{

int mode;
int prevprev;
int previous;

void CALLBACK errorCallback(GLenum code, ork::Mesh<vec2f, unsigned int> *m)
{
    assert(false);
}

void CALLBACK beginCallback(GLenum which, ork::Mesh<vec2f, unsigned int> *m)
{
    mode = which;
    prevprev = -1;
    previous = -1;
}

void CALLBACK vertexCallback(GLvoid *vertex, ork::Mesh<vec2f, unsigned int> *m)
{
    switch (mode) {
        case GL_TRIANGLES:
            m->addIndice((_int) vertex);
            break;
        case GL_TRIANGLE_STRIP:
            if (prevprev == -1) {
                prevprev = (_int) vertex;
            } else if (previous == -1) {
                previous = (_int) vertex;
            } else {
                m->addIndice(prevprev);
                m->addIndice(previous);
                m->addIndice((_int) vertex);
                prevprev = previous;
                previous = (_int) vertex;
            }
            break;
        case GL_TRIANGLE_FAN:
            if (prevprev == -1) {
                prevprev = (_int) vertex;
            } else if (previous == -1) {
                previous = (_int) vertex;
            } else {
                m->addIndice(prevprev);
                m->addIndice(previous);
                m->addIndice((_int) vertex);
                previous = (_int) vertex;
            }
            break;
    }
}

void CALLBACK combineCallback(GLdouble coords[3], GLvoid *vertex_data[4], GLfloat weight[4], GLvoid **outData, ork::Mesh<vec2f, unsigned int> *m)
{
    int v = m->getVertexCount();
    m->addVertex(vec2f((float) coords[0], (float) coords[1]));
    *outData = (GLvoid*) v;
}

}

namespace proland
{

Tesselator::Tesselator() : Object("Tesselator")
{
    tess = gluNewTess();
    gluTessCallback((GLUtesselator*) tess, GLU_TESS_ERROR_DATA, (TessCallBack) errorCallback);
    gluTessCallback((GLUtesselator*) tess, GLU_TESS_BEGIN_DATA, (TessCallBack) beginCallback);
    gluTessCallback((GLUtesselator*) tess, GLU_TESS_VERTEX_DATA, (TessCallBack) vertexCallback);
    gluTessCallback((GLUtesselator*) tess, GLU_TESS_COMBINE_DATA, (TessCallBack) combineCallback);
}

Tesselator::~Tesselator()
{
    gluDeleteTess((GLUtesselator*) tess);
}

void Tesselator::beginPolygon(ptr< Mesh<vec2f, unsigned int> > mesh)
{
    this->mesh = mesh;
    gluTessBeginPolygon((GLUtesselator*) tess, mesh.get());
}

void Tesselator::beginContour()
{
    gluTessBeginContour((GLUtesselator*) tess);
}

void Tesselator::newVertex(float x, float y)
{
    GLdouble coords[3];
    coords[0] = x;
    coords[1] = y;
    coords[2] = 0;
    int v = mesh->getVertexCount();
    mesh->addVertex(vec2f(x, y));
    gluTessVertex((GLUtesselator*) tess, coords, (GLvoid*) v);
}

void Tesselator::endContour()
{
    gluTessEndContour((GLUtesselator*) tess);
}

void Tesselator::endPolygon()
{
    gluTessEndPolygon((GLUtesselator*) tess);
    this->mesh = NULL;
}

}
