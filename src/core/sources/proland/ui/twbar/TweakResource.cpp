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

#include "proland/ui/twbar/TweakResource.h"

#include <fstream>

#include "ork/render/Program.h"
#include "ork/resource/ResourceTemplate.h"
#include "ork/scenegraph/SceneNode.h"

using namespace std;
using namespace ork;

namespace proland
{

TweakResource::Data::Data()
{
}

TweakResource::Data::~Data()
{
}

struct EnumData : public TweakResource::Data
{
    ptr<ResourceManager> manager;

    vector<string> files;

    vector< vector<string> > replacements;

    int value;

    EnumData(ptr<ResourceManager> manager) : manager(manager), value(-1)
    {
    }

    int getValue()
    {
        if (value != -1) {
            return value;
        }

        string path;
        try {
            path = manager->getLoader()->findResource(files[0]);
        } catch (...) {
        }

        string str;
        string line;
        ifstream in(path.c_str());
        while (getline(in, line)) {
            str = str + line + '\n';
        }

        for (unsigned int i = 0; i < replacements.size(); ++i) {
            string src = replacements[i][0];
            string::size_type pos = str.find(src, 0);
            if (pos != string::npos) {
                value = i;
                return value;
            }
        }
        return 0;
    }

    void setValue(int value)
    {
        for (unsigned int f = 0; f < files.size(); ++f) {
            string path;
            try {
                path = manager->getLoader()->findResource(files[f]);
            } catch (...) {
            }

            string str;
            string line;
            ifstream in(path.c_str());
            while (getline(in, line)) {
                str = str + line + '\n';
            }

            for (unsigned int j = 0; j < replacements[0].size(); ++j) {
                for (unsigned int i = 0; i < replacements.size(); ++i) {
                    string src = replacements[i][j];
                    string::size_type pos = str.find(src, 0);
                    if (pos != string::npos) {
                        string dst = replacements[value][j];
                        while (pos != string::npos) {
                            str.replace(str.begin() + pos, str.begin() + pos + src.length(), dst);
                            pos = str.find(src, pos + src.length());
                        }
                        break;
                    }
                }
            }

            ofstream out(path.c_str());
            out << str;
            out.close();
        }

        this->value = value;
        manager->updateResources();
    }
};

struct UniformData : public TweakResource::Data
{
    ptr<ResourceManager> manager;

    string path;

    string dir;

    string file;

    string name;

    int dim;


    void getPath(const string &path, string &dir, string &file)
    {
        string::size_type p = path.find_first_of('/');
        if (p == string::npos) {
            dir = path;
            file = "";
            name = "";
        } else {
            string::size_type p2 = path.find('/', p + 1);
            dir = path.substr(0, p);
            file = path.substr(p + 1, p2 - p - 1);
            name = path.substr(p2 + 1);
        }
    }

    ptr<Object> find(ptr<SceneNode> obj, string path)
    {
        ptr<Module> s = obj->getModule(file);
        if (s != NULL) {
            return s;
        }
        for (unsigned int i = 0; i < obj->getChildrenCount(); ++i) {
            ptr<SceneNode> child = obj->getChild(i);
            if (child->hasFlag(dir)) {
                ptr<Object> o = find(child, file);
                if (o != NULL) {
                    return o;
                }
            }
        }
        return NULL;
    }

    UniformData(ptr<ResourceManager> manager, string path, int dim) :
        manager(manager), path(path), dim(dim)
    {
        getPath(path, dir, file);
    }

