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

#ifndef _PROLAND_TWEAKSCENEGRAPH_H_
#define _PROLAND_TWEAKSCENEGRAPH_H_

#include "ork/math/box2.h"
#include "ork/render/Mesh.h"
#include "ork/render/Uniform.h"
#include "ork/scenegraph/SceneNode.h"
#include "proland/ui/twbar/TweakBarHandler.h"

using namespace ork;

namespace proland
{

/**
 * A TweakBarHandler to control the scene graph.
 * @ingroup twbar
 * @authors Eric Bruneton, Antoine Begault
 */
PROLAND_API class TweakSceneGraph : public TweakBarHandler
{
public:
    struct TextureInfo
    {
    public:
        ptr<Texture> tex;

        int level;

        int mode;

        vec4f norm;

        box2f area;

        TextureInfo(ptr<Texture> t) : tex(t), level(0), mode(1),
            norm(1.0f, 1.0f, 1.0f, 1.0f), area(0.0f, 1.0f, 0.0f, 1.0f)
        {
        }
    };

    /**
     * Creates a new TweakSceneGraph.
     *
     * @param scene the root of the scene graph to control.
     * @param active true if this TweakBarHandler must be initialy active.
     */
    TweakSceneGraph(ptr<SceneNode> scene, bool active);

    /**
     * Deletes this TweakSceneGraph.
     */
    virtual ~TweakSceneGraph();

    int getCurrentTexture() const;

    int getCurrentLevel() const;

    int getCurrentMode() const;

    box2f getCurrentArea() const;

    vec4f getCurrentNorm() const;

    void setCurrentTexture(int tex);

    void setCurrentLevel(int level);

    void setCurrentMode(int mode);

    void setCurrentArea(const box2f &area);

    void setCurrentNorm(const vec4f &n);

    virtual void redisplay(double t, double dt, bool &needUpdate);

    virtual bool mouseClick(EventHandler::button b, EventHandler::state s, EventHandler::modifier m, int x, int y, bool &needUpdate);

    virtual bool mouseWheel(EventHandler::wheel b, EventHandler::modifier m, int x, int y, bool &needUpdate);

    virtual bool mouseMotion(int x, int y, bool &needUpdate);

    virtual bool mousePassiveMotion(int x, int y, bool &needUpdate);

protected:
    /**
     * Creates an uninitialized TweakSceneGraph.
     */
    TweakSceneGraph();

    /**
     * Initializes this TweakSceneGraph.
     * See #TweakSceneGraph.
     */
    virtual void init(ptr<SceneNode> scene, bool active);

    virtual void updateBar(TwBar *bar);

    void swap(ptr<TweakSceneGraph> o);

private:
    enum MODE
    {
        NONE,
        MOVING,
        MOVING_UV,
        ZOOMING
    };

    /**
     * The root of the scene graph to control.
     */
    ptr<SceneNode> scene;

    std::map<std::string, TextureInfo> textures;

    int currentTexture;

    TextureInfo *currentInfo;

    box2f displayBox;

    ptr<Program> renderProg;
    ptr<UniformSampler> renderTexture1DU;
    ptr<UniformSampler> renderTexture2DU;
    ptr<UniformSampler> renderTexture2DArrayU;
    ptr<UniformSampler> renderTexture3DU;
    ptr<Uniform1f> renderTypeU;
    ptr<Uniform1f> renderLevelU;
    ptr<Uniform1f> renderModeU;
    ptr<Uniform4f> renderNormU;
    ptr<Uniform4f> renderPositionU;
    ptr<Uniform4f> renderCoordsU;
    ptr<Uniform3i> renderGridU;

    ptr<Program> selectProg;
    ptr<Uniform4f> selectPositionU;

    MODE mode;

    vec2f lastPos;

    vec2f newPos;
};

}

#endif
