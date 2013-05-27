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

#include "proland/ui/twbar/TweakViewHandler.h"

#include "ork/resource/ResourceTemplate.h"
#include "ork/render/FrameBuffer.h"

using namespace std;
using namespace ork;

namespace proland
{

TW_CALL void GetViewCallback(void *clientData)
{
    BasicViewHandler::Position q;
    ((BasicViewHandler*) clientData)->getPosition(q);
    if (Logger::INFO_LOGGER != NULL) {
        ostringstream oss;
        oss << "<view name=\"untitled\" key=\"0\" x0=\"";
        oss << q.x0 << "\" y0=\"";
        oss << q.y0 << "\" theta=\"";
        oss << q.theta << "\" phi=\"";
        oss << q.phi << "\" d=\"";
        oss << q.d << "\" sx=\"";
        oss << q.sx << "\" sy=\"";
        oss << q.sy << "\" sz=\"";
        oss << q.sz << "\"/>";
        Logger::INFO_LOGGER->log("UI", oss.str());
    }
}

TW_CALL void SetViewCallback(void *clientData)
{
    TweakViewHandler::Position *p = (TweakViewHandler::Position*) clientData;
    p->go();
}

void TweakViewHandler::Position::go()
{
    if (owner->animate) {
        owner->viewHandler->goToPosition(*this);
    } else {
        owner->viewHandler->jumpToPosition(*this);
    }
}

TweakViewHandler::TweakViewHandler() : TweakBarHandler()
{
}

TweakViewHandler::TweakViewHandler(ptr<BasicViewHandler> viewHandler, const vector<Position> &views, bool animate, bool active)
{
    init(viewHandler, views, animate, active);
}

void TweakViewHandler::init(ptr<BasicViewHandler> viewHandler, const vector<Position> &views, bool animate, bool active)
{
    assert(FrameBuffer::getError() == 0);
    TweakBarHandler::init("View", NULL, active);
    this->viewHandler = viewHandler;
    this->views = views;
    this->animate = animate;
}

TweakViewHandler::~TweakViewHandler()
{
}

void TweakViewHandler::updateBar(TwBar *bar)
{
    for (unsigned int i = 0; i < views.size(); ++i) {
        char def[256] = "group='View'";
        if (views[i].key != 0) {
            sprintf(def, "key='%c' group='View'", views[i].key);
        }
        TwAddButton(bar, views[i].name, SetViewCallback, &(views[i]), def);
    }
    if (!views.empty()) {
        TwAddSeparator(bar, NULL, "group='View'");
        TwAddVarRW(bar, "Animate", TW_TYPE_BOOL8, &animate, "group='View'");
        TwAddButton(bar, "Print", GetViewCallback, viewHandler.get(), "group='View'");
    }
}

void TweakViewHandler::swap(ptr<TweakViewHandler> o)
{
    TweakBarHandler::swap(o);
    std::swap(viewHandler, o->viewHandler);
    std::swap(views, o->views);
    std::swap(animate, o->animate);
}

class TweakViewHandlerResource : public ResourceTemplate<55, TweakViewHandler>
{
public:
    TweakViewHandlerResource(ptr<ResourceManager> manager, const string &name, ptr<ResourceDescriptor> desc, const TiXmlElement *e = NULL) :
        ResourceTemplate<55, TweakViewHandler> (manager, name, desc)
    {
        e = e == NULL ? desc->descriptor : e;
        checkParameters(desc, e, "name,viewHandler,animate,active,");

        ptr<BasicViewHandler> viewHandler = manager->loadResource(getParameter(desc, e, "viewHandler")).cast<BasicViewHandler>();

        bool active = true;
        if (e->Attribute("active") != NULL) {
            active = strcmp(e->Attribute("active"), "true") == 0;
        }

        vector<Position> views;
        const TiXmlNode *n = e->FirstChild();
        while (n != NULL) {
            const TiXmlElement *f = n->ToElement();
            if (f == NULL) {
                n = n->NextSibling();
                continue;
            }
            if (strcmp(f->Value(), "view") == 0) {
                checkParameters(desc, f, "name,key,x0,y0,theta,phi,d,sx,sy,sz,");
                float x0, y0, theta, phi, d, sx, sy, sz;
                getFloatParameter(desc, f, "x0", &x0);
                getFloatParameter(desc, f, "y0", &y0);
                getFloatParameter(desc, f, "theta", &theta);
                getFloatParameter(desc, f, "phi", &phi);
                getFloatParameter(desc, f, "d", &d);
                getFloatParameter(desc, f, "sx", &sx);
                getFloatParameter(desc, f, "sy", &sy);
                getFloatParameter(desc, f, "sz", &sz);
                Position p;
                p.owner = this;
                p.name = f->Attribute("name");
                p.key = f->Attribute("key") == NULL ? 0 : f->Attribute("key")[0];
                p.x0 = x0;
                p.y0 = y0;
                p.theta = theta;
                p.phi = phi;
                p.d = d;
                p.sx = sx;
                p.sy = sy;
                p.sz = sz;
                views.push_back(p);
            }
            n = n->NextSibling();
        }

        bool animate = true;
        if (e->Attribute("animate") != NULL) {
            animate = strcmp(e->Attribute("animate"), "true") == 0;
        }

        init(viewHandler, views, animate, active);
    }
};

extern const char tweakView[] = "tweakView";

static ResourceFactory::Type<tweakView, TweakViewHandlerResource> TweakViewHandlerType;

}
