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

#ifndef __OPENSPACE_MODULE_SPACE___SIMPLESPHEREGEOMETRY___H__
#define __OPENSPACE_MODULE_SPACE___SIMPLESPHEREGEOMETRY___H__

#include <modules/space/rendering/planetgeometry.h>

#include <openspace/properties/scalar/intproperty.h>
#include <openspace/properties/vector/vec3property.h>

namespace openspace {
    class Renderable;
    class Sphere;
} // namespace openspace

namespace openspace::documentation { struct Documentation; }

namespace openspace::planetgeometry {

class SimpleSphereGeometry : public PlanetGeometry {
public:
    SimpleSphereGeometry(const ghoul::Dictionary& dictionary);
    ~SimpleSphereGeometry() override;

    void initialize() override;
    void deinitialize() override;
    void render() override;

    float boundingSphere() const override;

    static documentation::Documentation Documentation();

private:
    void createSphere();

    properties::Vec3Property _radius;
    properties::IntProperty _segments;
    Sphere* _sphere;
};

} // namespace openspace::planetgeometry

#endif // __OPENSPACE_MODULE_SPACE___SIMPLESPHEREGEOMETRY___H__