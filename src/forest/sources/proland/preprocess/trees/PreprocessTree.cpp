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

#include "proland/preprocess/trees/PreprocessTree.h"

#include "tiffio.h"

#include "ork/render/FrameBuffer.h"
#include "ork/ui/GlutWindow.h"

namespace proland
{

#define GRIDRES 128

const char *treeViewShader = "\
uniform vec3 dir;\n\
uniform sampler2D colorSampler;\n\
uniform sampler3D aoSampler;\n\
uniform mat4 worldToScreen;\n\
\n\
#ifdef _VERTEX_\n\
layout(location=0) in vec3 p;\n\
layout(location=1) in vec2 uv;\n\
out vec3 fp;\n\
out vec2 fuv;\n\
\n\
void main() {\n\
    gl_Position = worldToScreen * vec4(p, 1.0);\n\
    fp = p;\n\
    fuv = uv;\n\
}\n\
#endif\n\
#ifdef _FRAGMENT_\n\
in vec3 fp;\n\
in vec2 fuv;\n\
layout(location=0) out vec4 data;\n\
\n\
void main() {\n\
    if (fp.z < -1.0) {\n\
        discard;\n\
    }\n\
    if (texture(colorSampler, fuv).a < -0.25) {\n\
        discard;\n\
    }\n\
    float t = (dot(fp, dir) + sqrt(2.0)) / (2.0 * sqrt(2.0));\n\
    data = vec4(t, t, texture(aoSampler, fp * 0.5 + vec3(0.5)).r, 1.0);\n\
}\n\
#endif\n\
";

TreeMesh::TreeMesh(ptr< Mesh<Vertex, unsigned int> > mesh, ptr<Texture2D> texture) :
    mesh(mesh), texture(texture)
{
}

class PreprocessTree : public GlutWindow
{
public:
    vector<TreeMesh> tree;
    int n;
    int w;
    const char* output;

    ptr<Program> p;

    PreprocessTree(loadTreeMeshFunction loadTree, int n, int w, const char* output) :
        GlutWindow(Window::Parameters().size(w, w).depth(true).alpha(true)),
        n(n), w(w), output(output)
    {
        loadTree(tree);
        unsigned char *ao = computeAO();
        p = new Program(new Module(330, treeViewShader));
        p->getUniformSampler("aoSampler")->set(new Texture3D(GRIDRES, GRIDRES, GRIDRES, RGBA8,
            RGBA, UNSIGNED_BYTE, Texture::Parameters().min(LINEAR).mag(LINEAR), Buffer::Parameters(), CPUBuffer(ao)));
        delete[] ao;
    }

