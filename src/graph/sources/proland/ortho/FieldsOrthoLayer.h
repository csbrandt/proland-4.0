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

#ifndef _PROLAND_FIELDSORTHOLAYER_H_
#define _PROLAND_FIELDSORTHOLAYER_H_

#include "proland/graph/producer/GraphLayer.h"

namespace proland
{

/**
 * An OrthoGPUProducer layer to draw fields.
 * @ingroup ortho
 * @author Antoine Begault, Guillaume Piolat
 */
PROLAND_API class FieldsOrthoLayer : public GraphLayer
{
public:
    /**
     * Creates a new FieldsOrthoLayer.
     *
     * @param graphProducer the GraphProducer that produces the graphs to
     *      be drawn by this layer.
     * @param layerProgram the Program to be used to draw the graphs.
     * @param fillProgram the GLSL Program to be used to fill areas in this Layer.
     * @param displayLevel the quadtree level at which the display of
     *      this layer must start.
     * @param quality enable or not the quality mode (better display).
     */
    FieldsOrthoLayer(ptr<GraphProducer> graphProducer, ptr<Program> layerProgram,
        ptr<Program> fillProgram, int displayLevel = 0, bool quality = true);

    /**
     * Deletes this FieldsOrthoLayer.
     */
    virtual ~FieldsOrthoLayer();

    virtual void setTileSize(int tileSize, int tileBorder, float rootQuadSize);

    virtual bool doCreateTile(int level, int tx, int ty, TileStorage::Slot *data);

protected:
    /**
     * Program used to fill areas.
     */
    ptr<Program> fill;

    /**
     * Creates an uninitialized FieldsOrthoLayer.
     */
    FieldsOrthoLayer();

    /**
     * Initializes this FieldsOrthoLayer. See #FieldsOrthoLayer.
     */
    void init(ptr<GraphProducer> graphProducer, ptr<Program> layerProgram,
        ptr<Program> fillProgram, int displayLevel = 0, bool quality = true);

    virtual void swap(ptr<FieldsOrthoLayer> p);

private:
    /**
     * Fields colors.
     */
    static vec4f COLOR[10];

    /**
     * Fields dColor.
     */
    static mat3f DCOLOR[10];

    /**
     * Fields stripe size.
     */
    static vec3f STRIPES[10];

    /**
     * The mesh used for drawing curves.
     */
    ptr< Mesh<vec2f, unsigned int> > mesh;

    /**
     * The tesselator used for drawing areas.
     */
    ptr<Tesselator> tess;

    /**
     * Returns the color of a given Area, depending on its type.
     *
     * @param field the Area to color.
     * @param[out] dcolor the associated dcolor.
     * @param[out] stripeSize the associated stripeSize.
     * @param[out] stripeDir the associated stripeDir.
     */
    vec4f* getColor(AreaPtr field, mat3f **dcolor, vec3f **stripeSize, vec2f &stripeDir);

    /**
     * Matrix used to compute border color in the shader.
     */
    ptr<Uniform3f> fillOffsetU;

    ptr<Uniform1f> depthU;

    ptr<Uniform4f> fillColorU;

    ptr<Uniform3f> stripeSizeU;

    ptr<Uniform4f> stripeDirU;

    ptr<Uniform4f> colorU;

    ptr<Uniform3f> tileOffsetU;
};

}

#endif
