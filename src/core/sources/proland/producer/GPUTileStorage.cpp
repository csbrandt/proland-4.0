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

#include "proland/producer/GPUTileStorage.h"

#include "ork/resource/ResourceTemplate.h"

using namespace std;
using namespace ork;

namespace ork
{

void getParameters(const ptr<ResourceDescriptor> desc, const TiXmlElement *e, TextureInternalFormat &ff, TextureFormat &f, PixelType &t);

void getParameters(const ptr<ResourceDescriptor> desc, const TiXmlElement *e, Texture::Parameters &params);

}

namespace proland
{

const char *mipmapShader = "\
uniform ivec4 bufferLayerLevelWidth;\n\
#ifdef _VERTEX_\n\
layout(location=0) in vec4 vertex;\n\
void main () { gl_Position = vertex; }\n\
#endif\n\
#ifdef _GEOMETRY_\n\
#extension GL_EXT_geometry_shader4 : enable\n\
layout(triangles) in;\n\
layout(triangle_strip,max_vertices=3) out;\n\
void main() { gl_Layer = bufferLayerLevelWidth.y; gl_Position = gl_PositionIn[0]; EmitVertex(); gl_Position = gl_PositionIn[1]; EmitVertex(); gl_Position = gl_PositionIn[2]; EmitVertex(); EndPrimitive(); }\n\
#endif\n\
#ifdef _FRAGMENT_\n\
uniform sampler2DArray input_[8];\n\
layout(location=0) out vec4 output_;\n\
void main() {\n\
    vec2 xy = floor(gl_FragCoord.xy);\n\
    vec4 uv = vec4(xy + vec2(0.25), xy + vec2(0.75)) / float(bufferLayerLevelWidth.w);\n\
    vec4 result;\n\
    switch (bufferLayerLevelWidth.x) {\n\
    case 0:\n\
        result = texture(input_[0], vec3(uv.zw, bufferLayerLevelWidth.y), bufferLayerLevelWidth.z);\n\
        result += texture(input_[0], vec3(uv.xw, bufferLayerLevelWidth.y), bufferLayerLevelWidth.z);\n\
        result += texture(input_[0], vec3(uv.zy, bufferLayerLevelWidth.y), bufferLayerLevelWidth.z);\n\
        result += texture(input_[0], vec3(uv.zw, bufferLayerLevelWidth.y), bufferLayerLevelWidth.z);\n\
        break;\n\
    case 1:\n\
        result = texture(input_[1], vec3(uv.zw, bufferLayerLevelWidth.y), bufferLayerLevelWidth.z);\n\
        result += texture(input_[1], vec3(uv.xw, bufferLayerLevelWidth.y), bufferLayerLevelWidth.z);\n\
        result += texture(input_[1], vec3(uv.zy, bufferLayerLevelWidth.y), bufferLayerLevelWidth.z);\n\
        result += texture(input_[1], vec3(uv.zw, bufferLayerLevelWidth.y), bufferLayerLevelWidth.z);\n\
        break;\n\
    case 2:\n\
        result = texture(input_[2], vec3(uv.zw, bufferLayerLevelWidth.y), bufferLayerLevelWidth.z);\n\
        result += texture(input_[2], vec3(uv.xw, bufferLayerLevelWidth.y), bufferLayerLevelWidth.z);\n\
        result += texture(input_[2], vec3(uv.zy, bufferLayerLevelWidth.y), bufferLayerLevelWidth.z);\n\
        result += texture(input_[2], vec3(uv.zw, bufferLayerLevelWidth.y), bufferLayerLevelWidth.z);\n\
        break;\n\
    case 3:\n\
        result = texture(input_[3], vec3(uv.zw, bufferLayerLevelWidth.y), bufferLayerLevelWidth.z);\n\
        result += texture(input_[3], vec3(uv.xw, bufferLayerLevelWidth.y), bufferLayerLevelWidth.z);\n\
        result += texture(input_[3], vec3(uv.zy, bufferLayerLevelWidth.y), bufferLayerLevelWidth.z);\n\
        result += texture(input_[3], vec3(uv.zw, bufferLayerLevelWidth.y), bufferLayerLevelWidth.z);\n\
        break;\n\
    case 4:\n\
        result = texture(input_[4], vec3(uv.zw, bufferLayerLevelWidth.y), bufferLayerLevelWidth.z);\n\
        result += texture(input_[4], vec3(uv.xw, bufferLayerLevelWidth.y), bufferLayerLevelWidth.z);\n\
        result += texture(input_[4], vec3(uv.zy, bufferLayerLevelWidth.y), bufferLayerLevelWidth.z);\n\
        result += texture(input_[4], vec3(uv.zw, bufferLayerLevelWidth.y), bufferLayerLevelWidth.z);\n\
        break;\n\
    case 5:\n\
        result = texture(input_[5], vec3(uv.zw, bufferLayerLevelWidth.y), bufferLayerLevelWidth.z);\n\
        result += texture(input_[5], vec3(uv.xw, bufferLayerLevelWidth.y), bufferLayerLevelWidth.z);\n\
        result += texture(input_[5], vec3(uv.zy, bufferLayerLevelWidth.y), bufferLayerLevelWidth.z);\n\
        result += texture(input_[5], vec3(uv.zw, bufferLayerLevelWidth.y), bufferLayerLevelWidth.z);\n\
        break;\n\
    case 6:\n\
        result = texture(input_[6], vec3(uv.zw, bufferLayerLevelWidth.y), bufferLayerLevelWidth.z);\n\
        result += texture(input_[6], vec3(uv.xw, bufferLayerLevelWidth.y), bufferLayerLevelWidth.z);\n\
        result += texture(input_[6], vec3(uv.zy, bufferLayerLevelWidth.y), bufferLayerLevelWidth.z);\n\
        result += texture(input_[6], vec3(uv.zw, bufferLayerLevelWidth.y), bufferLayerLevelWidth.z);\n\
        break;\n\
    case 7:\n\
        result = texture(input_[7], vec3(uv.zw, bufferLayerLevelWidth.y), bufferLayerLevelWidth.z);\n\
        result += texture(input_[7], vec3(uv.xw, bufferLayerLevelWidth.y), bufferLayerLevelWidth.z);\n\
        result += texture(input_[7], vec3(uv.zy, bufferLayerLevelWidth.y), bufferLayerLevelWidth.z);\n\
        result += texture(input_[7], vec3(uv.zw, bufferLayerLevelWidth.y), bufferLayerLevelWidth.z);\n\
        break;\n\
    }\n\
    output_ = result * 0.25;\n\
}\n\
#endif\n";

GPUTileStorage::GPUSlot::GPUSlot(TileStorage *owner, int index, ptr<Texture2DArray> t, int l) :
    Slot(owner), t(t), l(l), index(index)
{
}

GPUTileStorage::GPUSlot::~GPUSlot()
{
}

void GPUTileStorage::GPUSlot::copyPixels(ptr<FrameBuffer> fb, int x, int y, int w, int h)
{
    fb->copyPixels(0, 0, l, x, y, w, h, *t, 0);
}

void GPUTileStorage::GPUSlot::setSubImage(int x, int y, int w, int h, TextureFormat f, PixelType t, const Buffer::Parameters &s, const Buffer &pixels)
{
    this->t->setSubImage(0, x, y, l, w, h, 1, f, t, s, pixels);
}

GPUTileStorage::GPUTileStorage() : TileStorage()
{
}

GPUTileStorage::GPUTileStorage(int tileSize, int nTiles,
    TextureInternalFormat internalf, TextureFormat f, PixelType t,
    const Texture::Parameters &params, bool useTileMap) :
        TileStorage(tileSize, nTiles)
{
    init(tileSize, nTiles, internalf, f, t, params, useTileMap);
}

void GPUTileStorage::init(int tileSize, int nTiles,
    TextureInternalFormat internalf, TextureFormat f, PixelType t,
    const Texture::Parameters &params, bool useTileMap)
{
    TileStorage::init(tileSize, nTiles);

    int maxLayers = Texture2DArray::getMaxLayers();
    int nTextures = nTiles / maxLayers + (nTiles % maxLayers == 0 ? 0 : 1);
    needMipmaps = false;

    for (int i = 0; i < nTextures; ++i) {
        int nLayers = i == nTextures - 1 && nTiles % maxLayers != 0 ? nTiles % maxLayers : maxLayers;
        ptr<Texture2DArray> tex = new Texture2DArray(tileSize, tileSize, nLayers,
            internalf, f, t, params, Buffer::Parameters(), CPUBuffer());
        needMipmaps = needMipmaps || (tex->hasMipmaps());
        textures.push_back(tex);
        tex->generateMipMap();
        for (int j = 0; j < nLayers; ++j) {
            freeSlots.push_back(new GPUSlot(this, i, textures[i], j));
        }
    }

    if (needMipmaps) {
        assert(nTextures <= 8);
        dirtySlots = new set<GPUSlot*>[nTextures];
        fbo = new FrameBuffer();
        fbo->setReadBuffer(BufferId(0));
        fbo->setDrawBuffers(BufferId(COLOR0 | COLOR1));
        mipmapProg = new Program(new Module(330, mipmapShader));
        ptr<Sampler> s = new Sampler(Sampler::Parameters().min(NEAREST).mag(NEAREST).wrapS(CLAMP_TO_EDGE).wrapT(CLAMP_TO_EDGE));
        for (int i = 0; i < nTextures; ++i) {
            char buf[256];
            sprintf(buf, "input_[%d]", i);
            mipmapProg->getUniformSampler(string(buf))->set(textures[i]);
            mipmapProg->getUniformSampler(string(buf))->setSampler(s);
        }
        mipmapParams = mipmapProg->getUniform4i("bufferLayerLevelWidth");
    } else {
        dirtySlots = NULL;
    }

    changes = false;
    if (useTileMap) {
        assert(nTextures == 1);
        tileMap = new Texture2D(4096, 8, RG8, RG, UNSIGNED_BYTE,
            Texture::Parameters().wrapS(CLAMP_TO_EDGE).wrapT(CLAMP_TO_EDGE).min(NEAREST).mag(NEAREST),
            Buffer::Parameters(), CPUBuffer());
    }
}

GPUTileStorage::~GPUTileStorage()
{
    if (dirtySlots != NULL) {
        delete[] dirtySlots;
    }
}

int GPUTileStorage::getTextureCount()
{
    return (int) textures.size();
}

ptr<Texture2DArray> GPUTileStorage::getTexture(int index)
{
    return textures[index];
}

ptr<Texture2D> GPUTileStorage::getTileMap()
{
    return tileMap;
}

void GPUTileStorage::notifyChange(GPUSlot *s)
{
    if (needMipmaps) {
        dirtySlots[s->index].insert(s);
        changes = true;
    }
}

void GPUTileStorage::generateMipMap()
{
    if (changes) {
        int level = 1;
        int width = tileSize / 2;
        while (width >= 1) {
            fbo->setViewport(vec4i(0, 0, width, width));
            for (unsigned int n = 0; n < textures.size(); ++n) {
                fbo->setTextureBuffer(BufferId(1 << n), textures[n], level, -1);
            }
            for (unsigned int n = 0; n < textures.size(); ++n) {
                fbo->setDrawBuffer(BufferId(1 << n));
                set<GPUSlot*>::iterator i = dirtySlots[n].begin();
                while (i != dirtySlots[n].end()) {
                    GPUSlot *s = *i;
                    mipmapParams->set(vec4i(s->index, s->l, level - 1, width));
                    fbo->drawQuad(mipmapProg);
                    ++i;
                }
            }
            width /= 2;
            level += 1;
        }
        for (unsigned int n = 0; n < textures.size(); ++n) {
            dirtySlots[n].clear();
        }
        changes = false;
    }
}

void GPUTileStorage::swap(ptr<GPUTileStorage> s)
{
    assert(false);
}

class GPUTileStorageResource : public ResourceTemplate<0, GPUTileStorage>
{
public:
    GPUTileStorageResource(ptr<ResourceManager> manager, const string &name, ptr<ResourceDescriptor> desc, const TiXmlElement *e = NULL) :
        ResourceTemplate<0, GPUTileStorage>(manager, name, desc)
    {
        e = e == NULL ? desc->descriptor : e;
        int tileSize;
        int nTiles;
        TextureInternalFormat tf;
        TextureFormat f;
        PixelType t;
        Texture::Parameters params;
        bool useTileMap = false;
        checkParameters(desc, e, "name,tileSize,nTiles,tileMap,internalformat,format,type,min,mag,minLod,maxLod,minLevel,maxLevel,swizzle,anisotropy,");
        getParameters(desc, e, tf, f, t);
        getParameters(desc, e, params);
        getIntParameter(desc, e, "tileSize", &tileSize);
        getIntParameter(desc, e, "nTiles", &nTiles);
        if (e->Attribute("tileMap") != NULL) {
            useTileMap = strcmp(e->Attribute("tileMap"), "true") == 0;
        }

        init(tileSize, nTiles, tf, f, t, params, useTileMap);
    }
};

extern const char gpuTileStorage[] = "gpuTileStorage";

static ResourceFactory::Type<gpuTileStorage, GPUTileStorageResource> GPUTileStorageType;

}
