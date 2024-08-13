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
#include <modules/kameleon/ext/kameleon/src/ccmc/KameleonInterpolator.h>
#include <ghoul/filesystem/filesystem.h>
#include <ghoul/logging/logmanager.h>
#include <filesystem>

namespace {
constexpr std::string_view _loggerCat = "RenderableCutPlane";

//constexpr openspace::properties::Property::PropertyInfo FilePathInfo = {
//    "FilePath",
//    "Filepath to the file to create texture from",
//    " ",
//    openspace::properties::Property::Visibility::User
//};
constexpr openspace::properties::Property::PropertyInfo DataPropertiesInfo = {
    "DataProperties",
    "Name of the data property",
    "Data property to color the cutplane by",
    openspace::properties::Property::Visibility::User
};
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
    "A list of paths to transferfunction .txt files containing color tables used for "
    "colorizing the cutplane according to different data properties",
    openspace::properties::Property::Visibility::User
};
constexpr openspace::properties::Property::PropertyInfo ColorTableRangesInfo = {
    "ColorTableRanges",
    "Values of a range",
    "List of ranges for which their corresponding data property values will be colorized "
    "by. Should be entered as {min value, max value} per range",
    openspace::properties::Property::Visibility::User
};

struct [[codegen::Dictionary(RenderableCutPlane)]] Parameters {
    // Filepath to the file to create texture from
    std::filesystem::path input;
    // Data property to color the cutplane by
    std::string dataProperty;
    // [[codegen::verbatim(AxisInfo.description)]]
    std::string axis;
    // [[codegen::verbatim(CutValueInfo.description)]]
    float cutValue;
    // Resolution of the cut plane. Default is 2 by 2 pixels
    std::optional<glm::vec2> resolution;
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
    , _dataProperties(DataPropertiesInfo, properties::OptionProperty::DisplayType::Dropdown)

{
    const Parameters p = codegen::bake<Parameters>(dictionary);

    _inputPath = absPath(p.input);
    //dataproperties will need an onchange
    _dataProperty = p.dataProperty;
    //so will axis
    _axis = p.axis;
    // and cutvalue, if not a full plane equation is entered some other way
    _cutValue = p.cutValue;

    _resolution = p.resolution.value_or(_resolution);

    if (p.colorTablePaths.has_value()) {
        _colorTablePaths = p.colorTablePaths.value();
    }
    if (p.colorTableRanges.has_value()) {
        _colorTableRanges = *p.colorTableRanges;
    }
    else {
        _colorTableRanges.push_back(glm::vec2(0.f, 1.f));
    }

    if (_inputPath.extension() == ".cdf") {
        readCdfFile();
    }
    if (_inputPath.extension() == ".h5") {
        readh5File();
    }

}

void RenderableCutPlane::initialize() {
    if (!std::filesystem::is_regular_file(_inputPath)) {
        throw ghoul::FileNotFoundError(_inputPath.string());
    }

    // Load cdf file
    //Extract slice from data
}

void RenderableCutPlane::readCdfFile() {
    std::unique_ptr<ccmc::Kameleon> kameleon = kameleonHelper::createKameleonObject(
        _inputPath.string()
    );
    long status = kameleon->open(_inputPath.string());
    if (status != ccmc::FileReader::OK) {
        throw ghoul::RuntimeError(std::format(
            "Failed to open file '{}' with Kameleon",
            _inputPath
        ));
    }



    _slicer = VolumeSlicer(_inputPath, _axis, _cutValue, _dataProperty);
    _slicer = VolumeSlicer(kameleon.get(), _axis, _cutValue, _dataProperty);

    //_slicer.dimension;
    //_slicer.origin;
    //_slizer.axis;
    //_slizer.cutvalue;
    //vec<vec<float>> planeData= _slicer.data;
    //Texture texture = (planeData, TF);


}
void RenderableCutPlane::readh5File() {
    //read file
    //call loaddatafromH5slice()
    //create texture
}

void RenderableCutPlane::loadDataFromCDFSlice() {



}

void RenderableCutPlane::initializeGL() {

}

void RenderableCutPlane::deinitializeGL() {

    //RenderablePlane::deinitializeGL();
}

void RenderableCutPlane::render(const RenderData& data, RendererTasks& task) {

}

void RenderableCutPlane::update(const UpdateData& data) {

}

} // namespace openspace
