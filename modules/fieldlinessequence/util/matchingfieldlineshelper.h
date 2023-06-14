/*****************************************************************************************
 *                                                                                       *
 * OpenSpace                                                                             *
 *                                                                                       *
 * Copyright (c) 2014-2022                                                               *
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

#ifndef __OPENSPACE_MODULE_FIELDLINESSEQUENCE___MATCHINGFIELDLINEHELPER___H__
#define __OPENSPACE_MODULE_FIELDLINESSEQUENCE___MATCHINGFIELDLINEHELPER___H__

 #include <ghoul/glm.h>
 #include <string>
 #include <vector>

//fel att göra så
#include <ghoul/logging/logmanager.h>
#include <ccmc/Kameleon.h>


namespace openspace {

    class FieldlinesState;

    struct Seedpoint {
        glm::vec3 seedPoint;
        std::string topology;
        glm::vec3 criticalPoint;
    };

    namespace fls {
        bool convertCdfToMatchingFieldlinesState(
            FieldlinesState& state,
            ccmc::Kameleon* kameleon,
            std::vector<Seedpoint>& seedPoints,
            const std::vector<double>& birthTimes,
            double manualTimeOffset,
            const std::string& tracingVar,
            std::vector<std::string>& extraVars,
            std::vector<std::string>& extraMagVars,
            const size_t nPointsOnPathLine,
            const size_t nPointsOnFieldLines);

        std::vector <Seedpoint> validateAndModifyAllSeedPoints(
            std::vector<Seedpoint>& seedPoints,
            const std::string& tracingVar,
            ccmc::Kameleon* kameleon,
            const size_t nPointsOnPathLine,
            double& accuracy
        );

        std::vector <Seedpoint> findAndAddNightsideSeedPoints(
            std::vector<Seedpoint>& seedPoints,
            std::vector<double>& birthTimes,
            ccmc::Kameleon* kameleon,
            const std::string& tracingVar,
            const size_t nPointsOnPathLine
        );

        std::vector<glm::vec3> getFieldlinePositions(
            glm::vec3& seedPoint,
            ccmc::Kameleon* kameleon,
            float innerBoundaryLimit,
            size_t nPointsOnFieldlines
        );

    } // namespace fls
} // namespace openspace

#endif // __OPENSPACE_MODULE_FIELDLINESSEQUENCE___MOVINGFIELDLINEHELPER___H__
