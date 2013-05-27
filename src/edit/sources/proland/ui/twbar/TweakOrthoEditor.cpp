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

#include "proland/ui/twbar/TweakOrthoEditor.h"

#include "ork/resource/ResourceTemplate.h"
#include "proland/edit/EditOrthoProducer.h"

using namespace ork;

namespace proland
{

TW_CALL void ResetOrthoCB(void * clientData)
{
    EditOrthoProducer::getEditorHandler()->reset();
}

TW_CALL void SetOrthoEditorStateCallback(const void *value, void *clientData)
{
    static_cast<Editor*>(clientData)->setActive(*static_cast<const bool*>(value));
}

TW_CALL void GetOrthoEditorStateCallback(void *value, void *clientData)
{
    *static_cast<bool *>(value) = static_cast<Editor*>(clientData)->isActive();
}

TW_CALL void SetOrthoEditorGroupStateCallback(const void *value, void *clientData)
{
    EditOrthoProducer::getEditorHandler()->setActive(static_cast<Editor*>(clientData)->getGroup(), *static_cast<const bool*>(value));
}

TW_CALL void GetOrthoEditorGroupStateCallback(void *value, void *clientData)
{
    *static_cast<bool *>(value) = EditOrthoProducer::getEditorHandler()->isActive(static_cast<Editor*>(clientData)->getGroup());
}

TweakOrthoEditor::TweakOrthoEditor(bool active)
{
    TweakBarHandler::init("Ortho Editor", EditOrthoProducer::getEditorHandler(), active);
}

TweakOrthoEditor::~TweakOrthoEditor()
{
}

void TweakOrthoEditor::setActive(bool active)
{
    TweakBarHandler::setActive(active);
    eventHandler.cast<EditorHandler>()->setActive(active);
}

void TweakOrthoEditor::updateBar(TwBar *bar)
{
    EditorHandler *e = dynamic_cast<EditorHandler*>(eventHandler.get());
    if (e == NULL) {
        return;
    }

    TwAddVarRW(bar, "orthoBrushRadius", TW_TYPE_FLOAT, &(e->relativeRadius), " group=OrthoEditor label='Brush Radius' help='Size of the Ortho Editor Brush' min=0.0 step=0.01 ");
    TwAddVarRW(bar, "orthoBrushColor", TW_TYPE_COLOR4F, &(e->brushColor), " group=OrthoEditor label='Brush Color' help='Color applied to the texture' ");
    TwAddButton(bar, "orthoReset", ResetOrthoCB, NULL, " group=OrthoEditor label='Reset' help='Cancels all editing operations performed on active editors' ");
    char def[200];
    char name[20];
    sprintf(def, "%s/OrthoEditor label='Ortho Edition'", TwGetBarName(bar));
    TwDefine(def);

    set<string> groupNames;

    for (int i = 0; i < e->getEditorCount(); i++) {
        string n = e->getEditor(i)->getGroup();
        if (groupNames.find(n) == groupNames.end()) {
            groupNames.insert(n);
            sprintf(name, "orthoEditorGroup%d", i);
            sprintf(def, " group=%s label='Activate %s' help='Activate or Deactivate the selected Editor' ", n.c_str(), n.c_str());
            TwAddVarCB(bar, name, TW_TYPE_BOOLCPP, SetOrthoEditorGroupStateCallback, GetOrthoEditorGroupStateCallback, e->getEditor(i), def);

            sprintf(def, " group=%s ", n.c_str());
            TwAddSeparator(bar, NULL, def);

            sprintf(def, "%s/%s label='%s' group=OrthoEditor ", TwGetBarName(bar), n.c_str(), n.c_str());
            TwDefine(def);
        }
    }

    for (int i = 0; i < e->getEditorCount(); i++) {
        sprintf(name, "orthoEditor%d", i);
        sprintf(def, " group=%s label='%s' help='Activate or Deactivate the selected Editor' ", e->getEditor(i)->getGroup().c_str(), e->getEditor(i)->getName().c_str());
        TwAddVarCB(bar, name, TW_TYPE_BOOLCPP, SetOrthoEditorStateCallback, GetOrthoEditorStateCallback, e->getEditor(i), def);
    }
}

class TweakOrthoEditorResource : public ResourceTemplate<55, TweakOrthoEditor>
{
public:
    TweakOrthoEditorResource(ptr<ResourceManager> manager, const string &name, ptr<ResourceDescriptor> desc,
            const TiXmlElement *e = NULL) :
        ResourceTemplate<55, TweakOrthoEditor> (manager, name, desc)
    {
        e = e == NULL ? desc->descriptor : e;
        checkParameters(desc, e, "name,active,editor,");

        bool active = true;
        if (e->Attribute("active") != NULL) {
            active = strcmp(e->Attribute("active"), "true") == 0;
        }
        setActive(active);
    }
};

extern const char tweakOrtho[] = "tweakOrtho";

static ResourceFactory::Type<tweakOrtho, TweakOrthoEditorResource> TweakOrthoEditorType;

}
