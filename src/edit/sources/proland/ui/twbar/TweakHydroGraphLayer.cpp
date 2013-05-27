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

#include "proland/ui/twbar/TweakHydroGraphLayer.h"

#include "proland/rivers/graph/HydroCurve.h"

namespace proland
{

static TW_CALL void SetCurvePotentialCallback(const void *value, void *clientData)
{
    EditGraphOrthoLayer::SelectionData *e = (EditGraphOrthoLayer::SelectionData*)clientData;
    dynamic_cast<HydroCurve*>(e->c)->setPotential(*(float*)value);
    e->editor->updateSelectedCurve();
}

static TW_CALL void GetCurvePotentialCallback(void *value, void *clientData)
{
    ptr<HydroCurve> c = dynamic_cast<HydroCurve*>((*(EditGraphOrthoLayer::SelectionData*) clientData).c);
    if (c != NULL) {
        *static_cast<float*> (value) = c->getPotential();
    } else {
        *static_cast<float*> (value) = -1.f;
    }
}

TweakHydroGraphLayer::TweakHydroGraphLayer(bool active)
{
    init(active);
}

TweakHydroGraphLayer::~TweakHydroGraphLayer()
{
}

TweakHydroGraphLayer::TweakHydroGraphLayer() : TweakGraphLayer()
{
}

void TweakHydroGraphLayer::init(bool active)
{
    TweakGraphLayer::init(active);
}

void TweakHydroGraphLayer::displayCurveInfo(TwBar *b, EditGraphOrthoLayer::SelectionData* curveData)
{
    TwAddVarCB(b, "Potential", TW_TYPE_FLOAT, SetCurvePotentialCallback, GetCurvePotentialCallback, curveData, " Group='CurveData' ");
    TweakGraphLayer::displayCurveInfo(b, curveData);
}

class TweakHydroGraphLayerResource : public ResourceTemplate<40, TweakHydroGraphLayer>
{
public:
    TweakHydroGraphLayerResource(ptr<ResourceManager> manager, const string &name, ptr<ResourceDescriptor> desc,
            const TiXmlElement *e = NULL) :
        ResourceTemplate<40, TweakHydroGraphLayer> (manager, name, desc)
    {
        e = e == NULL ? desc->descriptor : e;
        checkParameters(desc, e, "name,active,");

        bool active = true;
        if (e->Attribute("active") != NULL) {
            active = strcmp(e->Attribute("active"), "true") == 0;
        }
        init(active);
    }

};

extern const char tweakHydroGraphLayer[] = "tweakHydroGraphLayer";

static ResourceFactory::Type<tweakHydroGraphLayer, TweakHydroGraphLayerResource> TweakHydroGraphLayerType;

}
