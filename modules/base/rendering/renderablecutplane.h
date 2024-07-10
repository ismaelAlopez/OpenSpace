/*****************************************************************************************
 *                                                                                       *
 * OpenSpace                                                                             *
 *                                                                                       *
 * Copyright (c) 2014-2024                                                               *
 *                                                                                       *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this  *
 * software and associated documentation files (the "Software"), to deal in the Software *
 * without restriction, including without limitation the rights to use, copy, modify,    *
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to    *
 * permit persons to whom the Software is furnished to do so, subject to the following   *
 * conditions:                                                                           *
 *                                                                                       *
 * The above copyright notice and this permission notice shall be included in all copies *
 * or substantial portions of the Software.                                              *
 *                                                                                       *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,   *
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A         *
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT    *
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF  *
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE  *
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                                         *
 ****************************************************************************************/

#ifndef __OPENSPACE_MODULE_BASE___RENDERABLECUTPLANE___H__
#define __OPENSPACE_MODULE_BASE___RENDERABLECUTPLANE___H__


#include <openspace/rendering/renderable.h>
#include <modules/base/rendering/renderableplane.h>
#include <modules/kameleonvolume/kameleonvolumereader.h>
#include <modules/kameleon/ext/kameleon/src/ccmc/FileReader.h>
#include <modules/kameleon/ext/kameleon/src/ccmc/GeneralFileReader.h>
#include <modules/kameleon/ext/kameleon/src/ccmc/Kameleon.h>
#include <modules/kameleon/ext/kameleon/src/ccmc/Interpolator.h>
#include <modules/kameleon/include/kameleonhelper.h>


#include <modules/base/basemodule.h>
#include <openspace/documentation/documentation.h>
#include <openspace/documentation/verifier.h>


#include <ghoul/glm.h>
#include <memory>
#include <string>
#include <vector>
#include <iostream>

namespace ccmc {
    class Attribute;
    class Interpolator;
    class Kameleon;
}

namespace ghoul::filesystem { class File; }
namespace ghoul::opengl { class Texture; }

namespace openspace {

struct RenderData;
struct UpdateData;

namespace documentation { struct Documentation; }

class RenderableCutPlane : public RenderablePlane {
public:
    RenderableCutPlane(const ghoul::Dictionary& dictionary);

    void initialize() override;
    void initializeGL() override;
    void deinitializeGL() override;

    void update(const UpdateData& data) override;
    void render(const RenderData& data, RendererTasks&) override;

    static documentation::Documentation Documentation();

protected:

private:
    std::filesystem::path _inputPath;
    // What data property to render
    std::string _dataProperty;

    std::unique_ptr<ccmc::Kameleon> _kameleon;
    std::unique_ptr<ccmc::Interpolator> _interpolator;
};

} // namespace openspace

#endif //__OPENSPACE_MODULE_BASE___RENDERABLECUTPLANE___H__