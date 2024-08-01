///*****************************************************************************************
// *                                                                                       *
// * OpenSpace                                                                             *
// *                                                                                       *
// * Copyright (c) 2014-2024                                                               *
// *                                                                                       *
// * Permission is hereby granted, free of charge, to any person obtaining a copy of this  *
// * software and associated documentation files (the "Software"), to deal in the Software *
// * without restriction, including without limitation the rights to use, copy, modify,    *
// * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to    *
// * permit persons to whom the Software is furnished to do so, subject to the following   *
// * conditions:                                                                           *
// *                                                                                       *
// * The above copyright notice and this permission notice shall be included in all copies *
// * or substantial portions of the Software.                                              *
// *                                                                                       *
// * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,   *
// * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A         *
// * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT    *
// * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF  *
// * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE  *
// * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                                         *
// ****************************************************************************************/

#include <modules/base/util/volumeslicer.h>

#include <ghoul/logging/logmanager.h>
#include <cmath>

namespace openspace {
    constexpr std::string_view _loggerCat = "CutPlaneSlicer";

VolumeSlicer::VolumeSlicer(std::filesystem::path path, std::string axis, std::string cutValue, std::string dataProperty)
{
    _volumeDimensions = glm::vec3(1.f, 1.f, 1.f);
    _data = { {{0.1f, 0.1f}, {0.2f,0.2f}}, {{0.3f, 0.3f}, {0.4f, 0.4f}} };

}
VolumeSlicer::VolumeSlicer(ccmc::Kameleon* kameleon, std::string axis, std::string cutValue, std::string dataProperty)
{
    //0. tests

    if (!kameleon->doesVariableExist(dataProperty)) {
        LERROR(std::format("'{}' does not exists in data volume", dataProperty));
    }

    LWARNING(std::format("Model name: '{}'", kameleon->getModelName()));
    LWARNING(std::format("Filename: '{}'", kameleon->getCurrentFilename()));
    int number = kameleon->getNumberOfVariables();
    LWARNING(std::format("Number of variables: '{}'", number));
    for (int i = 0; i < number; ++i) {
        LWARNING(std::format("Variable name: '{}'", kameleon->getVariableName(i)));
    }
    int globalnumber = kameleon->getNumberOfGlobalAttributes();
    LWARNING(std::format("Number of global variables: '{}'", globalnumber));
    for (int i = 0; i < globalnumber; ++i) {
        LWARNING(std::format("global variable name: '{}'", kameleon->getGlobalAttributeName(i)));
    }
    LWARNING(std::format("Number of variable attributes: '{}'", kameleon->getNumberOfVariableAttributes()));
    LWARNING(std::format("Current time: '{}'", kameleon->getCurrentTime().toString()));


    //1. read data
    std::vector<float>* data = kameleon->getVariable(dataProperty);
    int root = sqrt(data->size());
    for (int i = 0; i < root; ++i) {
        LWARNING(std::format("'{}', ", (*data)[i]));
    }
    //2. figure out which 2d data out of the 3d data is the plane

    //3. create and return texture with a function






    _volumeDimensions = glm::vec3(1.f, 1.f, 1.f);
    _data = { {{0.1f, 0.1f}, {0.2f,0.2f}}, {{0.3f, 0.3f}, {0.4f, 0.4f}} };

}


} // namespace openspace

