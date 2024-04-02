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

#include <modules/base/rendering/renderablecutplane.h>
#include <modules/kameleonvolume/kameleonvolumereader.h>
#include <ghoul/filesystem/filesystem.h>
#include <filesystem>

namespace {
constexpr openspace::properties::Property::PropertyInfo FilePathInfo = {
    "FilePath",
    "Filepath to the file to create texture from",
    " ",
    openspace::properties::Property::Visibility::User
};

constexpr openspace::properties::Property::PropertyInfo DataPropertyInfo = {
        "DataProperty",
        "Name of the data property",
        "Data property to color the cutplane by",
        openspace::properties::Property::Visibility::User
};

struct [[codegen::Dictionary(RenderableCutPlane)]] Parameters {
    // [[codegen::verbatim(FilePathInfo.description)]]
    std::filesystem::path input;
    // [[codegen::verbatim(DataPropertyInfo.description)]]
    std::string dataProperty;
};
#include "renderablecutplane_codegen.cpp"
} // namespace


namespace openspace {

documentation::Documentation RenderableCutPlane::Documentation() {
    return codegen::doc<Parameters>(
        "base_renderablecutplane",
        RenderablePlane::Documentation()
    );
}

RenderableCutPlane::RenderableCutPlane(const ghoul::Dictionary& dictionary)
    : RenderablePlane(dictionary)
{
    const Parameters p = codegen::bake<Parameters>(dictionary);

    _inputPath = absPath(p.input.string());
    _dataProperty = p.dataProperty;
}

void RenderableCutPlane::initialize() {

}

void RenderableCutPlane::initializeGL() {

    //kameleonvolume::KameleonVolumeReader volumeReader(_inputPath.string());

    std::string _path = "C:/Users/alundkvi/Documents/work/data/Tracing/cdf/3d__var_1_e20230323-000000-000.out.cdf";

    std::unique_ptr<ccmc::Kameleon> kameleon = kameleonHelper::createKameleonObject(
        _inputPath.string()
    );
    

    if (!std::filesystem::is_regular_file(
        _inputPath)) {
        throw ghoul::FileNotFoundError(
            _inputPath.string());
    }

    long status = kameleon->open(_inputPath.string());
    if (status != ccmc::FileReader::OK) {
        throw ghoul::RuntimeError(fmt::format(
            "Failed to open file '{}' with Kameleon",
            _inputPath
        ));
    }

    std::cout << "Model name: " << kameleon->getModelName() << std::endl;
    std::cout << "Filename: " << kameleon->getCurrentFilename() << std::endl;
    std::cout << "Number of variables: " << kameleon->getNumberOfVariables() << std::endl;
    std::cout << "Number of variable attributes: " << kameleon->getNumberOfVariableAttributes() << std::endl;
    std::cout << "Current time: " << kameleon->getCurrentTime() << std::endl;


    //long status;
    //status = _kameleon->open("C:/Users/alundkvi/Documents/work/data/Tracing/cdf/3d__var_1_e20230323-000000-000.out.cdf");

    // Load cdf file
    //Extract slice from data
}

void RenderableCutPlane::deinitializeGL() {

    //RenderablePlane::deinitializeGL();
}

void RenderableCutPlane::render(const RenderData& data, RendererTasks& t) {

}

void RenderableCutPlane::update(const UpdateData& data) {

}

} // namespace openspace