    void redisplay(double t, double dt)
    {
        printf("COMPUTING VIEWS...\n");
        ptr<FrameBuffer> fb = FrameBuffer::getDefault();
        fb->setPolygonMode(FILL, FILL);
        fb->setMultisample(true);
        fb->setSampleAlpha(true, true);
        fb->setViewport(vec4<GLint>(0, 0, w, w));
        fb->setDepthTest(true, LESS);

        int total = 2 * (n * n + n) + 1;
        unsigned char *buf = new unsigned char[w * w * total * 4];
        int current = 0;

        const float zmax = 1;
        const float zmin = -1;

        FILE *f;
        char name[256];
        sprintf(name, "%s/views.xml", output);
        fopen(&f, name, "w");

        for (int i = -n; i <= n; ++i) {
            for (int j = -n + abs(i); j <= n - abs(i); ++j) {

                printf("VIEW %d of %d\n", current, total);

                float x = (i + j) / float(n);
                float y = (j - i) / float(n);
                float angle = 90.0 - std::max(fabs(x),fabs(y)) * 90.0;
                float alpha = x == 0.0 && y == 0.0 ? 0.0 : atan2(y, x) / M_PI * 180.0;

                mat4f cameraToWorld = mat4f::rotatex(90) * mat4f::rotatex(-angle);
                mat4f worldToCamera = cameraToWorld.inverse();

                box3f b;
                b = b.enlarge((worldToCamera * vec4f(-1.0, -1.0, zmin, 1.0)).xyz());
                b = b.enlarge((worldToCamera * vec4f(+1.0, -1.0, zmin, 1.0)).xyz());
                b = b.enlarge((worldToCamera * vec4f(-1.0, +1.0, zmin, 1.0)).xyz());
                b = b.enlarge((worldToCamera * vec4f(+1.0, +1.0, zmin, 1.0)).xyz());
                b = b.enlarge((worldToCamera * vec4f(-1.0, -1.0, zmax, 1.0)).xyz());
                b = b.enlarge((worldToCamera * vec4f(+1.0, -1.0, zmax, 1.0)).xyz());
                b = b.enlarge((worldToCamera * vec4f(-1.0, +1.0, zmax, 1.0)).xyz());
                b = b.enlarge((worldToCamera * vec4f(+1.0, +1.0, zmax, 1.0)).xyz());
                mat4f c2s = mat4f::orthoProjection(b.xmax, b.xmin, b.ymax, b.ymin, -2.0 * b.zmax, -2.0 * b.zmin);
                mat4f w2s = c2s * worldToCamera * mat4f::rotatez(-90.0 - alpha);
                vec3f dir = ((mat4f::rotatez(90.0 + alpha) * cameraToWorld) * vec4f(0.0, 0.0, 1.0, 0.0)).xyz();

                p->getUniformMatrix4f("worldToScreen")->setMatrix(w2s);
                p->getUniform3f("dir")->set(dir);

                fb->clear(true, false, true);

                fb->setColorMask(true, false, true, true);
                vector<TreeMesh>::iterator k = tree.begin();
                while (k != tree.end()) {
                    p->getUniformSampler("colorSampler")->set(k->texture);
                    fb->draw(p, *(k->mesh));
                    k++;
                }

                fb->setColorMask(false, true, false, false);
                fb->setDepthTest(true, GREATER);
                fb->setClearDepth(0.0);
                fb->clear(false, false, true);
                k = tree.begin();
                while (k != tree.end()) {
                    p->getUniformSampler("colorSampler")->set(k->texture);
                    fb->draw(p, *(k->mesh));
                    k++;
                }

                fb->setClearDepth(1.0);
                fb->setDepthTest(true, LESS);
                fb->setColorMask(true, true, true, true);

                int view = i * (1 - abs(i)) + j + 2 * n * i + n * (n + 1);
                assert(view == current);
                current++;
                fprintf(f, "    <uniformMatrix3f name=\"views[%d]\" value=\"%f,%f,%f,%f,%f,%f,%f,%f,%f\"/>\n",
                       view,
                       w2s[0][0], w2s[0][1], w2s[0][2],
                       w2s[1][0], w2s[1][1], w2s[1][2],
                       w2s[2][0], w2s[2][1], w2s[2][2]);

                fb->readPixels(0, 0, w, w, RGBA, UNSIGNED_BYTE, Buffer::Parameters(), CPUBuffer(buf + view * w * w * 4));
                clearBorder(buf + view * w * w *4);
            }
        }
        fclose(f);

        sprintf(name, "%s/treeViews.tiff", output);
        TIFF* out = TIFFOpen(name, "wb");
        TIFFSetField(out, TIFFTAG_IMAGEWIDTH, w);
        TIFFSetField(out, TIFFTAG_IMAGELENGTH, w * total);
        TIFFSetField(out, TIFFTAG_SAMPLESPERPIXEL, 4);
        TIFFSetField(out, TIFFTAG_BITSPERSAMPLE, 8);
        TIFFSetField(out, TIFFTAG_COMPRESSION, COMPRESSION_DEFLATE);
        TIFFSetField(out, TIFFTAG_ORIENTATION, ORIENTATION_BOTLEFT);
        TIFFSetField(out, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
        TIFFSetField(out, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
        TIFFWriteEncodedStrip(out, 0, buf, w * w * total * 4);
        TIFFClose(out);
        delete[] buf;

        printf("VIEWS DONE.\n");
        ::exit(0);
    }

    void clearBorder(unsigned char *buf)
    {
        for (int i = 0; i < w; ++i) {
            for (int c = 0; c < 4; ++c) {
                buf[4*i+c] = 0;
                buf[4*i*w+c] = 0;
                buf[4*(i*w+w-1)+c] = 0;
                buf[4*(i+(w-1)*w)+c] = 0;
            }
        }
    }

    bool full(unsigned char *b, int i, int j, int k)
    {
        if (i < 0 || i >= GRIDRES || j < 0 || j >= GRIDRES || k < 0 || k >= GRIDRES) {
            return false;
        }
        return b[4*(i + j * GRIDRES + k * GRIDRES * GRIDRES)+3] != 0;
    }

    unsigned char *computeAO()
    {
        printf("COMPUTING AMBIENT OCCLUSION...\n");
        unsigned char *buf = new unsigned char[GRIDRES*GRIDRES*GRIDRES*4];
        for (int i = 0; i < GRIDRES; ++i) {
            for (int j = 0; j < GRIDRES; ++j) {
                for (int k = 0; k < GRIDRES; ++k) {
                    int off = i + j * GRIDRES + k * GRIDRES * GRIDRES;
                    buf[4*off] = 0;
                    buf[4*off+1] = 0;
                    buf[4*off+2] = 0;
                    buf[4*off+3] = 0;
                }
            }
        }

        vector<TreeMesh>::const_iterator ti = tree.begin();
        while (ti != tree.end()) {
            ptr< Mesh<TreeMesh::Vertex, unsigned int> > m = ti->mesh;
            ti++;

            for (int in = 0; in < m->getIndiceCount(); in += 3) {
                int a = m->getIndice(in);
                int b = m->getIndice(in+1);
                int c = m->getIndice(in+2);
                float x1 = m->getVertex(a).pos.x, y1 = m->getVertex(a).pos.y, z1 = m->getVertex(a).pos.z;
                float x2 = m->getVertex(b).pos.x, y2 = m->getVertex(b).pos.y, z2 = m->getVertex(b).pos.z;
                float x3 = m->getVertex(c).pos.x, y3 = m->getVertex(c).pos.y, z3 = m->getVertex(c).pos.z;
                x1 = (x1 + 1.0) / 2.0;
                x2 = (x2 + 1.0) / 2.0;
                x3 = (x3 + 1.0) / 2.0;
                y1 = (y1 + 1.0) / 2.0;
                y2 = (y2 + 1.0) / 2.0;
                y3 = (y3 + 1.0) / 2.0;
                z1 = (z1 + 1.0) / 2.0;
                z2 = (z2 + 1.0) / 2.0;
                z3 = (z3 + 1.0) / 2.0;
                float l12 = sqrt((x2-x1)*(x2-x1)+(y2-y1)*(y2-y1)+(z2-z1)*(z2-z1));
                float l23 = sqrt((x3-x2)*(x3-x2)+(y3-y2)*(y3-y2)+(z3-z2)*(z3-z2));
                float l31 = sqrt((x1-x3)*(x1-x3)+(y1-y3)*(y1-y3)+(z1-z3)*(z1-z3));
                if (l12 > l23 && l12 > l31) {
                    std::swap(a, c);
                    std::swap(x1, x3); std::swap(y1, y3); std::swap(z1, z3);
                    std::swap(l12, l23);
                } else if (l31 > l12 && l31 > l23) {
                    std::swap(a, b);
                    std::swap(x1, x2); std::swap(y1, y2); std::swap(z1, z2);
                    std::swap(l31, l23);
                }

                int n12 = int(ceil(l12 * GRIDRES) * 2.0);
                int n13 = int(ceil(l31 * GRIDRES) * 2.0);
                for (int i = 0; i <= n12; ++i) {
                    float u = float(i) / n12;
                    for (int j = 0; j <= n13; ++j) {
                        float v = float(j) / n13;
                        if (u + v < 1.0) {
                            float x = x1 + u * (x2 - x1) + v * (x3 - x1);
                            float y = y1 + u * (y2 - y1) + v * (y3 - y1);
                            float z = z1 + u * (z2 - z1) + v * (z3 - z1);
                            int ix = int(x * GRIDRES);
                            int iy = int(y * GRIDRES);
                            int iz = int(z * GRIDRES);
                            if (ix >= 0 && ix < GRIDRES && iy >= 0 && iy < GRIDRES && iz >= 0 && iz < GRIDRES) {
                                int off = 4*(ix + iy * GRIDRES + iz * GRIDRES * GRIDRES);
                                buf[off] = 255;
                                buf[off+1] = 255;
                                buf[off+2] = 255;
                                buf[off+3] = 255;
                            }
                        }
                    }
                }
            }
        }

        float *vocc = new float[GRIDRES*GRIDRES*GRIDRES];
        for (int i = 0; i < GRIDRES*GRIDRES*GRIDRES; ++i) {
            vocc[i] = 1.0;
        }
        const int N = 8;
        for (int i = 0; i < N; ++i) {
            float theta = (i + 0.5) / N * M_PI / 2.0;
            float dtheta = 1.0 / N * M_PI / 2.0;
            for (int j = 0; j < 4*N; ++j) {
                float phi = (j + 0.5) / (4 * N) * 2.0 * M_PI;
                float dphi = 1.0 / (4 * N) * 2.0 * M_PI;
                float docc = cos(theta) * sin(theta) * dtheta * dphi / M_PI;
                printf("STEP %d of %d\n", i * 4*N + j, 4*N*N);

                vec3f uz = vec3f(cos(phi)*sin(theta),sin(phi)*sin(theta),cos(theta));
                vec3f ux = uz.z == 1.0 ? vec3f(1.0, 0.0, 0.0) : vec3f(-uz.y,uz.x,0.0).normalize();
                vec3f uy = uz.crossProduct(ux);
                mat3f toView = mat3f(ux.x, ux.y, ux.z, uy.x, uy.y, uy.z, uz.x, uz.y, uz.z);
                mat3f toVol = mat3f(ux.x, uy.x, uz.x, ux.y, uy.y, uz.y, ux.z, uy.z, uz.z);
                box3f b;
                b = b.enlarge(toView * vec3f(-1.0, -1.0, -1.0));
                b = b.enlarge(toView * vec3f(+1.0, -1.0, -1.0));
                b = b.enlarge(toView * vec3f(-1.0, +1.0, -1.0));
                b = b.enlarge(toView * vec3f(+1.0, +1.0, -1.0));
                b = b.enlarge(toView * vec3f(-1.0, -1.0, +1.0));
                b = b.enlarge(toView * vec3f(+1.0, -1.0, +1.0));
                b = b.enlarge(toView * vec3f(-1.0, +1.0, +1.0));
                b = b.enlarge(toView * vec3f(+1.0, +1.0, +1.0));
                int nx = int((b.xmax - b.xmin) * GRIDRES / 2);
                int ny = int((b.ymax - b.ymin) * GRIDRES / 2);
                int nz = int((b.zmax - b.zmin) * GRIDRES / 2);
                int* occ = new int[nx*ny*nz];
                for (int i = 0; i < nx*ny*nz; ++i) {
                    occ[i] = 0;
                }
                for (int iz = nz - 1; iz >= 0; --iz) {
                    float z = b.zmin + (iz + 0.5) / nz * (b.zmax - b.zmin);
                    for (int iy = 0; iy < ny; ++iy) {
                        float y = b.ymin + (iy + 0.5) / ny * (b.ymax - b.ymin);
                        for (int ix = 0; ix < nx; ++ix) {
                            float x = b.xmin + (ix + 0.5) / nx * (b.xmax - b.xmin);
                            int val = 0;
                            vec3f p = toVol * vec3f(x, y, z);
                            int vx = int((p.x + 1.0) / 2.0 * GRIDRES);
                            int vy = int((p.y + 1.0) / 2.0 * GRIDRES);
                            int vz = int((p.z + 1.0) / 2.0 * GRIDRES);
                            if (vx >= 0 && vx < GRIDRES && vy >= 0 && vy < GRIDRES && vz >= 0 && vz < GRIDRES) {
                                val = buf[4*(vx + vy * GRIDRES + vz * GRIDRES * GRIDRES)+3] == 255 ? 1 : 0;
                            }
                            occ[ix+iy*nx+iz*nx*ny] = val;
                            if (iz != nz-1) {
                                occ[ix+iy*nx+iz*nx*ny] += occ[ix+iy*nx+(iz+1)*nx*ny];
                            }
                        }
                    }
                }
                for (int i = 0; i < GRIDRES; ++i) {
                    float x = -1.0 + (i + 0.5) / GRIDRES * 2.0;
                    for (int j = 0; j < GRIDRES; ++j) {
                        float y = -1.0 + (j + 0.5) / GRIDRES * 2.0;
                        for (int k = 0; k < GRIDRES; ++k) {
                            float z = -1.0 + (k + 0.5) / GRIDRES * 2.0;
                            vec3f p = toView * vec3f(x, y, z);
                            int vx = int((p.x - b.xmin) / (b.xmax - b.xmin) * nx);
                            int vy = int((p.y - b.ymin) / (b.ymax - b.ymin) * ny);
                            int vz = int((p.z - b.zmin) / (b.zmax - b.zmin) * nz);
                            if (vx >= 0 && vx < nx && vy >= 0 && vy < ny && vz >= 0 && vz < nz) {
                                int n = occ[vx+vy*nx+vz*nx*ny];
                                if (n > 6) {
                                    vocc[i + j * GRIDRES + k * GRIDRES * GRIDRES] -= docc;
                                }
                            }
                        }
                    }
                }
                delete[] occ;
            }
        }

        for (int i = 0; i < GRIDRES; ++i) {
            for (int j = 0; j < GRIDRES; ++j) {
                for (int k = 0; k < GRIDRES; ++k) {
                    int off = i + j * GRIDRES + k * GRIDRES * GRIDRES;
                    if (buf[4*off+3] == 255) {
                        int v = max(vocc[off], 0.0f) * 255;
                        buf[4*off] = v;
                        buf[4*off+1] = v;
                        buf[4*off+2] = v;
                    }
                }
            }
        }

        printf("AMBIENT OCCLUSION DONE.\n");
        return buf;
    }

    static static_ptr<Window> app;
};

static_ptr<Window> PreprocessTree::app;

PROLAND_API void preprocessTree(loadTreeMeshFunction loadTree, int n, int w, const char *output)
{
    PreprocessTree::app = new PreprocessTree(loadTree, n, w, output);
    PreprocessTree::app->start();
}

set<int> combinations[9][9];

void computeCombinations()
{
    for (int n = 0; n < 9; ++n) {
        combinations[n][0].insert(0);
    }
    for (int n = 1; n < 9; ++n) {
        for (int k = 1; k <= n; ++k) {
            set<int>::iterator i = combinations[n - 1][k - 1].begin();
            while (i != combinations[n - 1][k - 1].end()) {
                int v = *i;
                combinations[n][k].insert(v | (1 << (n-1)));
                i++;
            }
            i = combinations[n - 1][k].begin();
            while (i != combinations[n - 1][k].end()) {
                int v = *i;
                combinations[n][k].insert(v);
                i++;
            }
        }
    }
}

int getCombination(int n, int k, int index)
{
    index = index % combinations[n][k].size();
    set<int>::iterator it = combinations[n][k].begin();
    for (int i = 0; i < index; ++i) {
        it++;
    }
    return *it;
}

void preprocessMultisample(const char *output)
{
    computeCombinations();
    int w = 9;
    int h = 70;
    unsigned char *buf = new unsigned char[w*h];
    for (int i = 0; i < w; ++i) {
        for (int j = 0; j < h; ++j) {
            int v = getCombination(8, i, j);
            buf[i + j * w] = v;
        }
    }

    TIFF* out = TIFFOpen(output, "wb");
    TIFFSetField(out, TIFFTAG_IMAGEWIDTH, w);
    TIFFSetField(out, TIFFTAG_IMAGELENGTH, h);
    TIFFSetField(out, TIFFTAG_SAMPLESPERPIXEL, 1);
    TIFFSetField(out, TIFFTAG_BITSPERSAMPLE, 8);
    TIFFSetField(out, TIFFTAG_COMPRESSION, COMPRESSION_DEFLATE);
    TIFFSetField(out, TIFFTAG_ORIENTATION, ORIENTATION_BOTLEFT);
    TIFFSetField(out, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(out, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
    TIFFWriteEncodedStrip(out, 0, buf, w * h);
    TIFFClose(out);
}

}
