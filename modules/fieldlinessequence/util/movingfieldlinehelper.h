/*****************************************************************************************
 *                                                                                       *
 * OpenSpace                                                                             *
 *                                                                                       *
 * Copyright (c) 2014-2021                                                               *
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

#ifndef __OPENSPACE_MODULE_FIELDLINESSEQUENCE___MOVINGFIELDLINEHELPER___H__
#define __OPENSPACE_MODULE_FIELDLINESSEQUENCE___MOVINGFIELDLINEHELPER___H__

 //#include <ghoul/glm.h>
 //#include <string>
 //#include <vector>

//for the use of ifstream
#include <fstream>

namespace openspace {

    class FieldlinesState;

    namespace fls {
        //Simon: Test to see if we can edit our seedpoints
        struct pointsWithEigVec {
            std::vector<glm::vec3> cPoints;
            std::vector<glm::vec3> eigVals;
            std::vector<glm::vec3> eigVecs;
        };

        bool convertCdfToMovingFieldlinesState(FieldlinesState& state, const std::string& cdfPath,
            const std::vector<glm::vec3>& seedMap,
            double manualTimeOffset, const std::string& tracingVar,
            std::vector<std::string>& extraVars, std::vector<std::string>& extraMagVars,
            const size_t nPointsOnPathLine, const size_t nPointsOnFieldLines);



        glm::vec3 moveSeedpointInEigenvectorDirection(const glm::vec3& const pointInSpace, const glm::vec3& const eigenvector, const float& direction);
        
        
    } // namespace fls
} // namespace openspace

#endif // __OPENSPACE_MODULE_FIELDLINESSEQUENCE___MOVINGFIELDLINEHELPER___H__
