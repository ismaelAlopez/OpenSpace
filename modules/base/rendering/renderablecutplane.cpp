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

#include <filesystem>


namespace {
constexpr openspace::properties::Property::PropertyInfo AxisInfo = {
    "Axis",
    "The x, y or z axis",
    "Axis to cut the volume on",
    openspace::properties::Property::Visibility::User
};
constexpr openspace::properties::Property::PropertyInfo CutValueInfo = {
    "CutValue",
    "A value within the volume dimension",
    "A value to cut the plane on within the dimension of the selected axis",
    openspace::properties::Property::Visibility::User
};
constexpr openspace::properties::Property::PropertyInfo ColorTablePathsInfo = {
    "ColorTablePaths",
    "A local varibale of a local color transfer function",
    "A list of paths to transferfunction .txt files containing color tables used for colorizing the cutplane according to different data properties",
    openspace::properties::Property::Visibility::User
};
constexpr openspace::properties::Property::PropertyInfo ColorTableRangesInfo = {
    "ColorTableRanges",
    "Values of a range",
    "List of ranges for which their corresponding data property values will be colorized by. Should be entered as {min value, max value} per range",
    openspace::properties::Property::Visibility::User
};
constexpr openspace::properties::Property::PropertyInfo DataPropertyInfo = {
    "DataProperty",
    "Name of the data property",
    "Data property to color the cutplane by",
    openspace::properties::Property::Visibility::User
};
constexpr openspace::properties::Property::PropertyInfo FilePathInfo = {
    "FilePath",
    "Filepath to the file to create texture from",
    " ",
    openspace::properties::Property::Visibility::User
};
constexpr openspace::properties::Property::PropertyInfo SizeInfo = {
    "Size",
    "Size (in meters)",
    "This value specifies the size unit",
    openspace::properties::Property::Visibility::User
};

struct [[codegen::Dictionary(RenderableCutPlane)]] Parameters {
    // [[codegen::verbatim(AxisInfo.description)]]
    std::string axis;
    // [[codegen::verbatim(CutValueInfo.description)]]
    float cutValue;
    // [[codegen::verbatim(DataPropertyInfo.description)]]
    std::string dataProperty;
    // [[codegen::verbatim(FilePathInfo.description)]]
    std::string filePath;
    // [[codegen::verbatim(SizeInfo.description)]]
    std::variant<float, glm::vec3> size;
    // [[codegen::verbatim(ColorTablePathsInfo.description)]]
    std::optional<std::vector<std::string>> colorTablePaths;
    // [[codegen::verbatim(ColorTableRangesInfo.description)]]
    std::optional<std::vector<glm::vec2>> colorTableRanges;
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

}

void RenderableCutPlane::initialize() {

}

void RenderableCutPlane::initializeGL() {

    if (!std::filesystem::is_regular_file(
        "C:/Users/alundkvi/Documents/work/data/Tracing/cdf/3d__var_1_e20230323-000000-000.out.cdf")) {
        throw ghoul::FileNotFoundError(
            "C:/Users/alundkvi/Documents/work/data/Tracing/cdf/3d__var_1_e20230323-000000-000.out.cdf");
    }

    const long status = _kameleon->open("C:/Users/alundkvi/Documents/work/data/Tracing/cdf/3d__var_1_e20230323-000000-000.out.cdf");
    if (status != ccmc::FileReader::OK) {
        throw ghoul::RuntimeError(fmt::format(
            "Failed to open file '{}' with Kameleon",
            "C:/Users/alundkvi/Documents/work/data/Tracing/cdf/3d__var_1_e20230323-000000-000.out.cdf"
        ));
    }

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
