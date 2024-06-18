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

 /***************************************************************************************
 * Modified part of the code (4D texture mechanism) from Eric Bruneton is used in the
 * following code.
 ****************************************************************************************/

/**
 * Precomputed Atmospheric Scattering
 * Copyright (c) 2008 INRIA
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice, this list of
 *    conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list
 *    of conditions and the following disclaimer in the documentation and/or other
 *    materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <modules/atmosphere/rendering/atmospheredeferredcaster.h>

#include <openspace/engine/globals.h>
#include <openspace/query/query.h>
#include <openspace/rendering/renderengine.h>
#include <openspace/scene/scene.h>
#include <openspace/util/spicemanager.h>
#include <ghoul/filesystem/filesystem.h>
#include <ghoul/logging/logmanager.h>
#include <ghoul/opengl/openglstatecache.h>
#include <cmath>
#include <fstream>

namespace {
    constexpr std::string_view _loggerCat = "AtmosphereDeferredcaster";

    constexpr float ATM_EPS = 2000.f;
    constexpr float KM_TO_M = 1000.f;

    template <GLenum colorBufferAttachment = GL_COLOR_ATTACHMENT0>
    void saveTextureFile(const std::filesystem::path& fileName, const glm::ivec2& size,
        bool decimaloutput = false) {
        std::ofstream ppmFile(fileName);
        if (!ppmFile.is_open()) {
            return;
        }

        ppmFile << "P3" << '\n' << size.x << " " << size.y << '\n' << "255" << '\n';

        glReadBuffer(colorBufferAttachment);

        if (decimaloutput) {
            // Decimal output
            std::vector<float> px(
                size.x * size.y * 3,
                static_cast<float>(255)
            );

            glReadPixels(0, 0, size.x, size.y, GL_RGB, GL_FLOAT, px.data());

            int k = 0;
            for (int i = 0; i < size.x; i++) {
                for (int j = 0; j < size.y; j++) {
                    // decimal
                    ppmFile << px[k] << ' ' << px[k + 1] << ' ' << px[k + 2] << ' ';
                    k += 3;
                }
                ppmFile << '\n';
            }
        }
        else {
            // RGB output
            std::vector<unsigned char> px(
                size.x * size.y * 3,
                static_cast<unsigned char>(255)
            );

            glReadPixels(0, 0, size.x, size.y, GL_RGB, GL_UNSIGNED_BYTE, px.data());

            int k = 0;
            for (int i = 0; i < size.x; i++) {
                for (int j = 0; j < size.y; j++) {
                    // decimal
                    //ppmFile << static_cast<float>(px[k]) << ' '
                    //    << static_cast<float>(px[k + 1]) << ' '
                    //    << static_cast<float>(px[k + 2]) << ' ';
                    // RGB
                    ppmFile << static_cast<unsigned int>(px[k]) << ' '
                        << static_cast<unsigned int>(px[k + 1]) << ' '
                        << static_cast<unsigned int>(px[k + 2]) << ' ';
                    k += 3;
                }
                ppmFile << '\n';
            }
        }
    }

    void saveTextureFile(const std::filesystem::path& fileName,
                         const openspace::CPUTexture& texture, bool writeFloats = false) {
        //const std::filesystem::path filename = "my_deltaE_table_test.ppm";
        std::ofstream ppmFile(fileName);
        if (!ppmFile.is_open()) {
            return;
        }
        ppmFile << "P3" << '\n' << texture.width << " " << texture.height
            << '\n' << "255" << '\n';

        int k = 0;
        for (int x = 0; x < texture.width; x++) {
            for (int y = 0; y < texture.height; y++) {

                float r = texture.data[k];
                float g = texture.data[k + 1];
                float b = texture.data[k + 2];

                if (writeFloats) {
                    ppmFile << r << ' ' << g << ' ' << b << ' ';
                }
                else {
                    ppmFile
                        << static_cast<unsigned int>(r * 255) << ' '
                        << static_cast<unsigned int>(g * 255) << ' '
                        << static_cast<unsigned int>(b * 255) << ' ';
                }

                k += 3;
            }
            ppmFile << '\n';
        }
    }

    bool isAtmosphereInFrustum(const glm::dmat4& mv, const glm::dvec3& position,
                               double radius)
    {
        // Frustum Planes
        const glm::dvec3 col1 = glm::dvec3(mv[0][0], mv[1][0], mv[2][0]);
        const glm::dvec3 col2 = glm::dvec3(mv[0][1], mv[1][1], mv[2][1]);
        const glm::dvec3 col3 = glm::dvec3(mv[0][2], mv[1][2], mv[2][2]);
        const glm::dvec3 col4 = glm::dvec3(mv[0][3], mv[1][3], mv[2][3]);

        glm::dvec3 leftNormal = col4 + col1;
        glm::dvec3 rightNormal = col4 - col1;
        glm::dvec3 bottomNormal = col4 + col2;
        glm::dvec3 topNormal = col4 - col2;
        glm::dvec3 nearNormal = col3 + col4;
        glm::dvec3 farNormal = col4 - col3;

        // Plane Distances
        double leftDistance = mv[3][3] + mv[3][0];
        double rightDistance = mv[3][3] - mv[3][0];
        double bottomDistance = mv[3][3] + mv[3][1];
        double topDistance = mv[3][3] - mv[3][1];
        double nearDistance = mv[3][3] + mv[3][2];

        // Normalize Planes
        const double invLeftMag = 1.0 / glm::length(leftNormal);
        leftNormal *= invLeftMag;
        leftDistance *= invLeftMag;

        const double invRightMag = 1.0 / glm::length(rightNormal);
        rightNormal *= invRightMag;
        rightDistance *= invRightMag;

        const double invBottomMag = 1.0 / glm::length(bottomNormal);
        bottomNormal *= invBottomMag;
        bottomDistance *= invBottomMag;

        const double invTopMag = 1.0 / glm::length(topNormal);
        topNormal *= invTopMag;
        topDistance *= invTopMag;

        const double invNearMag = 1.0 / glm::length(nearNormal);
        nearNormal *= invNearMag;
        nearDistance *= invNearMag;

        const double invFarMag = 1.0 / glm::length(farNormal);
        farNormal *= invFarMag;

        const bool outsideFrustum =
            (((glm::dot(leftNormal, position) + leftDistance) < -radius) ||
            ((glm::dot(rightNormal, position) + rightDistance) < -radius) ||
            ((glm::dot(bottomNormal, position) + bottomDistance) < -radius) ||
            ((glm::dot(topNormal, position) + topDistance) < -radius) ||
            ((glm::dot(nearNormal, position) + nearDistance) < -radius));

        return !outsideFrustum;
    }

    GLuint createTexture(const glm::ivec2& size, std::string_view name) {
        GLuint t = 0;
        glGenTextures(1, &t);
        glBindTexture(GL_TEXTURE_2D, t);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        // Stopped using a buffer object for GL_PIXEL_UNPACK_BUFFER
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RGB32F,
            size.x,
            size.y,
            0,
            GL_RGB,
            GL_FLOAT,
            nullptr
        );
        if (glbinding::Binding::ObjectLabel.isResolved()) {
            glObjectLabel(GL_TEXTURE, t, static_cast<GLsizei>(name.size()), name.data());
        }
        return t;
    }

    GLuint createTexture(const glm::ivec3& size, std::string_view name, int components) {
        ghoul_assert(components == 3 || components == 4, "Only 3-4 components supported");

        GLuint t = 0;
        glGenTextures(1, &t);
        glBindTexture(GL_TEXTURE_3D, t);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        // Stopped using a buffer object for GL_PIXEL_UNPACK_BUFFER
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        glTexImage3D(
            GL_TEXTURE_3D,
            0,
            (components == 3) ? GL_RGB32F : GL_RGBA32F,
            size.x,
            size.y,
            size.z,
            0,
            GL_RGB,
            GL_FLOAT,
            nullptr
        );
        if (glbinding::Binding::ObjectLabel.isResolved()) {
            glObjectLabel(GL_TEXTURE, t, static_cast<GLsizei>(name.size()), name.data());
        }
        return t;
    }
} // namespace

namespace openspace {

AtmosphereDeferredcaster::AtmosphereDeferredcaster(float textureScale,
                                       std::vector<ShadowConfiguration> shadowConfigArray,
                                                              bool saveCalculatedTextures)
    : _transmittanceTableSize(glm::ivec2(256 * textureScale, 64 * textureScale))
    , _irradianceTableSize(glm::ivec2(64 * textureScale, 16 * textureScale))
    , _deltaETableSize(glm::ivec2(64 * textureScale, 16 * textureScale))
    , _muSSamples(static_cast<int>(32 * textureScale))
    , _nuSamples(static_cast<int>(8 * textureScale))
    , _muSamples(static_cast<int>(128 * textureScale))
    , _rSamples(static_cast<int>(32 * textureScale))
    , _textureSize(_muSSamples * _nuSamples, _muSamples, _rSamples)
    , _shadowConfArray(std::move(shadowConfigArray))
    , _saveCalculationTextures(saveCalculatedTextures)
{
    std::memset(_uniformNameBuffer, '\0', sizeof(_uniformNameBuffer));
    std::strcpy(_uniformNameBuffer, "shadowDataArray[");
    _shadowDataArrayCache.reserve(_shadowConfArray.size());
}

void AtmosphereDeferredcaster::initialize() {
    ZoneScoped;

    _transmittanceTableTexture = createTexture(_transmittanceTableSize, "Transmittance");
    _irradianceTableTexture = createTexture(_irradianceTableSize, "Irradiance");
    _inScatteringTableTexture = createTexture(_textureSize, "InScattering", 4);
    calculateAtmosphereParameters();
}

void AtmosphereDeferredcaster::deinitialize() {
    ZoneScoped;

    glDeleteTextures(1, &_transmittanceTableTexture);
    glDeleteTextures(1, &_irradianceTableTexture);
    glDeleteTextures(1, &_inScatteringTableTexture);
}

void AtmosphereDeferredcaster::update(const UpdateData&) {}

float AtmosphereDeferredcaster::eclipseShadow(const glm::dvec3& position) {
    // This code is copied from the atmosphere deferred fragment shader
    // It is used to calculate the eclipse shadow
    if (_shadowDataArrayCache.empty() || !_shadowDataArrayCache.front().isShadowing) {
        return 1.f;
    }

    const ShadowRenderingStruct& shadow = _shadowDataArrayCache.front();
    const glm::dvec3 positionToCaster = shadow.casterPositionVec - position;
    const glm::dvec3 sourceToCaster = shadow.sourceCasterVec; // Normalized
    const glm::dvec3 casterShadow =
        dot(positionToCaster, sourceToCaster) * sourceToCaster;
    const glm::dvec3 positionToShadow = positionToCaster - casterShadow;

    const float distanceToShadow = static_cast<float>(length(positionToShadow));
    const double shadowLength = length(casterShadow);

    const float radiusPenumbra = static_cast<float>(
        shadow.radiusCaster * (shadowLength + shadow.penumbra) / shadow.penumbra
    );
    const float radiusUmbra = static_cast<float>(
        shadow.radiusCaster * (shadow.umbra - shadowLength) / shadow.umbra
    );

    // Is the position in the umbra - the fully shaded part
    if (distanceToShadow < radiusUmbra) {
        if (_hardShadowsEnabled) {
            return 0.5f;
        }
        else {
            // Smooth the shadow with the butterworth function
            const float s = radiusUmbra / (radiusUmbra + std::pow(distanceToShadow, 4.f));
            return std::sqrt(s);
        }
    }
    else if (distanceToShadow < radiusPenumbra) { // In penumbra - partially shaded part
        return _hardShadowsEnabled ? 0.5f : distanceToShadow / radiusPenumbra;
    }
    else {
        return 1.f;
    }
}

void AtmosphereDeferredcaster::preRaycast(const RenderData& data, const DeferredcastData&,
                                          ghoul::opengl::ProgramObject& program)
{
    ZoneScoped;
    TracyGpuZone("Atmosphere preRaycast");


    // Atmosphere Frustum Culling
    const glm::dvec3 tPlanetPos = glm::dvec3(
        _modelTransform * glm::dvec4(0.0, 0.0, 0.0, 1.0)
    );
    const double distance = glm::distance(tPlanetPos, data.camera.eyePositionVec3());

    // Radius is in KM
    const double scaledRadius = glm::length(
        glm::dmat3(_modelTransform) * glm::dvec3(KM_TO_M * _atmosphereRadius, 0.0, 0.0)
    );

    // Number of planet radii to use as distance threshold for culling
    program.setUniform(_uniformCache.cullAtmosphere, 1);

    constexpr double DistanceCullingRadii = 5000;
    const glm::dmat4 MV = glm::dmat4(data.camera.sgctInternal.projectionMatrix()) *
        data.camera.combinedViewMatrix();
    if (distance <= scaledRadius * DistanceCullingRadii &&
        isAtmosphereInFrustum(MV, tPlanetPos, scaledRadius + ATM_EPS))
    {
        program.setUniform(_uniformCache.cullAtmosphere, 0);
        program.setUniform(_uniformCache.opacity, _opacity);
        program.setUniform(_uniformCache.Rg, _atmospherePlanetRadius);
        program.setUniform(_uniformCache.Rt, _atmosphereRadius);
        program.setUniform(_uniformCache.groundRadianceEmission, _groundRadianceEmission);
        program.setUniform(_uniformCache.HR, _rayleighHeightScale);
        program.setUniform(_uniformCache.betaRayleigh, _rayleighScatteringCoeff);
        program.setUniform(_uniformCache.HM, _mieHeightScale);
        program.setUniform(_uniformCache.betaMieExtinction, _mieExtinctionCoeff);
        program.setUniform(_uniformCache.mieG, _miePhaseConstant);
        program.setUniform(_uniformCache.sunRadiance, _sunRadianceIntensity);
        program.setUniform(_uniformCache.ozoneLayerEnabled, _ozoneEnabled);
        program.setUniform(_uniformCache.HO, _ozoneHeightScale);
        program.setUniform(_uniformCache.betaOzoneExtinction, _ozoneExtinctionCoeff);
        program.setUniform(_uniformCache.SAMPLES_R, _rSamples);
        program.setUniform(_uniformCache.SAMPLES_MU, _muSamples);
        program.setUniform(_uniformCache.SAMPLES_MU_S, _muSSamples);
        program.setUniform(_uniformCache.SAMPLES_NU, _nuSamples);
        // We expose the value as degrees, but the shader wants radians
        program.setUniform(_uniformCache.sunAngularSize, glm::radians(_sunAngularSize));

        // Object Space
        const glm::dmat4 invModelMatrix = glm::inverse(_modelTransform);
        program.setUniform(_uniformCache.inverseModelTransformMatrix, invModelMatrix);
        program.setUniform(_uniformCache.modelTransformMatrix, _modelTransform);

        const glm::dmat4 viewToWorld = glm::inverse(data.camera.combinedViewMatrix());

        // Eye Space to World Space
        program.setUniform(_uniformCache.viewToWorldMatrix, viewToWorld);

        // Projection to Eye Space
        const glm::dmat4 dInvProj = glm::inverse(
            glm::dmat4(data.camera.projectionMatrix())
        );

        const glm::dmat4 invWholePipeline = invModelMatrix * viewToWorld * dInvProj;

        program.setUniform(
            _uniformCache.projectionToModelTransformMatrix,
            invWholePipeline
        );

        const glm::dvec4 camPosObjCoords =
            invModelMatrix * glm::dvec4(data.camera.eyePositionVec3(), 1.0);
        program.setUniform(_uniformCache.camPosObj, glm::dvec3(camPosObjCoords));

        // For the lighting we use the provided node, or the Sun
        SceneGraphNode* node =
            _lightSourceNode ? _lightSourceNode : sceneGraph()->sceneGraphNode("Sun");
        const glm::dvec3 sunPosWorld = node ? node->worldPosition() : glm::dvec3(0.0);

        glm::dvec3 sunPosObj;
        if (_sunFollowingCameraEnabled) {
            sunPosObj = camPosObjCoords;
        }
        else {
            sunPosObj = invModelMatrix * glm::dvec4(sunPosWorld, 1.0);
        }

        // Sun Position in Object Space
        program.setUniform(_uniformCache.sunDirectionObj, glm::normalize(sunPosObj));

        // Shadow calculations..
        _shadowDataArrayCache.clear();
        for (ShadowConfiguration& shadowConf : _shadowConfArray) {
            // TO REMEMBER: all distances and lengths in world coordinates are in
            // meters!!! We need to move this to view space...
            double lt = 0.0;
            glm::dvec3 sourcePos = SpiceManager::ref().targetPosition(
                shadowConf.source.first,
                "SSB",
                "GALACTIC",
                {},
                data.time.j2000Seconds(),
                lt
            );
            sourcePos *= KM_TO_M; // converting to meters
            glm::dvec3 casterPos = SpiceManager::ref().targetPosition(
                shadowConf.caster.first,
                "SSB",
                "GALACTIC",
                {},
                data.time.j2000Seconds(),
                lt
            );
            casterPos *= KM_TO_M; // converting to meters

            SceneGraphNode* sourceNode = sceneGraphNode(shadowConf.source.first);
            if (!sourceNode) {
                if (!shadowConf.printedSourceError) {
                    LERROR("Invalid scenegraph node for the shadow's receiver");
                    shadowConf.printedSourceError = true;
                }
                return;
            }
            SceneGraphNode* casterNode = sceneGraphNode(shadowConf.caster.first);
            if (!casterNode) {
                if (!shadowConf.printedCasterError) {
                    LERROR("Invalid scenegraph node for the shadow's caster");
                    shadowConf.printedCasterError = true;
                }
                return;
            }

            const double sourceScale = std::max(glm::compMax(sourceNode->scale()), 1.0);
            const double casterScale = std::max(glm::compMax(casterNode->scale()), 1.0);
            const double actualSourceRadius = shadowConf.source.second * sourceScale;
            const double actualCasterRadius = shadowConf.caster.second * casterScale;
            // First we determine if the caster is shadowing the current planet
            // (all calculations in World Coordinates):
            const glm::dvec3 planetCasterVec =
                casterPos - data.modelTransform.translation;
            const glm::dvec3 sourceCasterVec = casterPos - sourcePos;
            const double scLength = glm::length(sourceCasterVec);
            const glm::dvec3 planetCasterProj =
                (glm::dot(planetCasterVec, sourceCasterVec) / (scLength * scLength)) *
                sourceCasterVec;
            const double dTest = glm::length(planetCasterVec - planetCasterProj);
            const double xpTest = actualCasterRadius * scLength /
                (actualSourceRadius + actualCasterRadius);
            const double rpTest = actualCasterRadius *
                (glm::length(planetCasterProj) + xpTest) / xpTest;

            const double casterDistSun = glm::length(casterPos - sunPosWorld);
            const double planetDistSun = glm::length(
                data.modelTransform.translation - sunPosWorld
            );

            ShadowRenderingStruct shadow;
            shadow.isShadowing = false;

            if (((dTest - rpTest) < (_atmospherePlanetRadius * KM_TO_M)) &&
                (casterDistSun < planetDistSun))
            {
                // The current caster is shadowing the current planet
                shadow.isShadowing = true;
                shadow.radiusSource = actualSourceRadius;
                shadow.radiusCaster = actualCasterRadius;
                shadow.sourceCasterVec = glm::normalize(sourceCasterVec);
                shadow.penumbra = xpTest;
                shadow.umbra =
                    shadow.radiusCaster * scLength /
                    (shadow.radiusSource - shadow.radiusCaster);
                shadow.casterPositionVec = casterPos;
            }
            _shadowDataArrayCache.push_back(shadow);
        }

        // _uniformNameBuffer[0..15] = "shadowDataArray["
        unsigned int counter = 0;
        for (const ShadowRenderingStruct& sd : _shadowDataArrayCache) {
            // Add the counter
            char* bf = std::format_to(_uniformNameBuffer + 16, "{}", counter);

            std::strcpy(bf, "].isShadowing\0");
            program.setUniform(_uniformNameBuffer, sd.isShadowing);

            if (sd.isShadowing) {
                std::strcpy(bf, "].xp\0");
                program.setUniform(_uniformNameBuffer, sd.penumbra);
                std::strcpy(bf, "].xu\0");
                program.setUniform(_uniformNameBuffer, sd.umbra);
                std::strcpy(bf, "].rc\0");
                program.setUniform(_uniformNameBuffer, sd.radiusCaster);
                std::strcpy(bf, "].sourceCasterVec\0");
                program.setUniform(_uniformNameBuffer, sd.sourceCasterVec);
                std::strcpy(bf, "].casterPositionVec\0");
                program.setUniform(_uniformNameBuffer, sd.casterPositionVec);
            }
            counter++;
        }
        program.setUniform(_uniformCache.hardShadows, _hardShadowsEnabled);
    }
    _transmittanceTableTextureUnit.activate();
    glBindTexture(GL_TEXTURE_2D, _transmittanceTableTexture);
    program.setUniform(
        _uniformCache.transmittanceTexture,
        _transmittanceTableTextureUnit
    );

    _irradianceTableTextureUnit.activate();
    glBindTexture(GL_TEXTURE_2D, _irradianceTableTexture);
    program.setUniform(_uniformCache.irradianceTexture, _irradianceTableTextureUnit);

    _inScatteringTableTextureUnit.activate();
    glBindTexture(GL_TEXTURE_3D, _inScatteringTableTexture);
    program.setUniform(_uniformCache.inscatterTexture, _inScatteringTableTextureUnit);
}

void AtmosphereDeferredcaster::postRaycast(const RenderData&, const DeferredcastData&,
                                           ghoul::opengl::ProgramObject&)
{
    ZoneScoped;

    TracyGpuZone("Atmosphere postRaycast");

    // Deactivate the texture units
    _transmittanceTableTextureUnit.deactivate();
    _irradianceTableTextureUnit.deactivate();
    _inScatteringTableTextureUnit.deactivate();
}

std::filesystem::path AtmosphereDeferredcaster::deferredcastFSPath() const {
    return absPath("${MODULE_ATMOSPHERE}/shaders/atmosphere_deferred_fs.glsl");
}

std::filesystem::path AtmosphereDeferredcaster::deferredcastVSPath() const {
    return absPath("${MODULE_ATMOSPHERE}/shaders/atmosphere_deferred_vs.glsl");
}

std::filesystem::path AtmosphereDeferredcaster::helperPath() const {
    return ""; // no helper file
}

void AtmosphereDeferredcaster::initializeCachedVariables(
                                                    ghoul::opengl::ProgramObject& program)
{
    ghoul::opengl::updateUniformLocations(program, _uniformCache);
}

void AtmosphereDeferredcaster::setModelTransform(glm::dmat4 transform) {
    _modelTransform = std::move(transform);
}

void AtmosphereDeferredcaster::setOpacity(float opacity) {
    _opacity = opacity;
}

void AtmosphereDeferredcaster::setParameters(float atmosphereRadius, float planetRadius,
                                             float averageGroundReflectance,
                                             float groundRadianceEmission,
                                             float rayleighHeightScale, bool enableOzone,
                                             float ozoneHeightScale, float mieHeightScale,
                                             float miePhaseConstant, float sunRadiance,
                                             glm::vec3 rayScatteringCoefficients,
                                             glm::vec3 ozoneExtinctionCoefficients,
                                             glm::vec3 mieScatteringCoefficients,
                                             glm::vec3 mieExtinctionCoefficients,
                                             bool sunFollowing, float sunAngularSize,
                                             SceneGraphNode* lightSourceNode)
{
    _atmosphereRadius = atmosphereRadius;
    _atmospherePlanetRadius = planetRadius;
    _averageGroundReflectance = averageGroundReflectance;
    _groundRadianceEmission = groundRadianceEmission;
    _rayleighHeightScale = rayleighHeightScale;
    _ozoneEnabled = enableOzone;
    _ozoneHeightScale = ozoneHeightScale;
    _mieHeightScale = mieHeightScale;
    _miePhaseConstant = miePhaseConstant;
    _sunRadianceIntensity = sunRadiance;
    _rayleighScatteringCoeff = std::move(rayScatteringCoefficients);
    _ozoneExtinctionCoeff = std::move(ozoneExtinctionCoefficients);
    _mieScatteringCoeff = std::move(mieScatteringCoefficients);
    _mieExtinctionCoeff = std::move(mieExtinctionCoefficients);
    _sunFollowingCameraEnabled = sunFollowing;
    _sunAngularSize = sunAngularSize;
    // The light source may be nullptr which we interpret to mean a position of (0,0,0)
    _lightSourceNode = lightSourceNode;
}

void AtmosphereDeferredcaster::setHardShadows(bool enabled) {
    _hardShadowsEnabled = enabled;
}

float rayDistance(float r, float mu, float Rt, float Rg) {
    const int INSCATTER_INTEGRAL_SAMPLES = 50;
    const float M_PI = 3.141592657;
    const float ATM_EPSILON = 1.0;
    // The light ray starting at the observer in/on the atmosphere can have to possible end
    // points: the top of the atmosphere or the planet ground. So the shortest path is the
    // one we are looking for, otherwise we may be passing through the ground

// cosine law
    float atmRadiusEps2 = (Rt + ATM_EPSILON) * (Rt + ATM_EPSILON);
    float mu2 = mu * mu;
    float r2 = r * r;
    float rayDistanceAtmosphere = -r * mu + std::sqrt(r2 * (mu2 - 1.0) + atmRadiusEps2);
    float delta = r2 * (mu2 - 1.0) + Rg * Rg;

    // Ray may be hitting ground
    if (delta >= 0.0) {
        float rayDistanceGround = -r * mu - sqrt(delta);
        if (rayDistanceGround >= 0.0) {
            return std::min(rayDistanceAtmosphere, rayDistanceGround);
        }
    }
    return rayDistanceAtmosphere;
}


void computeTransmittance(std::vector<float>& img, float Rg, float Rt, float HR,
    glm::vec3 betaRayleigh, float HO, glm::vec3 betaOzoneExtinction, float HM,
    glm::vec3 betaMieExtinction, bool ozoneLayerEnabled, glm::ivec2 size) {

    const int TransmittanceSteps = 500;

    // Optical depth by integration, from ray starting at point vec(x), i.e, height r and
    // angle mu (cosine of vec(v)) until top of atmosphere or planet's ground.
    // r := height of starting point vect(x)
    // mu := cosine of the zeith angle of vec(v). Or mu = (vec(x) * vec(v))/r
    // H := Thickness of atmosphere if its density were uniform (used for Rayleigh and Mie)
    auto opticalDepth = [&](float r, float mu, float H) -> float {
        float r2 = r * r;
        // Is ray below horizon? The transmittance table will have only the values for
        // transmittance starting at r (x) until the light ray touches the atmosphere or the
        // ground and only for view angles v between 0 and pi/2 + eps. That's because we can
        // calculate the transmittance for angles bigger than pi/2 just inverting the ray
        // direction and starting and ending points.

        // cosine law for triangles: y_i^2 = a^2 + b^2 - 2abcos(alpha)
        float cosZenithHorizon = -sqrt(1.0 - ((Rg * Rg) / r2));
        if (mu < cosZenithHorizon) {
            return 1e9;
        }

        // Integrating using the Trapezoidal rule:
        // Integral(f(y)dy)(from a to b) = ((b-a)/2n_steps)*(Sum(f(y_i+1)+f(y_i)))
        float b_a = rayDistance(r, mu, Rt, Rg);
        float deltaStep = b_a / static_cast<float>(TransmittanceSteps);
        // cosine law
        float y_i = std::exp(-(r - Rg) / H);

        float accumulation = 0.0;
        for (int i = 1; i <= TransmittanceSteps; i++) {
            float x_i = static_cast<float>(i) * deltaStep;
            // cosine law for triangles: y_i^2 = a^2 + b^2 - 2abcos(alpha)
            // In this case, a = r, b = x_i and cos(alpha) = cos(PI-zenithView) = mu
            float y_ii = std::exp(-(std::sqrt(r2 + x_i * x_i + 2.0 * x_i * r * mu) - Rg) / H);
            accumulation += (y_ii + y_i);
            y_i = y_ii;
        }
        return accumulation * (b_a / (2.0 * TransmittanceSteps));
    };

    int k = 0;
    for (int y = 0; y < size.y; y++) {
        for (int x = 0; x < size.x; x++) {
            // In the shader this x and y here are actually gl_FragCoord.x, gl_FragCoord
            // assumes a lower-left origin and pixels centers are located at half-pixel
            // enters, thus we had 0.5 to x, y
            float u_mu = (x + 0.5f) / static_cast<float>(size.x);
            float u_r = (y + 0.5f) / static_cast<float>(size.y);

            // In the paper u_r^2 = (r^2-Rg^2)/(Rt^2-Rg^2)
            // So, extracting r from u_r in the above equation:
            float r = Rg + (u_r * u_r) * (Rt - Rg);

            // In the paper the Bruneton suggest mu = dot(v,x)/||x|| with ||v|| = 1.0
            // Later he proposes u_mu = (1-exp(-3mu-0.6))/(1-exp(-3.6))
            // But the below one is better. See Collienne.
            // One must remember that mu is defined from 0 to PI/2 + epsilon
            float muSun = -0.15 + std::tan(1.5 * u_mu) / std::tan(1.5) * 1.15;

             glm::vec3 ozoneContribution = glm::vec3(0.0f);
             if (ozoneLayerEnabled) {
                 ozoneContribution = betaOzoneExtinction * 0.0000006f * opticalDepth(r, muSun, HO);
             }
             glm::vec3 opDepth = ozoneContribution;
             glm::vec3 opDepthBetaMie = betaMieExtinction * opticalDepth(r, muSun, HM);
             glm::vec3 opDepthBetaRay = betaRayleigh * opticalDepth(r, muSun, HR);

             //glm::vec3 color = glm::exp(-opDepth);
             glm::vec3 color = (opDepth + opDepthBetaMie + opDepthBetaRay);
             color = glm::exp(-color);

             img[k] = color.r;
             img[k + 1] = color.g;
             img[k + 2] = color.b;
             k += 3;
        }
    }

}

void AtmosphereDeferredcaster::calculateTransmittance() {
    ZoneScoped;

    glFramebufferTexture(
        GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0,
        _transmittanceTableTexture,
        0
    );
    glViewport(0, 0, _transmittanceTableSize.x, _transmittanceTableSize.y);
    using ProgramObject = ghoul::opengl::ProgramObject;
    std::unique_ptr<ProgramObject> program = ProgramObject::Build(
        "Transmittance Program",
        absPath("${MODULE_ATMOSPHERE}/shaders/calculation_vs.glsl"),
        absPath("${MODULE_ATMOSPHERE}/shaders/transmittance_calc_fs.glsl")
    );
    program->activate();
    program->setUniform("Rg", _atmospherePlanetRadius);
    program->setUniform("Rt", _atmosphereRadius);
    program->setUniform("HR", _rayleighHeightScale);
    program->setUniform("betaRayleigh", _rayleighScatteringCoeff);
    program->setUniform("HM", _mieHeightScale);
    program->setUniform("betaMieExtinction", _mieExtinctionCoeff);
    program->setUniform("TRANSMITTANCE", _transmittanceTableSize);
    program->setUniform("ozoneLayerEnabled", _ozoneEnabled);
    program->setUniform("HO", _ozoneHeightScale);
    program->setUniform("betaOzoneExtinction", _ozoneExtinctionCoeff);

    constexpr glm::vec4 Black = glm::vec4(0.f, 0.f, 0.f, 0.f);
    glClearBufferfv(GL_COLOR, 0, glm::value_ptr(Black));
    glDrawArrays(GL_TRIANGLES, 0, 6);
    if (_saveCalculationTextures) {
        saveTextureFile("transmittance_texture.ppm", _transmittanceTableSize);
    }
    program->deactivate();

    if (_saveCalculationTextures) {

        std::vector<float> img(
            _transmittanceTableSize.x * _transmittanceTableSize.y * 3,
            255.0f
        );
        glm::ivec2 size(_transmittanceTableSize);
        computeTransmittance(img, _atmospherePlanetRadius, _atmosphereRadius, _rayleighHeightScale,
            _rayleighScatteringCoeff, _ozoneHeightScale, _ozoneExtinctionCoeff, _mieHeightScale,
            _mieExtinctionCoeff, _ozoneEnabled, _transmittanceTableSize);

        _transmittanceTexture.width = _transmittanceTableSize.x;
        _transmittanceTexture.height = _transmittanceTableSize.y;
        _transmittanceTexture.data = std::move(img);

        saveTextureFile("my_transmittance_test.ppm", _transmittanceTexture);
    }

}


// biliniear interpolate a texture, clamps the texture coordinates to (0, size - 1)
// Assumes x and y lookup coordinates are given as decimals (0,1)
glm::vec3 texture(const CPUTexture& tex, float x, float y) {
    auto getColor = [&tex](int i, int j) {
        int index = (j * tex.width + i) * 3; // Each pixel has 3 values R,G,B
        return glm::vec3(
            static_cast<float>(tex.data[index]),
            static_cast<float>(tex.data[index + 1]),
            static_cast<float>(tex.data[index + 2]));
    };

    //auto getWrappedIndex = [](float v, int max) {
    //    // v is within our texture, no need to wrap it
    //    if (v > 0 && v < max) {
    //        return static_cast<int>(v);
    //    }

    //    // max is 1 less than the width/height size, we add +1 to get the correct texel
    //    // after casting eg., -0.2 should wrap to the last element
    //    if (v < 0) {
    //        const int size = max + 1;
    //        float wrappedV = size - v;
    //        return static_cast<int>(wrappedV);
    //    }

    //    else {

    //    }
    //};

    // Scale lookup coordinates to match texture size
    x *= tex.width - 1;
    y *= tex.height - 1;

    // Calc integer coordinates of the four sourrounding pixels
    int x1 = std::clamp(static_cast<int>(x), 0, tex.width - 1);
    int y1 = std::clamp(static_cast<int>(y), 0, tex.height - 1);
    int x2 = std::clamp(x1 + 1, 0, tex.width - 1);
    int y2 = std::clamp(y1 + 1, 0, tex.height - 1);

    // Get fractional part of x and y
    float fx = x - x1;
    float fy = y - y1;

    // Get colors of the four sourrounding pixels
    glm::vec3 c11 = getColor(x1, y1);
    glm::vec3 c12 = getColor(x1, y2);
    glm::vec3 c21 = getColor(x2, y1);
    glm::vec3 c22 = getColor(x2, y2);

    // Interpolate the colors
    glm::vec3 c1, c2, result;
    c1 = glm::mix(c11, c21, fx);
    c2 = glm::mix(c21, c22, fx);
    result = glm::mix(c1, c2, fy);

    return result;
}

// Function to access the transmittance texture. Given r and mu, returns the transmittance
// of a ray starting at vec(x), height r, and direction vec(v), mu, and length until it
// hits the ground or the top of atmosphere.
// r := height of starting point vect(x)
// mu := cosine of the zeith angle of vec(v). Or mu = (vec(x) * vec(v))/r
glm::vec3 transmittance(const CPUTexture& tex, float r, float mu, float Rg, float Rt)
{
    // Given the position x (here the altitude r) and the view angle v
    // (here the cosine(v)= mu), we map this
    float u_r = std::sqrt((r - Rg) / (Rt - Rg));
    // See Collienne to understand the mapping
    float u_mu = std::atan((mu + 0.15) / 1.15 * tan(1.5)) / 1.5;
    return texture(tex, u_mu, u_r);
}

// Given a position r and direction mu, calculates de transmittance along the ray with
// length d. This function uses the propriety of Transmittance:
// T(a,b) = TableT(a,v)/TableT(b, v)
// r := height of starting point vect(x)
// mu := cosine of the zeith angle of vec(v). Or mu = (vec(x) * vec(v))/r
glm::vec3 transmittance(const CPUTexture& tex, float r, float mu, float d, float Rg, float Rt) {
    // Here we use the transmittance property: T(x,v) = T(x,d)*T(d,v) to, given a distance
    // d, calculates that transmittance along that distance starting in x (height r):
    // T(x,d) = T(x,v)/T(d,v).
    //
    // From cosine law: c^2 = a^2 + b^2 - 2*a*b*cos(ab)
    float ri = std:: sqrt(d * d + r * r + 2.0 * r * d * mu);
    // mu_i = (vec(d) dot vec(v)) / r_i
    //      = ((vec(x) + vec(d-x)) dot vec(v))/ r_i
    //      = (r*mu + d) / r_i
    float mui = (d + r * mu) / ri;

    // It's important to remember that we calculate the Transmittance table only for zenith
    // angles between 0 and pi/2+episilon. Then, if mu < 0.0, we just need to invert the
    // view direction and the start and end points between them, i.e., if
    // x --> x0, then x0-->x.
    // Also, let's use the property: T(a,c) = T(a,b)*T(b,c)
    // Because T(a,c) and T(b,c) are already in the table T, T(a,b) = T(a,c)/T(b,c).
    glm::vec3 res;
    if (mu > 0.0) {
        res = transmittance(tex, r, mu, Rg, Rt) / transmittance(tex, ri, mui, Rg, Rt);
    }
    else {
        res = transmittance(tex, ri, -mui, Rg, Rt) / transmittance(tex, r, -mu, Rg, Rt);
    }
    return glm::min(res, 1.0f);
}

void computeDeltaE(CPUTexture& img, const  CPUTexture& _transmittance,
                   float Rg, float Rt) {

    int k = 0;
    for (int y = 0; y < img.height; y++) {
        for (int x = 0; x < img.width; x++) {
            // See Bruneton and Collienne to understand the mapping
            // In the shader it was gl_FragCoord.x - 0.5 but since fragcoord assume
            // center voxel and we already have left centered voxels we don't have to subtract
            float muSun = -0.2 + x / (static_cast<float>(img.width) - 1.0) * 1.2;
            float r = Rg + y / (static_cast<float>(img.height)) * (Rt - Rg);
            //float r = Rg + y / (static_cast<float>(img.height)) * (Rt - Rg);

            // We are calculating the Irradiance for L0, i.e., only the radiance coming from the Sun
            // direction is accounted for:
            // E[L0](x,s) = L0*dot(w,n) or 0 (if v!=s or the sun is occluded).
            // Because we consider the planet as a perfect sphere and we are considering only single
            // scattering here, the dot product dot(w,n) is equal to dot(s,n) that is equal to
            // dot(s, r/||r||) = muSun.
            glm::vec3 color = transmittance(_transmittance, r, muSun, Rg, Rt) * std::max(muSun, 0.0f);
            //color = glm::vec3(color.r, 0, 0);
            //img.data[k] = static_cast<unsigned int>(color.r * 255);
            //img.data[k + 1] = static_cast<unsigned int>(color.g * 255);
            //img.data[k + 2] = static_cast<unsigned int>(color.b * 255);
            img.data[k] = color.r;
            img.data[k + 1] = color.g;
            img.data[k + 2] = color.b;
            k += 3;
        }
    }

}

GLuint AtmosphereDeferredcaster::calculateDeltaE() {
    ZoneScoped;

    const GLuint deltaE = createTexture(_deltaETableSize, "DeltaE");
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, deltaE, 0);
    glViewport(0, 0, _deltaETableSize.x, _deltaETableSize.y);
    using ProgramObject = ghoul::opengl::ProgramObject;
    std::unique_ptr<ProgramObject> program = ProgramObject::Build(
        "Irradiance Program",
        absPath("${MODULE_ATMOSPHERE}/shaders/calculation_vs.glsl"),
        absPath("${MODULE_ATMOSPHERE}/shaders/irradiance_calc_fs.glsl")
    );
    program->activate();
    ghoul::opengl::TextureUnit unit;
    unit.activate();
    glBindTexture(GL_TEXTURE_2D, _transmittanceTableTexture);
    program->setUniform("transmittanceTexture", unit);
    program->setUniform("Rg", _atmospherePlanetRadius);
    program->setUniform("Rt", _atmosphereRadius);
    program->setUniform("OTHER_TEXTURES", _deltaETableSize);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    if (_saveCalculationTextures) {
        saveTextureFile("deltaE_table_texture.ppm", _deltaETableSize);
    }
    program->deactivate();

    if (_saveCalculationTextures) {
        _deltaETexture = CPUTexture(_deltaETableSize);

        //_deltaETexture = CPUTexture{
        //    .width = _deltaETableSize.x,
        //    .height = _deltaETableSize.y,
        //    .data = std::vector<float>(_deltaETableSize.x * _deltaETableSize.y * 3, 255.0f)
        //};

        computeDeltaE(
            _deltaETexture,
            _transmittanceTexture,
            _atmospherePlanetRadius,
            _atmosphereRadius
        );

        saveTextureFile("my_deltaE_table_test.ppm", _deltaETexture);
    }

    return deltaE;
}

std::pair<glm::vec3, glm::vec3> integrand(float r, float mu, float muSun, float nu,
    float y, float Rg, float Rt, const CPUTexture& transmittanceTexture,
    bool ozoneLayerEnabled, float HO, float HM, float HR)
{
    // The integral's integrand is the single inscattering radiance:
    // S[L0] = P_M*S_M[L0] + P_R*S_R[L0]
    // where S_M[L0] = T*(betaMScattering * exp(-h/H_M))*L0 and
    // S_R[L0] = T*(betaRScattering * exp(-h/H_R))*L0.
    // T = transmittance.
    // One must remember that because the occlusion on L0, the integrand here will be equal
    // to 0 in that cases. Also it is important to remember that the phase function for the
    // Rayleigh and Mie scattering are added during the rendering time to increase the
    // angular precision
    glm::vec3 S_R{ 0.f };
    glm::vec3 S_M{ 0.f };

    // cosine law
    float ri = std::max(std::sqrt(r * r + y * y + 2.0f * r * mu * y), Rg);

    // Considering the Sun as a parallel light source, thew vector s_i = s.
    // So muSun_i = (vec(y_i) dot vec(s))/r_i = ((vec(x) + vec(yi-x)) dot vec(s))/r_i
    // muSun_i = (vec(x) dot vec(s) + vec(yi-x) dot vec(s))/r_i = (r*muSun + yi*nu)/r_i
    float muSun_i = (nu * y + muSun * r) / ri;

    // If the muSun_i is smaller than the angle to horizon (no sun radiance hitting the
    // point y), we return S_R = S_M = 0.0.
    if (muSun_i >= -std::sqrt(1.0f - Rg * Rg / (ri * ri))) {
        // It's the transmittance from the point y (ri) to the top of atmosphere in direction
        // of the sun (muSun_i) and the transmittance from the observer at x (r) to y (ri).
        glm::vec3 transmittanceY =
            transmittance(transmittanceTexture, r, mu, y, Rg, Rt) *
            transmittance(transmittanceTexture, ri, muSun_i, Rg, Rt);
        // exp(-h/H)*T(x,v)
        if (ozoneLayerEnabled) {
            S_R = (std::exp(-(ri - Rg) / HO) + std::exp(-(ri - Rg) / HR)) * transmittanceY;
            S_M = std::exp(-(ri - Rg) / HM) * transmittanceY;
        }
        else {
            S_R = std::exp(-(ri - Rg) / HR) * transmittanceY;
            S_M = std::exp(-(ri - Rg) / HM) * transmittanceY;
        }
        // The L0 (sun radiance) is added in real-time.
    }
    return std::make_pair(S_R, S_M);
}

std::pair<glm::vec3, glm::vec3> inscatter(float r, float mu, float muSun, float nu,
    float Rt, float Rg, const CPUTexture& transmittanceTexture, bool ozoneLayerEnabled,
    float HO, float HM, float HR, const glm::vec3& betaRayleigh, const glm::vec3& betaMieScattering) {

    const int INSCATTER_INTEGRAL_SAMPLES = 50;

    // Let's calculate S_M and S_R by integration along the eye ray path inside the
    // atmosphere, given a position r, a view angle (cosine) mu, a sun position angle
    // (cosine) muSun, and the angle (cosine) between the sun position and the view
    // direction, nu. Integrating using the Trapezoidal rule:
    // Integral(f(y)dy)(from a to b) = (b-a)/2n_steps*(Sum(f(y_i+1)+f(y_i)))
    glm::vec3 S_R{ 0.f };
    glm::vec3 S_M{ 0.f };

    float rayDist = rayDistance(r, mu, Rt, Rg);
    float dy = rayDist / float(INSCATTER_INTEGRAL_SAMPLES);
    //glm::vec3 S_Ri;
    //glm::vec3 S_Mi;
    auto [S_Ri, S_Mi] = integrand(r, mu, muSun, nu, 0.0, Rg, Rt,
        transmittanceTexture, ozoneLayerEnabled, HO, HM, HR
    );
    for (int i = 1; i <= INSCATTER_INTEGRAL_SAMPLES; i++) {
        float yj = float(i) * dy;
        auto [S_Rj, S_Mj] = integrand(r, mu, muSun, nu, yj, Rg, Rt, transmittanceTexture,
            ozoneLayerEnabled, HO, HM, HR
        );
        S_R += (S_Ri + S_Rj);
        S_M += (S_Mi + S_Mj);
        S_Ri = S_Rj;
        S_Mi = S_Mj;
    }
    S_R *= betaRayleigh * (rayDist / (2.0f * static_cast<float>(INSCATTER_INTEGRAL_SAMPLES)));
    S_M *= betaMieScattering * (rayDist / (2.0f * static_cast<float>(INSCATTER_INTEGRAL_SAMPLES)));

    return std::make_pair(S_R, S_M);
}

// Given the windows's fragment coordinates, for a defined view port, gives back the
// interpolated r e [Rg, Rt] and mu, muSun amd nu e [-1, 1]
// r := height of starting point vect(x)
// mu := cosine of the zeith angle of vec(v). Or mu = (vec(x) * vec(v))/r
// muSun := cosine of the zeith angle of vec(s). Or muSun = (vec(s) * vec(v))
// nu := cosone of the angle between vec(s) and vec(v)
// dhdH := it is a vec4. dhdH.x stores the dminT := Rt - r, dhdH.y stores the dH value
//         (see paper), dhdH.z stores dminG := r - Rg and dhdH.w stores dh (see paper)
glm::vec3 unmappingMuMuSunNu(float r, const glm::vec4& dhdH, int SAMPLES_MU, float Rg,
                             float Rt, int SAMPLES_MU_S, int SAMPLES_NU, int x, int y) {
    float mu;
    float muSun;
    float nu;
    // Window coordinates of pixel (uncentering also)
    //glm::vec2 fragment = gl_FragCoord.xy - vec2(0.5);
    glm::vec2 fragment{ x, y };

    // Pre-calculations
    float r2 = r * r;
    float Rg2 = Rg * Rg;

    float halfSAMPLE_MU = float(SAMPLES_MU) / 2.0f;
    // If the (vec(x) dot vec(v))/r is negative, i.e., the light ray has great probability
    // to touch the ground, we obtain mu considering the geometry of the ground
    if (fragment.y < halfSAMPLE_MU) {
        float ud = 1.0 - (fragment.y / (halfSAMPLE_MU - 1.0f));
        float d = std::min(std::max(dhdH.z, ud * dhdH.w), dhdH.w * 0.999f);
        // cosine law: Rg^2 = r^2 + d^2 - 2rdcos(pi-theta) where cosine(theta) = mu
        mu = (Rg2 - r2 - d * d) / (2.0 * r * d);
        // We can't handle a ray inside the planet, i.e., when r ~ Rg, so we check against it.
        // If that is the case, we approximate to a ray touching the ground.
        // cosine(pi-theta) = dh/r = sqrt(r^2-Rg^2)
        // cosine(theta) = - sqrt(1 - Rg^2/r^2)
        mu = std::min(mu, -std::sqrt(1.0f - (Rg2 / r2)) - 0.001f);
    }
    // The light ray is touching the atmosphere and not the ground
    else {
        float d = (fragment.y - halfSAMPLE_MU) / (halfSAMPLE_MU - 1.0f);
        d = std::min(std::max(dhdH.x, d * dhdH.y), dhdH.y * 0.999f);
        // cosine law: Rt^2 = r^2 + d^2 - 2rdcos(pi-theta) where cosine(theta) = mu
        mu = (Rt * Rt - r2 - d * d) / (2.0 * r * d);
    }

    float modValueMuSun = std::fmod(
        fragment.x,
        static_cast<float>(SAMPLES_MU_S)) / (static_cast<float>(SAMPLES_MU_S) - 1.0
    );
    // The following mapping is different from the paper. See Collienne for an details.
    muSun = std::tan((2.0f * modValueMuSun - 1.0f + 0.26f) * 1.1f) / std::tan(1.26f * 1.1f);
    nu = -1.0f + std::floor(fragment.x / static_cast<float>(SAMPLES_MU_S)) /
        (static_cast<float>(SAMPLES_NU) - 1.0f) * 2.0f;

    return glm::vec3{ mu, muSun, nu };
}

std::pair<float, glm::vec4> step3DTexture (float Rg, float Rt, int rSamples, int layer) {
    float atmospherePlanetRadius = Rg;
    float atmosphereRadius = Rt;
    //int _rSamples = textureSize.z;
    // See OpenGL redbook 8th Edition page 556 for Layered Rendering
    const float planet2 = atmospherePlanetRadius * atmospherePlanetRadius;
    const float diff = atmosphereRadius * atmosphereRadius - planet2;
    const float ri = static_cast<float>(layer) / static_cast<float>(rSamples - 1);
    float eps = 0.01f;
    if (layer > 0) {
        if (layer == (rSamples - 1)) {
            eps = -0.001f;
        }
        else {
            eps = 0.f;
        }
    }
    const float r = std::sqrt(planet2 + ri * ri * diff) + eps;
    const float dminG = r - atmospherePlanetRadius;
    const float dminT = atmosphereRadius - r;
    const float dh = std::sqrt(r * r - planet2);
    const float dH = dh + std::sqrt(diff);

    glm::vec4 dhdH{ dminT, dH, dminG, dh };
    return std::make_pair(r, dhdH);
}

std::pair<CPUTexture3D, CPUTexture3D> computeDeltaS(const glm::ivec3& textureSize,
    const CPUTexture& transmittanceTexture, float Rg, float Rt, float HR,
    const glm::vec3& betaRayleigh, float HM, const glm::vec3& betaMiescattering,
    int SAMPLES_MU_S, int SAMPLES_NU, int SAMPLES_MU, bool ozoneLayerEnabled, float HO) {

    CPUTexture3D deltaSRayleigh(textureSize.z, { textureSize.x, textureSize.y });
    CPUTexture3D deltaSmie(textureSize.z, { textureSize.x, textureSize.y });

    for (int layer = 0; layer < textureSize.z; layer++) {
        std::pair<float, glm::vec4> v = step3DTexture(Rg, Rt, textureSize.z, layer);
        const float r = v.first;
        const glm::vec4 dhdH = v.second;
        //auto [r, dhdH] = step3DTexture(Rg, Rt, textureSize.z,layer);

        int k = 0;
        for (int y = 0; y < textureSize.y; y++) {
            for (int x = 0; x < textureSize.x; x++) {
                // From the layer interpolation (see C++ code for layer to r) and the textures
                // parameters (uv), we unmapping mu, muSun and nu.
                glm::vec3 muMuSunNu = unmappingMuMuSunNu(r, dhdH, SAMPLES_MU, Rg, Rt,
                    SAMPLES_MU_S, SAMPLES_NU, x, y);

                float mu = muMuSunNu.x;
                float muSun = muMuSunNu.y;
                float nu = muMuSunNu.z;

                // Here we calculate the single inScattered light. Because this is a single
                // inscattering, the light that arrives at a point y in the path from the eye to the
                // infinity (top of atmosphere or planet's ground), comes only from the light source,
                // i.e., the sun. So, the there is no need to integrate over the whole solid angle
                // (4pi), we need only to consider the Sun position (cosine of sun pos = muSun). Then,
                // following the paper notation:
                // S[L] = P_R*S_R[L0] + P_M*S_M[L0] + S[L*]
                // For single inscattering only:
                // S[L0] = P_R*S_R[L0] + P_M*S_M[L0]
                // In order to save memory, we just store the red component of S_M[L0], and later we use
                // the proportionality rule to calcule the other components.
                //glm::vec3 S_R; // First Order Rayleigh InScattering
                //glm::vec3 S_M; // First Order Mie InScattering
                auto [S_R, S_M] = inscatter(r, mu, muSun, nu, Rt, Rg, transmittanceTexture,
                ozoneLayerEnabled, HO, HM, HR, betaRayleigh, betaMiescattering );

                deltaSRayleigh[layer].data[k] = S_R.r; //S_R.r;
                deltaSRayleigh[layer].data[k + 1] = S_R.g; // S_R.g;
                deltaSRayleigh[layer].data[k + 2] = S_R.b; // S_R.b;

                deltaSmie[layer].data[k] = S_M.r;
                deltaSmie[layer].data[k + 1] = S_M.g;
                deltaSmie[layer].data[k + 2] = S_M.b;

                k += 3;
            }
        }
    }
    return std::make_pair(deltaSRayleigh, deltaSmie);
}

std::pair<GLuint, GLuint> AtmosphereDeferredcaster::calculateDeltaS() {
    ZoneScoped;

    const GLuint deltaSRayleigh = createTexture(_textureSize, "DeltaS Rayleigh", 3);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, deltaSRayleigh, 0);
    const GLuint deltaSMie = createTexture(_textureSize, "DeltaS Mie", 3);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, deltaSMie, 0);
    std::array<GLenum, 2> colorBuffers = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, colorBuffers.data());
    glViewport(0, 0, _textureSize.x, _textureSize.y);
    using ProgramObject = ghoul::opengl::ProgramObject;
    std::unique_ptr<ProgramObject> program = ProgramObject::Build(
        "InScattering Program",
        absPath("${MODULE_ATMOSPHERE}/shaders/calculation_vs.glsl"),
        absPath("${MODULE_ATMOSPHERE}/shaders/inScattering_calc_fs.glsl"),
        absPath("${MODULE_ATMOSPHERE}/shaders/calculation_gs.glsl")
    );
    program->activate();
    ghoul::opengl::TextureUnit unit;
    unit.activate();
    glBindTexture(GL_TEXTURE_2D, _transmittanceTableTexture);
    program->setUniform("transmittanceTexture", unit);
    program->setUniform("Rg", _atmospherePlanetRadius);
    program->setUniform("Rt", _atmosphereRadius);
    program->setUniform("HR", _rayleighHeightScale);
    program->setUniform("betaRayleigh", _rayleighScatteringCoeff);
    program->setUniform("HM", _mieHeightScale);
    program->setUniform("betaMieScattering", _mieScatteringCoeff);
    program->setUniform("SAMPLES_MU_S", _muSSamples);
    program->setUniform("SAMPLES_NU", _nuSamples);
    program->setUniform("SAMPLES_MU", _muSamples);
    program->setUniform("ozoneLayerEnabled", _ozoneEnabled);
    program->setUniform("HO", _ozoneHeightScale);
    glClear(GL_COLOR_BUFFER_BIT);
    for (int layer = 0; layer < _rSamples; ++layer) {
        program->setUniform("layer", layer);
        step3DTexture(*program, layer);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }
    if (_saveCalculationTextures) {
        saveTextureFile("deltaS_rayleigh_texture.ppm", glm::ivec2(_textureSize));
        saveTextureFile<GL_COLOR_ATTACHMENT1>(
            "deltaS_mie_texture.ppm",
            glm::ivec2(_textureSize)
        );
    }

    if (_saveCalculationTextures) {
        auto [deltaSRayleigh, deltaSMie] = computeDeltaS(_textureSize,
            _transmittanceTexture, _atmospherePlanetRadius, _atmosphereRadius,
            _rayleighHeightScale, _rayleighScatteringCoeff, _mieHeightScale,
            _mieScatteringCoeff, _muSSamples, _nuSamples, _muSamples, _ozoneEnabled,
            _ozoneHeightScale
        );

        saveTextureFile("my_deltaS_rayleigh_texture_test.ppm", deltaSRayleigh[0]);
        saveTextureFile("my_deltaS_mie_texture_test.ppm", deltaSMie[0]);
    }


    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, 0, 0);
    const std::array<GLenum, 1> drawBuffers = { GL_COLOR_ATTACHMENT0 };
    glDrawBuffers(1, drawBuffers.data());

    program->deactivate();
    return { deltaSRayleigh, deltaSMie };
}

void AtmosphereDeferredcaster::calculateIrradiance() {
    ZoneScoped;

    glFramebufferTexture(
        GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0,
        _irradianceTableTexture,
        0
    );
    glDrawBuffer(GL_COLOR_ATTACHMENT0);

    glViewport(0, 0, _deltaETableSize.x, _deltaETableSize.y);
    using ProgramObject = ghoul::opengl::ProgramObject;
    std::unique_ptr<ProgramObject> program = ProgramObject::Build(
        "DeltaE Program",
        absPath("${MODULE_ATMOSPHERE}/shaders/calculation_vs.glsl"),
        absPath("${MODULE_ATMOSPHERE}/shaders/deltaE_calc_fs.glsl")
    );
    program->activate();
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    if (_saveCalculationTextures) {
        saveTextureFile("irradiance_texture.ppm", _deltaETableSize);
    }
    program->deactivate();
}

void AtmosphereDeferredcaster::calculateInscattering(GLuint deltaSRayleigh,
                                                     GLuint deltaSMie)
{
    ZoneScoped;

    glFramebufferTexture(
        GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0,
        _inScatteringTableTexture,
        0
    );
    glViewport(0, 0, _textureSize.x, _textureSize.y);
    using ProgramObject = ghoul::opengl::ProgramObject;
    std::unique_ptr<ProgramObject> program = ProgramObject::Build(
        "deltaSCalcProgram",
        absPath("${MODULE_ATMOSPHERE}/shaders/calculation_vs.glsl"),
        absPath("${MODULE_ATMOSPHERE}/shaders/deltaS_calc_fs.glsl"),
        absPath("${MODULE_ATMOSPHERE}/shaders/calculation_gs.glsl")
    );
    program->activate();

    ghoul::opengl::TextureUnit deltaSRayleighUnit;
    deltaSRayleighUnit.activate();
    glBindTexture(GL_TEXTURE_3D, deltaSRayleigh);
    program->setUniform("deltaSRTexture", deltaSRayleighUnit);

    ghoul::opengl::TextureUnit deltaSMieUnit;
    deltaSMieUnit.activate();
    glBindTexture(GL_TEXTURE_3D, deltaSMie);
    program->setUniform("deltaSMTexture", deltaSMieUnit);

    program->setUniform("SAMPLES_MU_S", _muSSamples);
    program->setUniform("SAMPLES_NU", _nuSamples);
    program->setUniform("SAMPLES_MU", _muSamples);
    program->setUniform("SAMPLES_R", _rSamples);
    glClear(GL_COLOR_BUFFER_BIT);
    for (int layer = 0; layer < _rSamples; ++layer) {
        program->setUniform("layer", layer);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }
    if (_saveCalculationTextures) {
        saveTextureFile("S_texture.ppm", glm::ivec2(_textureSize));
    }
    program->deactivate();
}

void AtmosphereDeferredcaster::calculateDeltaJ(int scatteringOrder,
                                               ghoul::opengl::ProgramObject& program,
                                               GLuint deltaJ, GLuint deltaE,
                                               GLuint deltaSRayleigh, GLuint deltaSMie)
{
    ZoneScoped;

    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, deltaJ, 0);
    glViewport(0, 0, _textureSize.x, _textureSize.y);
    program.activate();

    ghoul::opengl::TextureUnit transmittanceUnit;
    transmittanceUnit.activate();
    glBindTexture(GL_TEXTURE_2D, _transmittanceTableTexture);
    program.setUniform("transmittanceTexture", transmittanceUnit);

    ghoul::opengl::TextureUnit deltaEUnit;
    deltaEUnit.activate();
    glBindTexture(GL_TEXTURE_2D, deltaE);
    program.setUniform("deltaETexture", deltaEUnit);

    ghoul::opengl::TextureUnit deltaSRayleighUnit;
    deltaSRayleighUnit.activate();
    glBindTexture(GL_TEXTURE_3D, deltaSRayleigh);
    program.setUniform("deltaSRTexture", deltaSRayleighUnit);

    ghoul::opengl::TextureUnit deltaSMieUnit;
    deltaSMieUnit.activate();
    glBindTexture(GL_TEXTURE_3D, deltaSMie);
    program.setUniform("deltaSMTexture", deltaSMieUnit);

    program.setUniform("firstIteration", (scatteringOrder == 2) ? 1 : 0);
    program.setUniform("Rg", _atmospherePlanetRadius);
    program.setUniform("Rt", _atmosphereRadius);
    program.setUniform("AverageGroundReflectance", _averageGroundReflectance);
    program.setUniform("HR", _rayleighHeightScale);
    program.setUniform("betaRayleigh", _rayleighScatteringCoeff);
    program.setUniform("HM", _mieHeightScale);
    program.setUniform("betaMieScattering", _mieScatteringCoeff);
    program.setUniform("mieG", _miePhaseConstant);
    program.setUniform("SAMPLES_MU_S", _muSSamples);
    program.setUniform("SAMPLES_NU", _nuSamples);
    program.setUniform("SAMPLES_MU", _muSamples);
    program.setUniform("SAMPLES_R", _rSamples);
    for (int layer = 0; layer < _rSamples; ++layer) {
        program.setUniform("layer", layer);
        step3DTexture(program, layer);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }
    if (_saveCalculationTextures) {
        saveTextureFile(
            std::format("deltaJ_texture-scattering_order-{}.ppm", scatteringOrder),
            glm::ivec2(_textureSize)
        );
    }
    program.deactivate();
}

void AtmosphereDeferredcaster::calculateDeltaE(int scatteringOrder,
                                               ghoul::opengl::ProgramObject& program,
                                               GLuint deltaE, GLuint deltaSRayleigh,
                                               GLuint deltaSMie)
{
    ZoneScoped;

    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, deltaE, 0);
    glViewport(0, 0, _deltaETableSize.x, _deltaETableSize.y);
    program.activate();

    ghoul::opengl::TextureUnit deltaSRayleighUnit;
    deltaSRayleighUnit.activate();
    glBindTexture(GL_TEXTURE_3D, deltaSRayleigh);
    program.setUniform("deltaSRTexture", deltaSRayleighUnit);

    ghoul::opengl::TextureUnit deltaSMieUnit;
    deltaSMieUnit.activate();
    glBindTexture(GL_TEXTURE_3D, deltaSMie);
    program.setUniform("deltaSMTexture", deltaSMieUnit);

    program.setUniform("firstIteration", (scatteringOrder == 2) ? 1 : 0);
    program.setUniform("Rg", _atmospherePlanetRadius);
    program.setUniform("Rt", _atmosphereRadius);
    program.setUniform("mieG", _miePhaseConstant);
    program.setUniform("SKY", _irradianceTableSize);
    program.setUniform("SAMPLES_MU_S", _muSSamples);
    program.setUniform("SAMPLES_NU", _nuSamples);
    program.setUniform("SAMPLES_MU", _muSamples);
    program.setUniform("SAMPLES_R", _rSamples);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    if (_saveCalculationTextures) {
        saveTextureFile(
            std::format("deltaE_texture-scattering_order-{}.ppm", scatteringOrder),
            _deltaETableSize
        );
    }
    program.deactivate();
}

void AtmosphereDeferredcaster::calculateDeltaS(int scatteringOrder,
                                               ghoul::opengl::ProgramObject& program,
                                               GLuint deltaSRayleigh, GLuint deltaJ)
{
    ZoneScoped;

    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, deltaSRayleigh, 0);
    glViewport(0, 0, _textureSize.x, _textureSize.y);
    program.activate();

    ghoul::opengl::TextureUnit transmittanceUnit;
    transmittanceUnit.activate();
    glBindTexture(GL_TEXTURE_2D, _transmittanceTableTexture);
    program.setUniform("transmittanceTexture", transmittanceUnit);

    ghoul::opengl::TextureUnit deltaJUnit;
    deltaJUnit.activate();
    glBindTexture(GL_TEXTURE_3D, deltaJ);
    program.setUniform("deltaJTexture", deltaJUnit);

    program.setUniform("Rg", _atmospherePlanetRadius);
    program.setUniform("Rt", _atmosphereRadius);
    program.setUniform("SAMPLES_MU_S", _muSSamples);
    program.setUniform("SAMPLES_NU", _nuSamples);
    program.setUniform("SAMPLES_MU", _muSamples);
    program.setUniform("SAMPLES_R", _rSamples);
    for (int layer = 0; layer < _rSamples; ++layer) {
        program.setUniform("layer", layer);
        step3DTexture(program, layer);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }
    if (_saveCalculationTextures) {
        saveTextureFile(
            std::format("deltaS_texture-scattering_order-{}.ppm", scatteringOrder),
            glm::ivec2(_textureSize)
        );
    }
    program.deactivate();
}

void AtmosphereDeferredcaster::calculateIrradiance(int scatteringOrder,
                                                   ghoul::opengl::ProgramObject& program,
                                                   GLuint deltaE)
{
    ZoneScoped;

    glFramebufferTexture(
        GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0,
        _irradianceTableTexture,
        0
    );
    glViewport(0, 0, _deltaETableSize.x, _deltaETableSize.y);
    program.activate();

    ghoul::opengl::TextureUnit unit;
    unit.activate();
    glBindTexture(GL_TEXTURE_2D, deltaE);
    program.setUniform("deltaETexture", unit);
    program.setUniform("OTHER_TEXTURES", _deltaETableSize);

    glDrawArrays(GL_TRIANGLES, 0, 6);
    if (_saveCalculationTextures) {
        saveTextureFile(
            std::format("irradianceTable_order-{}.ppm", scatteringOrder),
            _deltaETableSize
        );
    }
    program.deactivate();
}

void AtmosphereDeferredcaster::calculateInscattering(int scatteringOrder,
                                                    ghoul::opengl::ProgramObject& program,
                                                                    GLuint deltaSRayleigh)

{
    ZoneScoped;

    glFramebufferTexture(
        GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0,
        _inScatteringTableTexture,
        0
    );
    glViewport(0, 0, _textureSize.x, _textureSize.y);
    program.activate();

    ghoul::opengl::TextureUnit unit;
    unit.activate();
    glBindTexture(GL_TEXTURE_3D, deltaSRayleigh);
    program.setUniform("deltaSTexture", unit);
    program.setUniform("SAMPLES_MU_S", _muSSamples);
    program.setUniform("SAMPLES_NU", _nuSamples);
    program.setUniform("SAMPLES_MU", _muSamples);
    program.setUniform("SAMPLES_R", _rSamples);
    for (int layer = 0; layer < _rSamples; ++layer) {
        program.setUniform("layer", layer);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }
    if (_saveCalculationTextures) {
        saveTextureFile(
            std::format("inscatteringTable_order-{}.ppm", scatteringOrder),
            glm::ivec2(_textureSize)
        );
    }
    program.deactivate();
}

void AtmosphereDeferredcaster::calculateAtmosphereParameters() {
    ZoneScoped;

    using ProgramObject = ghoul::opengl::ProgramObject;
    std::unique_ptr<ProgramObject> deltaJProgram = ProgramObject::Build(
        "DeltaJ Program",
        absPath("${MODULE_ATMOSPHERE}/shaders/calculation_vs.glsl"),
        absPath("${MODULE_ATMOSPHERE}/shaders/deltaJ_calc_fs.glsl"),
        absPath("${MODULE_ATMOSPHERE}/shaders/calculation_gs.glsl")
    );
    std::unique_ptr<ProgramObject> irradianceSupTermsProgram = ProgramObject::Build(
        "IrradianceSupTerms Program",
        absPath("${MODULE_ATMOSPHERE}/shaders/calculation_vs.glsl"),
        absPath("${MODULE_ATMOSPHERE}/shaders/irradiance_sup_calc_fs.glsl")
    );
    std::unique_ptr<ProgramObject> inScatteringSupTermsProgram = ProgramObject::Build(
        "InScatteringSupTerms Program",
        absPath("${MODULE_ATMOSPHERE}/shaders/calculation_vs.glsl"),
        absPath("${MODULE_ATMOSPHERE}/shaders/inScattering_sup_calc_fs.glsl"),
        absPath("${MODULE_ATMOSPHERE}/shaders/calculation_gs.glsl")
    );
    std::unique_ptr<ProgramObject> irradianceFinalProgram = ProgramObject::Build(
        "IrradianceEFinal Program",
        absPath("${MODULE_ATMOSPHERE}/shaders/calculation_vs.glsl"),
        absPath("${MODULE_ATMOSPHERE}/shaders/irradiance_final_fs.glsl")
    );
    std::unique_ptr<ProgramObject> deltaSSupTermsProgram = ProgramObject::Build(
        "DeltaSSUPTerms Program",
        absPath("${MODULE_ATMOSPHERE}/shaders/calculation_vs.glsl"),
        absPath("${MODULE_ATMOSPHERE}/shaders/deltaS_sup_calc_fs.glsl"),
        absPath("${MODULE_ATMOSPHERE}/shaders/calculation_gs.glsl")
    );


    // Saves current FBO first
    GLint defaultFBO = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &defaultFBO);

    std::array<GLint, 4> viewport;
    global::renderEngine->openglStateCache().viewport(viewport.data());

    // Creates the FBO for the calculations
    GLuint calcFBO = 0;
    glGenFramebuffers(1, &calcFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, calcFBO);
    std::array<GLenum, 1> drawBuffers = { GL_COLOR_ATTACHMENT0 };
    glDrawBuffers(1, drawBuffers.data());

    // Prepare for rendering/calculations
    GLuint quadVao = 0;
    glGenVertexArrays(1, &quadVao);
    glBindVertexArray(quadVao);
    GLuint quadVbo = 0;
    glGenBuffers(1, &quadVbo);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo);

    constexpr std::array<GLfloat, 12> VertexData = {
        // x     y
        -1.f, -1.f,
         1.f,  1.f,
        -1.f,  1.f,
        -1.f, -1.f,
         1.f, -1.f,
         1.f,  1.f,
    };

    glBufferData(GL_ARRAY_BUFFER, sizeof(VertexData), VertexData.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), nullptr);

    // Execute Calculations
    LDEBUG("Starting precalculations for scattering effects");
    glDisable(GL_BLEND);

    // See Precomputed Atmosphere Scattering from Bruneton et al. paper, algorithm 4.1:
    calculateTransmittance();

    // line 2 in algorithm 4.1
    const GLuint deltaETable = calculateDeltaE();

    // line 3 in algorithm 4.1
    auto [deltaSRayleighTable, deltaSMieTable] = calculateDeltaS();

    // line 4 in algorithm 4.1
    calculateIrradiance();

    // line 5 in algorithm 4.1
    calculateInscattering(deltaSRayleighTable, deltaSMieTable);

    const GLuint deltaJTable = createTexture(_textureSize, "DeltaJ", 3);

    // loop in line 6 in algorithm 4.1
    for (int scatteringOrder = 2; scatteringOrder <= 4; ++scatteringOrder) {
        // line 7 in algorithm 4.1
        calculateDeltaJ(
            scatteringOrder,
            *deltaJProgram,
            deltaJTable,
            deltaETable,
            deltaSRayleighTable,
            deltaSMieTable
        );

        // line 8 in algorithm 4.1
        calculateDeltaE(
            scatteringOrder,
            *irradianceSupTermsProgram,
            deltaETable,
            deltaSRayleighTable,
            deltaSMieTable
        );

        // line 9 in algorithm 4.1
        calculateDeltaS(
            scatteringOrder,
            *inScatteringSupTermsProgram,
            deltaSRayleighTable,
            deltaJTable
        );

        glEnable(GL_BLEND);
        glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
        glBlendFuncSeparate(GL_ONE, GL_ONE, GL_ONE, GL_ONE);

        // line 10 in algorithm 4.1
        calculateIrradiance(
            scatteringOrder,
            *irradianceFinalProgram,
            deltaETable
        );

        // line 11 in algorithm 4.1
        calculateInscattering(
            scatteringOrder,
            *deltaSSupTermsProgram,
            deltaSRayleighTable
        );

        glDisable(GL_BLEND);
    }

    // Restores OpenGL blending state
    global::renderEngine->openglStateCache().resetBlendState();

    glDeleteTextures(1, &deltaETable);
    glDeleteTextures(1, &deltaSRayleighTable);
    glDeleteTextures(1, &deltaSMieTable);
    glDeleteTextures(1, &deltaJTable);

    // Restores system state
    glBindFramebuffer(GL_FRAMEBUFFER, defaultFBO);
    global::renderEngine->openglStateCache().setViewportState(viewport.data());
    glDeleteBuffers(1, &quadVbo);
    glDeleteVertexArrays(1, &quadVao);
    glDeleteFramebuffers(1, &calcFBO);
    glBindVertexArray(0);

    LDEBUG("Ended precalculations for Atmosphere effects");
}

void AtmosphereDeferredcaster::step3DTexture(ghoul::opengl::ProgramObject& prg,
                                             int layer) const
{
    // See OpenGL redbook 8th Edition page 556 for Layered Rendering
    const float planet2 = _atmospherePlanetRadius * _atmospherePlanetRadius;
    const float diff = _atmosphereRadius * _atmosphereRadius - planet2;
    const float ri = static_cast<float>(layer) / static_cast<float>(_rSamples - 1);
    float eps = 0.01f;
    if (layer > 0) {
        if (layer == (_rSamples - 1)) {
            eps = -0.001f;
        }
        else {
            eps = 0.f;
        }
    }
    const float r = std::sqrt(planet2 + ri * ri * diff) + eps;
    const float dminG = r - _atmospherePlanetRadius;
    const float dminT = _atmosphereRadius - r;
    const float dh = std::sqrt(r * r - planet2);
    const float dH = dh + std::sqrt(diff);

    prg.setUniform("r", r);
    prg.setUniform("dhdH", dminT, dH, dminG, dh);
}

} // namespace openspace
