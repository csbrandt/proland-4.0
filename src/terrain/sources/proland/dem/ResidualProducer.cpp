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

#include "proland/dem/ResidualProducer.h"

#include <fstream>
#include <sstream>

#include "tiffio.h"

#include "ork/core/Logger.h"
#include "ork/resource/ResourceTemplate.h"
#include "proland/producer/CPUTileStorage.h"
#include "proland/util/mfs.h"

#include <pthread.h>
#include <cstring>

using namespace std;
using namespace ork;

namespace proland
{

//#define SINGLE_FILE

#define MAX_TILE_SIZE 256

void *ResidualProducer::key = NULL;

void residualDelete(void* data)
{
    delete[] (unsigned char*) data;
}

ResidualProducer::ResidualProducer(ptr<TileCache> cache, const char *name, int deltaLevel, float zscale) :
    TileProducer("ResidualProducer", "CreateResidualTile")
{
    init(cache, name, deltaLevel, zscale);
}

ResidualProducer::ResidualProducer() : TileProducer("ResidualProducer", "CreateResidualTile")
{
}

void ResidualProducer::init(ptr<TileCache> cache, const char *name, int deltaLevel, float zscale)
{
    TileProducer::init(cache, false);
    this->name = name;

    if (strlen(name) == 0) {
        this->tileFile = NULL;
        this->minLevel = 0;
        this->maxLevel = 32;
        this->rootLevel = 0;
        this->deltaLevel = 0;
        this->rootTx = 0;
        this->rootTy = 0;
        this->scale = 1.0;
    } else {
        fopen(&tileFile, name, "rb");
        if (tileFile == NULL) {
            if (Logger::ERROR_LOGGER != NULL) {
                Logger::ERROR_LOGGER->log("DEM", "Cannot open file '" + string(name) + "'");
            }
            maxLevel = -1;
            scale = 1.0;
        } else {
            fread(&minLevel, sizeof(int), 1, tileFile);
            fread(&maxLevel, sizeof(int), 1, tileFile);
            fread(&tileSize, sizeof(int), 1, tileFile);
            fread(&rootLevel, sizeof(int), 1, tileFile);
            fread(&rootTx, sizeof(int), 1, tileFile);
            fread(&rootTy, sizeof(int), 1, tileFile);
            fread(&scale, sizeof(float), 1, tileFile);
        }

        this->deltaLevel = rootLevel == 0 ? deltaLevel : 0;
        scale = scale * zscale;

        int ntiles = minLevel + ((1 << (max(maxLevel - minLevel, 0) * 2 + 2)) - 1) / 3;
        header = sizeof(float) + sizeof(int) * (6 + ntiles * 2);
        offsets = new unsigned int[ntiles * 2];
        if (tileFile != NULL) {
            fread(offsets, sizeof(unsigned int) * ntiles * 2, 1, tileFile);
#ifndef SINGLE_FILE
            fclose(tileFile);
            tileFile = NULL;
#endif
        }

        if (key == NULL) {
            key = new pthread_key_t;
            pthread_key_create((pthread_key_t*) key, residualDelete);
        }

        assert(tileSize + 5 < MAX_TILE_SIZE);
        assert(deltaLevel <= minLevel);

#ifdef SINGLE_FILE
        mutex = new pthread_mutex_t;
        pthread_mutex_init((pthread_mutex_t*) mutex, NULL);
#endif
    }
}

ResidualProducer::~ResidualProducer()
{
#ifdef SINGLE_FILE
    fclose(tileFile);
    pthread_mutex_destroy((pthread_mutex_t*) mutex);
    delete (pthread_mutex_t*) mutex;
#endif
    delete[] offsets;
}

int ResidualProducer::getBorder()
{
    return 2;
}

int ResidualProducer::getMinLevel()
{
    return minLevel;
}

int ResidualProducer::getDeltaLevel()
{
    return deltaLevel;
}

void ResidualProducer::addProducer(ptr<ResidualProducer> p)
{
    producers.push_back(p);
}

bool ResidualProducer::hasTile(int level, int tx, int ty)
{
    int l = level + deltaLevel - rootLevel;
    if (l >= 0 && (tx >> l) == rootTx && (ty >> l) == rootTy) {
        if (l <= maxLevel) {
            return true;
        }
        for (unsigned int i = 0; i < producers.size(); ++i) {
            if (producers[i]->hasTile(level + deltaLevel, tx, ty)) {
                return true;
            }
        }
    }
    return false;
}

bool ResidualProducer::doCreateTile(int level, int tx, int ty, TileStorage::Slot *data)
{
    int l = level + deltaLevel - rootLevel;
    if (l >= 0 && (tx >> l) == rootTx && (ty >> l) == rootTy) {
        if (l > maxLevel) {
            for (unsigned int i = 0; i < producers.size(); ++i) {
                producers[i]->doCreateTile(level + deltaLevel, tx, ty, data);
            }
            return true;
        }
    } else {
        return true;
    }

    if (Logger::DEBUG_LOGGER != NULL) {
        ostringstream oss;
        oss << "Residual tile " << getId() << " " << level << " " << tx << " " << ty;
        Logger::DEBUG_LOGGER->log("DEM", oss.str());
    }

    level = l;
    tx = tx - (rootTx << level);
    ty = ty - (rootTy << level);

    CPUTileStorage<float>::CPUSlot *cpuData = dynamic_cast<CPUTileStorage<float>::CPUSlot*>(data);
    assert(cpuData != NULL);
    assert(dynamic_cast<CPUTileStorage<float>*>(cpuData->getOwner())->getChannels() == 1);
    if ((int) name.size() == 0) {
        tileSize = cpuData->getOwner()->getTileSize() - 5;
        readTile(level, tx, ty, NULL, NULL, NULL, cpuData->data);
    } else {
        assert(cpuData->getOwner()->getTileSize() == tileSize + 5);

        unsigned char *tsData = (unsigned char*) pthread_getspecific(*((pthread_key_t*) key));
        if (tsData == NULL) {
            tsData = new unsigned char[MAX_TILE_SIZE * MAX_TILE_SIZE * 4];
            pthread_setspecific(*((pthread_key_t*) key), tsData);
        }
        unsigned char *compressedData = tsData;
        unsigned char *uncompressedData = tsData + MAX_TILE_SIZE * MAX_TILE_SIZE * 2;

        if (deltaLevel > 0 && level == deltaLevel) {
            float *tmp = new float[(tileSize + 5) * (tileSize + 5)];
            readTile(0, 0, 0, compressedData, uncompressedData, NULL, cpuData->data);
            for (int i = 1; i <= deltaLevel; ++i) {
                upsample(i, 0, 0, cpuData->data, tmp);
                readTile(i, 0, 0, compressedData, uncompressedData, tmp, cpuData->data);
            }
            delete[] tmp;
        } else {
            readTile(level, tx, ty, compressedData, uncompressedData, NULL, cpuData->data);
        }
    }

    return true;
}

void ResidualProducer::swap(ptr<ResidualProducer> p)
{
    TileProducer::swap(p);
    std::swap(name, p->name);
    std::swap(tileSize, p->tileSize);
    std::swap(rootLevel, p->rootLevel);
    std::swap(deltaLevel, p->deltaLevel);
    std::swap(rootTx, p->rootTx);
    std::swap(rootTy, p->rootTy);
    std::swap(minLevel, p->minLevel);
    std::swap(maxLevel, p->maxLevel);
    std::swap(scale, p->scale);
    std::swap(header, p->header);
    std::swap(offsets, p->offsets);
    std::swap(mutex, p->mutex);
    std::swap(tileFile, p->tileFile);
    std::swap(producers, p->producers);
}

int ResidualProducer::getTileSize(int level)
{
    return level < minLevel ? tileSize >> (minLevel - level) : tileSize;
}

int ResidualProducer::getTileId(int level, int tx, int ty)
{
    if (level < minLevel) {
        return level;
    } else {
        int l = max(level - minLevel, 0);
        return minLevel + tx + ty * (1 << l) + ((1 << (2 * l)) - 1) / 3;
    }
}

void ResidualProducer::readTile(int level, int tx, int ty,
        unsigned char* compressedData, unsigned char *uncompressedData,
        float *tile, float *result)
{
    int tilesize = getTileSize(level) + 5;

    if ((int) name.size() == 0) {
        if (tile != NULL) {
            for (int j = 0; j < tilesize; ++j) {
                for (int i = 0; i < tilesize; ++i) {
                    result[i + j * (tileSize + 5)] = tile[i + j * (tileSize + 5)];
                }
            }
        } else {
            for (int j = 0; j < tilesize; ++j) {
                for (int i = 0; i < tilesize; ++i) {
                    result[i + j * (tileSize + 5)] = 0.f;
                }
            }
        }
    } else {
        int tileid = getTileId(level, tx, ty);
        int fsize = offsets[2 * tileid + 1] - offsets[2 * tileid];
        assert(fsize < (tileSize + 5) * (tileSize + 5) * 2);

#ifdef SINGLE_FILE
        pthread_mutex_lock((pthread_mutex_t*) mutex);
        fseek64(tileFile, header + offsets[2 * tileid], SEEK_SET);
        fread(compressedData, fsize, 1, tileFile);
        pthread_mutex_unlock((pthread_mutex_t*) mutex);
#else
        FILE *file;
        fopen(&file, name.c_str(), "rb");
        fseek64(file, header + offsets[2 * tileid], SEEK_SET);
        fread(compressedData, fsize, 1, file);
        fclose(file);
#endif
        /*ifstream fs(name.c_str(), ios::binary);
        fs.seekg(header + offsets[2 * tileid], ios::beg);
        fs.read((char*) compressedData, fsize);
        fs.close();*/

        // TODO compare perfs FILE vs ifstream vs mmap

        mfs_file fd;
        mfs_open(compressedData, fsize, (char *)"r", &fd);
        TIFF* tf = TIFFClientOpen("name", "r", &fd,
            (TIFFReadWriteProc) mfs_read, (TIFFReadWriteProc) mfs_write, (TIFFSeekProc) mfs_lseek,
            (TIFFCloseProc) mfs_close, (TIFFSizeProc) mfs_size, (TIFFMapFileProc) mfs_map,
            (TIFFUnmapFileProc) mfs_unmap);
        TIFFReadEncodedStrip(tf, 0, uncompressedData, (tsize_t) -1);
        TIFFClose(tf);

        if (tile != NULL) {
            for (int j = 0; j < tilesize; ++j) {
                for (int i = 0; i < tilesize; ++i) {
                    int off = 2 * (i + j * tilesize);
                    int toff = i + j * (tileSize + 5);
                    short z = short(uncompressedData[off + 1]) << 8 | short(uncompressedData[off]);
                    result[toff] = tile[toff] + z * scale;
                }
            }
        } else {
            for (int j = 0; j < tilesize; ++j) {
                for (int i = 0; i < tilesize; ++i) {
                    int off = 2 * (i + j * tilesize);
                    short z = short(uncompressedData[off + 1]) << 8 | short(uncompressedData[off]);
                    result[i + j * (tileSize + 5)] = z * scale;
                }
            }
        }
    }
}

void ResidualProducer::upsample(int level, int tx, int ty, float *parentTile, float *result)
{
    int n = tileSize + 5;
    int tilesize = getTileSize(level);
    int px = 1 + (tx % 2) * tilesize / 2;
    int py = 1 + (ty % 2) * tilesize / 2;
    for (int j = 0; j <= tilesize + 4; ++j) {
        for (int i = 0; i <= tilesize + 4; ++i) {
            float z;
            if (j%2 == 0) {
                if (i%2 == 0) {
                    z = parentTile[i/2+px + (j/2+py)*n];
                } else {
                    float z0 = parentTile[i/2+px-1 + (j/2+py)*n];
                    float z1 = parentTile[i/2+px + (j/2+py)*n];
                    float z2 = parentTile[i/2+px+1 + (j/2+py)*n];
                    float z3 = parentTile[i/2+px+2 + (j/2+py)*n];
                    z = ((z1+z2)*9-(z0+z3))/16;
                }
            } else {
                if (i%2 == 0) {
                    float z0 = parentTile[i/2+px + (j/2-1+py)*n];
                    float z1 = parentTile[i/2+px + (j/2+py)*n];
                    float z2 = parentTile[i/2+px + (j/2+1+py)*n];
                    float z3 = parentTile[i/2+px + (j/2+2+py)*n];
                    z = ((z1+z2)*9-(z0+z3))/16;
                } else {
                    int di, dj;
                    z = 0.0;
                    for (dj = -1; dj <= 2; ++dj) {
                        float f = dj == -1 || dj == 2 ? -1/16.0f : 9/16.0f;
                        for (di = -1; di <= 2; ++di) {
                            float g = di == -1 || di == 2 ? -1/16.0f : 9/16.0f;
                            z += f*g*parentTile[i/2+di+px + (j/2+dj+py)*n];
                        }
                    }
                }
            }
            int off = i + j * n;
            result[off] = z;
        }
    }
}

void ResidualProducer::init(ptr<ResourceManager> manager, Resource *r, const string &name, ptr<ResourceDescriptor> desc, const TiXmlElement *e)
{
    e = e == NULL ? desc->descriptor : e;
    ptr<TileCache> cache;
    string file;
    int deltaLevel = 0;
    float zscale = 1.0;
    cache = manager->loadResource(r->getParameter(desc, e, "cache")).cast<TileCache>();
    if (e->Attribute("file") != NULL) {
        file = r->getParameter(desc, e, "file");
        file = manager->getLoader()->findResource(file);
    }
    if (e->Attribute("scale") != NULL) {
        r->getFloatParameter(desc, e, "scale", &zscale);
    }
    if (e->Attribute("delta") != NULL) {
        r->getIntParameter(desc, e, "delta", &deltaLevel);
    }
    init(cache, file.c_str(), deltaLevel, zscale);
    const TiXmlNode *n = e->FirstChild();
    while (n != NULL) {
        const TiXmlElement *f = n->ToElement();
        if (f != NULL) {
            if (strncmp(f->Value(), "residualProducer", 16) == 0) {
                addProducer(ResourceFactory::getInstance()->create(manager, f->Value(), desc, f).cast<ResidualProducer>());
            } else {
                if (Logger::ERROR_LOGGER != NULL) {
                    Resource::log(Logger::ERROR_LOGGER, desc, f, "Invalid subelement");
                }
                throw exception();
            }
        }
        n = n->NextSibling();
    }
}

class ResidualProducerResource : public ResourceTemplate<2, ResidualProducer>
{
public:
    ResidualProducerResource(ptr<ResourceManager> manager, const string &name, ptr<ResourceDescriptor> desc, const TiXmlElement *e = NULL) :
        ResourceTemplate<2, ResidualProducer>(manager, name, desc)
    {
        e = e == NULL ? desc->descriptor : e;
        checkParameters(desc, e, "name,cache,file,delta,scale,");
        init(manager, this, name, desc, e);
    }
};

extern const char residualProducer[] = "residualProducer";

static ResourceFactory::Type<residualProducer, ResidualProducerResource> ResidualProducerType;

}
