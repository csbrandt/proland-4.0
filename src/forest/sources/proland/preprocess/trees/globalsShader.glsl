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

const char *globalsShaderSource = "\
#include \"globalsShader.glhl\"\n\
\n\
uniform globals {\n\
    vec3 worldCameraPos;\n\
    vec3 worldSunDir;\n\
    float minRadius;\n\
    float maxRadius;\n\
    float treeTau;\n\
    float treeHeight;\n\
    float treeDensity;\n\
    int nViews;\n\
    float maxTreeDistance;\n\
    vec4 shadowLimit;\n\
    mat4 tangentFrameToShadow[4];\n\
};\n\
\n\
vec3 getWorldCameraPos() {\n\
    return worldCameraPos;\n\
}\n\
\n\
vec3 getWorldSunDir() {\n\
    return worldSunDir;\n\
}\n\
\n\
float getMinRadius() {\n\
    return minRadius;\n\
}\n\
\n\
float getMaxRadius() {\n\
    return maxRadius;\n\
}\n\
\n\
float getTreeTau() {\n\
    return treeTau;\n\
}\n\
\n\
float getTreeHeight() {\n\
    return treeHeight;\n\
}\n\
\n\
float getTreeDensity() {\n\
    return treeDensity;\n\
}\n\
\n\
int getNViews() {\n\
    return nViews;\n\
}\n\
\n\
float getMaxTreeDistance() {\n\
    return maxTreeDistance;\n\
}\n\
\n\
float getShadowLimit(int i)\n\
{\n\
    return shadowLimit[i];\n\
}\n\
\n\
mat4 getTangentFrameToShadow(int i)\n\
{\n\
    return tangentFrameToShadow[i];\n\
}\n\
";
