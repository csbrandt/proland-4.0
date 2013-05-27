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

#include "proland/ui/twbar/TweakDemEditor.h"

#include "ork/resource/ResourceTemplate.h"
#include "proland/edit/EditElevationProducer.h"

using namespace ork;

namespace proland
{

TW_CALL void ResetDemCB(void * clientData)
{
    EditElevationProducer::getEditorHandler()->reset();
}

TW_CALL void SetDemEditorStateCallback(const void *value, void *clientData)
{
    static_cast<Editor*>(clientData)->setActive(*static_cast<const bool*>(value));
}

TW_CALL void GetDemEditorStateCallback(void *value, void *clientData)
{
    *static_cast<bool *>(value) = static_cast<Editor*>(clientData)->isActive();
}

TW_CALL void SetDemEditorGroupStateCallback(const void *value, void *clientData)
{
    EditElevationProducer::getEditorHandler()->setActive(static_cast<Editor*>(clientData)->getGroup(), *static_cast<const bool*>(value));
}

TW_CALL void GetDemEditorGroupStateCallback(void *value, void *clientData)
{
    *static_cast<bool *>(value) = EditElevationProducer::getEditorHandler()->isActive(static_cast<Editor*>(clientData)->getGroup());
}

TW_CALL void SetDemEditModeCallback(const void *value, void *clientData)
{
    ptr<EditorHandler> e = EditElevationProducer::getEditorHandler();
    for (int i = 0; i < e->getEditorCount(); ++i) {
        dynamic_cast<EditElevationProducer*>(e->getEditor(i))->setEditMode(*static_cast<const BlendEquation*>(value));
    }
}

TW_CALL void GetDemEditModeCallback(void *value, void *clientData)
{
    *static_cast<BlendEquation*>(value) = dynamic_cast<EditElevationProducer*>(EditElevationProducer::getEditorHandler()->getEditor(0))->getEditMode();
}

TweakDemEditor::TweakDemEditor(bool active)
{
    TweakBarHandler::init("Dem Editor", EditElevationProducer::getEditorHandler(), active);
}

TweakDemEditor::~TweakDemEditor()
{
}

void TweakDemEditor::setActive(bool active)
{
    TweakBarHandler::setActive(active);
    eventHandler.cast<EditorHandler>()->setActive(active);
}

void TweakDemEditor::updateBar(TwBar *bar)
{
    EditorHandler *e = dynamic_cast<EditorHandler*>(eventHandler.get());
    if (e == NULL) {
        return;
    }

    // Adding the list of edited graphs
    TwEnumVal *editModes= new TwEnumVal[2];
    editModes[0].Value = ADD;
    editModes[0].Label = "ADD";
    editModes[1].Value = MAX;
    editModes[1].Label = "MAX";

    TwType editType = TwDefineEnum("EditType", editModes, 2);
    TwAddVarCB(bar, "EditMode", editType, SetDemEditModeCallback, GetDemEditModeCallback, NULL, " label='Edit Mode' group='DemEditor' key='e' ");

    TwAddVarRW(bar, "demBrushRadius", TW_TYPE_FLOAT, &(e->relativeRadius), " group=DemEditor label='Brush Radius' help='Size of the Dem Editor Brush' min=0.0 step=0.01 ");
    TwAddVarRW(bar, "demBrushColor", TW_TYPE_FLOAT, &(e->brushColor[0]), " group=DemEditor label='Brush altitude' help='Altitude applied to the texture' step='0.1' ");
    TwAddButton(bar, "demReset", ResetDemCB, NULL, " group=DemEditor label='Reset' help='Cancels all editing operations performed on active editors' ");
    char def[200];
    sprintf(def, "%s/DemEditor label='Dem Edition'", TwGetBarName(bar));
        char name[20];
    TwDefine(def);

    set<string> groupNames;
    for (int i = 0; i < e->getEditorCount(); i++) {
        string n = e->getEditor(i)->getGroup();
        if (groupNames.find(n) == groupNames.end()) {
            groupNames.insert(n);
            sprintf(name, "demEditorGroup%d", i);
            sprintf(def, " group=%s label='Activate %s' help='Activate or Deactivate the selected Editor' ", n.c_str(), n.c_str());
            TwAddVarCB(bar, name, TW_TYPE_BOOLCPP, SetDemEditorGroupStateCallback, GetDemEditorGroupStateCallback, e->getEditor(i), def);

            sprintf(def, " group=%s ", n.c_str());
            TwAddSeparator(bar, NULL, def);

            sprintf(def, "%s/%s label='%s' group=DemEditor ", TwGetBarName(bar), n.c_str(), n.c_str());
            TwDefine(def);
        }
    }

    for (int i = 0; i < e->getEditorCount(); i++) {
        sprintf(name, "DemEditor%d", i);
        sprintf(def, " group=%s label='%s' help='Activate or Deactivate the selected Editor' ", e->getEditor(i)->getName().c_str(), e->getEditor(i)->getName().c_str());
        TwAddVarCB(bar, name, TW_TYPE_BOOLCPP, SetDemEditorStateCallback, GetDemEditorStateCallback, e->getEditor(i), def);
    }
    delete[] editModes;
}

class TweakDemEditorResource : public ResourceTemplate<55, TweakDemEditor>
{
public:
    TweakDemEditorResource(ptr<ResourceManager> manager, const string &name, ptr<ResourceDescriptor> desc,
            const TiXmlElement *e = NULL) :
        ResourceTemplate<55, TweakDemEditor> (manager, name, desc)
    {
        e = e == NULL ? desc->descriptor : e;
        checkParameters(desc, e, "name,active,");

        bool active = true;
        if (e->Attribute("active") != NULL) {
            active = strcmp(e->Attribute("active"), "true") == 0;
        }
        setActive(active);
    }
};

extern const char tweakDem[] = "tweakDem";

static ResourceFactory::Type<tweakDem, TweakDemEditorResource> TweakDemEditorType;

}
