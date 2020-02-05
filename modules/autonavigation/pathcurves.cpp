/*****************************************************************************************
 *                                                                                       *
 * OpenSpace                                                                             *
 *                                                                                       *
 * Copyright (c) 2014-2019                                                               *
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

#include <modules/autonavigation/pathcurves.h>

#include <modules/autonavigation/helperfunctions.h>
#include <openspace/query/query.h>
#include <openspace/scene/scenegraphnode.h>
#include <ghoul/logging/logmanager.h>

namespace {
    constexpr const char* _loggerCat = "PathCurve";
} // namespace

namespace openspace::autonavigation {

PathCurve::~PathCurve() {}

// Approximate the curve length by dividing the curve into smaller linear 
// segments and accumulate their length
double PathCurve::arcLength(double tLimit) {
    double dt = 0.01; // TODO: choose a good dt
    double sum = 0.0;
    for (double t = 0.0; t <= tLimit - dt; t += dt) {
        double ds = glm::length(valueAt(t + dt) - valueAt(t));
        sum += ds;
    }
    return sum;
}

// TODO: remove when not needed
// Created for debugging
std::vector<glm::dvec3> PathCurve::getPoints() {
    return _points;
}

Bezier3Curve::Bezier3Curve(CameraState& start, CameraState& end) {
    // TODO: CALCULATE AND SET CONDITION BOOLS IN CURVE CONSTRUCTOR
    glm::dvec3 startNodePos = sceneGraphNode(start.referenceNode)->worldPosition();
    glm::dvec3 startDirection = start.position - startNodePos;
    double startRadius = sceneGraphNode(start.referenceNode)->boundingSphere();
    double endRadius = sceneGraphNode(end.referenceNode)->boundingSphere();

    glm::dvec3 endNodePos = sceneGraphNode(end.referenceNode)->worldPosition();
    glm::dvec3 endDirection = end.position - endNodePos;

    glm::dvec3 nodePosDiff = endNodePos - startNodePos;
    double cosStartAngle = glm::dot(normalize(startDirection), normalize(nodePosDiff));
    double cosEndAngle = glm::dot(normalize(endDirection), normalize(nodePosDiff));

    // TODO: Test with raycaster, test is dependent on start position
    bool TARGET_BEHIND_STARTNODE = cosStartAngle < -0.8; 
    bool TARGET_ON_BACKSIDE = cosEndAngle > 0.8;
    bool TARGET_IN_OPPOSITE_DIRECTION = cosStartAngle > 0.7;


    // SET CONTROL POINTS
    _points.push_back(start.position);
    _points.push_back(start.position + 2.0 * startRadius * normalize(startDirection));

    if ( TARGET_BEHIND_STARTNODE )
    {
        glm::dvec3 parallell = normalize(nodePosDiff) * glm::dot(startDirection, normalize(nodePosDiff));
        glm::dvec3 orthogonal = normalize(startDirection - parallell);
        //Point on the side of start node
        double dist = 5.0 * startRadius;
        glm::dvec3 extraKnot = startNodePos + dist * orthogonal;
        
        _points.push_back(extraKnot - parallell);
        _points.push_back(extraKnot); 
        _points.push_back(extraKnot + parallell);
    }

    if (TARGET_IN_OPPOSITE_DIRECTION && ! TARGET_ON_BACKSIDE) {
        glm::dvec3 parallell = normalize(nodePosDiff * glm::dot(startDirection, normalize(nodePosDiff)));
        glm::dvec3 orthogonal = normalize(normalize(startDirection) - parallell);
        // Distant middle point
        double dist = 0.5 * length(nodePosDiff);
        glm::dvec3 extraKnot = startNodePos - dist * parallell + 3.0 * dist * orthogonal;

        _points.push_back(extraKnot - 0.5 * dist * parallell);
        _points.push_back(extraKnot);
        _points.push_back(extraKnot + 0.5 * dist * parallell);
    }
    
    if (TARGET_ON_BACKSIDE)
    {
        glm::dvec3 parallell = normalize(nodePosDiff) * glm::dot(endDirection, normalize(nodePosDiff));
        glm::dvec3 orthogonal = normalize(endDirection - parallell);
        //Point on the side of start node
        double dist = 5.0 * endRadius;
        glm::dvec3 extraKnot = endNodePos + dist * orthogonal;

        _points.push_back(extraKnot - parallell);
        _points.push_back(extraKnot);
        _points.push_back(extraKnot + parallell);
    }

    _points.push_back(end.position + 2.0 * endRadius * normalize(endDirection));
    _points.push_back(end.position);
}

glm::dvec3 Bezier3Curve::valueAt(double t) {
    return interpolation::piecewiseCubicBezier(t, _points);
}

LinearCurve::LinearCurve(CameraState& start, CameraState& end) {
    _points.push_back(start.position);
    _points.push_back(end.position);
}

glm::dvec3 LinearCurve::valueAt(double t) {
    return interpolation::linear(t, _points[0], _points[1]);
}

// TODO: Iprove handling of pauses
PauseCurve::PauseCurve(CameraState& state) {
    _points.push_back(state.position);
}

glm::dvec3 PauseCurve::valueAt(double t) {
    return _points[0];
}

} // namespace openspace::autonavigation