    void getValue(float *value)
    {
        ptr<SceneNode> scene = manager->loadResource("scene").cast<SceneNode>();
        ptr<Module> o = find(scene, path).cast<Module>();
        if (o == NULL) {
            return;
        }
        assert(!o->getUsers().empty());
        ptr<Uniform> u = (*(o->getUsers().begin()))->getUniform(name);
        if (u != NULL) {
            vec2f v2;
            vec3f v3;
            vec4f v4;
            switch (dim) {
            case 1:
                value[0] = u.cast<Uniform1f>()->get();
                break;
            case 2:
                v2 = u.cast<Uniform2f>()->get();
                value[0] = v2.x;
                value[1] = v2.y;
                break;
            case 3:
                v3 = u.cast<Uniform3f>()->get();
                value[0] = v3.x;
                value[1] = v3.y;
                value[2] = v3.z;
                break;
            case 4:
                v4 = u.cast<Uniform4f>()->get();
                value[0] = v4.x;
                value[1] = v4.y;
                value[2] = v4.z;
                value[3] = v4.w;
                break;
            }
        }
    }

    void setValue(const float *value)
    {
        ptr<SceneNode> scene = manager->loadResource("scene").cast<SceneNode>();
        ptr<Module> o = find(scene, path).cast<Module>();
        if (o == NULL) {
            return;
        }
        set<Program *> progs = o->getUsers();
        for(set<Program *>::iterator i = progs.begin(); i != progs.end(); ++i) {
            ptr<Uniform> u = (*i)->getUniform(name);
            if (u != NULL) {
                switch (dim) {
                case 1:
                    u.cast<Uniform1f>()->set(value[0]);
                    break;
                case 2:
                    u.cast<Uniform2f>()->set(vec2f(value[0], value[1]));
                    break;
                case 3:
                    u.cast<Uniform3f>()->set(vec3f(value[0], value[1], value[2]));
                    break;
                case 4:
                    u.cast<Uniform4f>()->set(vec4f(value[0], value[1], value[2], value[4]));
                    break;
                }

            }
        }
    }
};

TW_CALL void GetEnumCallback(void *value, void *clientData)
{
    *((int*) value) = ((EnumData*) clientData)->getValue();
}

TW_CALL void SetEnumCallback(const void *value, void *clientData)
{
    ((EnumData*) clientData)->setValue(*((int*) value));
}

TW_CALL void GetUniformCallback(void *value, void *clientData)
{
    ((UniformData*) clientData)->getValue((float*) value);
}

TW_CALL void SetUniformCallback(const void *value, void *clientData)
{
    ((UniformData*) clientData)->setValue((const float*) value);
}

TweakResource::TweakResource()
{
}

TweakResource::TweakResource(string name, ptr<ResourceManager> manager, const TiXmlElement *e)
{
    init(name, manager, e);
}

void TweakResource::init(string name, ptr<ResourceManager> manager, const TiXmlElement *e)
{
    TweakBarHandler::init(name.c_str(), NULL, true);
    this->manager = manager;
    this->e = (TiXmlElement*) e;
}

TweakResource::~TweakResource()
{
    clearData();
    delete e;
}

void TweakResource::swap(ptr<TweakResource> p)
{
    TweakBarHandler::swap(p);
    std::swap(manager, p->manager);
    std::swap(e, p->e);
    std::swap(datas, p->datas);
}

void TweakResource::setParam(TwBar *bar, const TiXmlElement *el, const char* param)
{
    if (el->Attribute(param) != NULL) {
        TwSetParam(bar, el->Attribute("label"), param, TW_PARAM_CSTRING, 1, el->Attribute(param));
    }
}

void TweakResource::clearData()
{
    for (int i = 0; i < (int) datas.size(); i++) {
        delete datas[i];
    }
    datas.clear();
}

static int TWBAR_COUNTER = 0;

void TweakResource::updateBar(TwBar *bar)
{
    clearData();

    const TiXmlNode *n = e->FirstChild();
    while (n != NULL) {
        const TiXmlElement *f = n->ToElement();
        if (f == NULL) {
            n = n->NextSibling();
            continue;
        }
        if (strcmp(f->Value(), "enum") == 0) {
            EnumData *enumVar = new EnumData(manager);
            string files = string(f->Attribute("files")) + ",";
            string::size_type start = 0;
            string::size_type index;
            while ((index = files.find(',', start)) != string::npos) {
                enumVar->files.push_back(files.substr(start, index - start));
                start = index + 1;
            }
            datas.push_back(enumVar);

            TwEnumVal enumValues[256];
            int nbValues = 0;
            const TiXmlNode *m = f->FirstChild();
            while (m != NULL) {
                const TiXmlElement *g = m->ToElement();
                enumValues[nbValues].Value = nbValues;
                enumValues[nbValues].Label = g->Attribute("label");
                vector<string> values;
                if (g->FirstChild() != NULL && strcmp(g->FirstChild()->Value(), "a") == 0) {
                    const TiXmlNode *l = g->FirstChild();
                    while (l != NULL) {
                        values.push_back(l->ToElement()->GetText());
                        l = l->NextSibling();
                    }
                } else {
                    values.push_back(string(g->GetText()));
                }
                enumVar->replacements.push_back(values);
                m = m->NextSibling();
                nbValues++;
            }

            if (nbValues == 2 && strcmp(enumValues[0].Label, "OFF") == 0 && strcmp(enumValues[1].Label, "ON") == 0) {
                TwAddVarCB(bar, f->Attribute("label"), TW_TYPE_BOOL32, SetEnumCallback, GetEnumCallback, enumVar, NULL);
            } else {
                ostringstream enumName;
                enumName << "enum" << TWBAR_COUNTER++;
                TwType enumType = TwDefineEnum(enumName.str().c_str(), enumValues, nbValues);
                TwAddVarCB(bar, f->Attribute("label"), enumType, SetEnumCallback, GetEnumCallback, enumVar, NULL);
            }
            setParam(bar, f, "key");
            setParam(bar, f, "group");
            setParam(bar, f, "help");
        }
        if (strcmp(f->Value(), "bool") == 0) {
            UniformData *var = new UniformData(manager, f->Attribute("path"), 1);
            datas.push_back(var);
            TwAddVarCB(bar, f->Attribute("label"), TW_TYPE_BOOLCPP, SetUniformCallback, GetUniformCallback, var, NULL);
            setParam(bar, f, "group");
            setParam(bar, f, "help");
        }
        if (strcmp(f->Value(), "float") == 0) {
            UniformData *var = new UniformData(manager, f->Attribute("path"), 1);
            datas.push_back(var);
            TwAddVarCB(bar, f->Attribute("label"), TW_TYPE_FLOAT, SetUniformCallback, GetUniformCallback, var, NULL);
            setParam(bar, f, "keyincr");
            setParam(bar, f, "keydecr");
            setParam(bar, f, "min");
            setParam(bar, f, "max");
            setParam(bar, f, "step");
            setParam(bar, f, "group");
            setParam(bar, f, "help");
        }
        if (strcmp(f->Value(), "color") == 0) {
            UniformData *var = new UniformData(manager, f->Attribute("path"), 3);
            datas.push_back(var);
            TwAddVarCB(bar, f->Attribute("label"), TW_TYPE_COLOR3F, SetUniformCallback, GetUniformCallback, var, NULL);
            setParam(bar, f, "group");
            setParam(bar, f, "help");
        }
        if (strcmp(f->Value(), "direction") == 0) {
            UniformData *var = new UniformData(manager, f->Attribute("path"), 3);
            datas.push_back(var);
            TwAddVarCB(bar, f->Attribute("label"), TW_TYPE_DIR3F, SetUniformCallback, GetUniformCallback, var, NULL);
            setParam(bar, f, "group");
            setParam(bar, f, "help");
        }
        n = n->NextSibling();
    }

}

class TweakResourceResource : public ResourceTemplate<0, TweakResource>
{
public:
    TweakResourceResource(ptr<ResourceManager> manager, const string &name, ptr<ResourceDescriptor> desc, const TiXmlElement *e = NULL) :
        ResourceTemplate<0, TweakResource>(manager, name, desc)
    {
        TiXmlElement *te = new TiXmlElement(*e);
        init(name, manager, te);
    }

};

extern const char tweakBar[] = "tweakBar";

static ResourceFactory::Type<tweakBar, TweakResourceResource> TweakResourceType;

}

