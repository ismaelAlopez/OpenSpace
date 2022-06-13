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

#include <modules/skybrowser/include/targetbrowserpair.h>

#include <modules/skybrowser/include/renderableskytarget.h>
#include <modules/skybrowser/include/screenspaceskybrowser.h>
#include <modules/skybrowser/include/utility.h>
#include <modules/skybrowser/include/wwtdatahandler.h>
#include <modules/skybrowser/skybrowsermodule.h>
#include <openspace/camera/camera.h>
#include <openspace/engine/globals.h>
#include <openspace/engine/moduleengine.h>
#include <openspace/navigation/navigationhandler.h>
#include <openspace/rendering/screenspacerenderable.h>
#include <openspace/scripting/scriptengine.h>
#include <ghoul/misc/assert.h>
#include <glm/gtc/constants.hpp>
#include <functional>
#include <chrono>

namespace openspace {

TargetBrowserPair::TargetBrowserPair(SceneGraphNode* targetNode,
                                     ScreenSpaceSkyBrowser* browser)
    : _targetNode(targetNode)
    , _browser(browser)
{
    ghoul_assert(browser, "Sky browser is null pointer");
    ghoul_assert(targetNode, "Sky target is null pointer");

    _targetRenderable = dynamic_cast<RenderableSkyTarget*>(_targetNode->renderable());
}

TargetBrowserPair& TargetBrowserPair::operator=(TargetBrowserPair other) {
    std::swap(_targetNode, other._targetNode);
    std::swap(_browser, other._browser);
    return *this;
}

void TargetBrowserPair::setImageOrder(int i, int order) {
    _browser->setImageOrder(i, order);
}

void TargetBrowserPair::aimTargetGalactic(glm::dvec3 direction) {
    std::string id = _targetNode->identifier();
    glm::dvec3 positionCelestial = glm::normalize(direction) *
        skybrowser::CelestialSphereRadius;

    std::string script = fmt::format(
        "openspace.setPropertyValueSingle('Scene.{}.Translation.Position', {});",
        id, ghoul::to_string(positionCelestial)
    );
    openspace::global::scriptEngine->queueScript(
        script,
        scripting::ScriptEngine::RemoteScripting::Yes
    );
}

void TargetBrowserPair::startFinetuningTarget() {
    _startTargetPosition = _targetNode->worldPosition();
}

// The fine tune of the target is a way to "drag and drop" the target with right click
// drag on the sky browser window. This is to be able to drag the target around when it
// has a very small field of view
void TargetBrowserPair::fineTuneTarget(const glm::vec2& startMouse,
                                       const glm::vec2& translation)
{
    glm::vec2 fineTune = _browser->fineTuneVector(translation);
    glm::vec2 endMouse = startMouse + fineTune;

    // Translation world
    glm::dvec3 startWorld = skybrowser::localCameraToGalactic(
        glm::vec3(startMouse, skybrowser::ScreenSpaceZ)
    );
    glm::dvec3 endWorld = skybrowser::localCameraToGalactic(
        glm::vec3(endMouse, skybrowser::ScreenSpaceZ)
    );

    glm::dvec3 translationWorld = endWorld - startWorld;
    aimTargetGalactic(_startTargetPosition + translationWorld);
}

void TargetBrowserPair::synchronizeAim() {
    if (!_targetAnimation.isAnimating() && _browser->isInitialized()) {
        _browser->setEquatorialAim(targetDirectionEquatorial());
        _browser->setTargetRoll(targetRoll());
        _targetRenderable->setVerticalFov(_browser->verticalFov());
    }
}

void TargetBrowserPair::setEnabled(bool enable) {
    _browser->setEnabled(enable);
    _targetRenderable->property("Enabled")->set(enable);
}

void TargetBrowserPair::setOpacity(float opacity) {
    _browser->property("Opacity")->set(opacity);
    _targetRenderable->property("Opacity")->set(opacity);
}

bool TargetBrowserPair::isEnabled() const {
    return _targetRenderable->isEnabled() || _browser->isEnabled();
}

void TargetBrowserPair::initialize() {
    _targetRenderable->setColor(_browser->borderColor());
    glm::vec2 dim = _browser->screenSpaceDimensions();
    _targetRenderable->setRatio(dim.x / dim.y);
    _browser->updateBorderColor();
    _browser->hideChromeInterface();
    _browser->setIsInitialized(true);
}

glm::ivec3 TargetBrowserPair::borderColor() const {
    return _browser->borderColor();
}

glm::dvec2 TargetBrowserPair::targetDirectionEquatorial() const {
    glm::dvec3 cartesian = skybrowser::galacticToEquatorial(
        glm::normalize(_targetNode->worldPosition())
    );
    return skybrowser::cartesianToSpherical(cartesian);
}

glm::dvec3 TargetBrowserPair::targetDirectionGalactic() const {
    return glm::normalize(_targetNode->worldPosition());
}

std::string TargetBrowserPair::browserGuiName() const {
    return _browser->guiName();
}

std::string TargetBrowserPair::browserId() const {
    return _browser->identifier();
}

std::string TargetBrowserPair::targetRenderableId() const {
    return _targetRenderable->identifier();
}

std::string TargetBrowserPair::targetNodeId() const {
    return _targetNode->identifier();
}

float TargetBrowserPair::browserRatio() const {
    return _browser->browserRatio();
}

double TargetBrowserPair::verticalFov() const {
    return _browser->verticalFov();
}

std::vector<int> TargetBrowserPair::selectedImages() const {
    return _browser->selectedImages();
}

ghoul::Dictionary TargetBrowserPair::dataAsDictionary() const {
    glm::dvec2 spherical = targetDirectionEquatorial();
    glm::dvec3 cartesian = skybrowser::sphericalToCartesian(spherical);

    ghoul::Dictionary res;
    res.setValue("id", browserId());
    res.setValue("name", browserGuiName());
    res.setValue("fov", static_cast<double>(verticalFov()));
    res.setValue("ra", spherical.x);
    res.setValue("dec", spherical.y);
    res.setValue("roll", targetRoll());
    res.setValue("color", borderColor());
    res.setValue("cartesianDirection", cartesian);
    res.setValue("ratio", static_cast<double>(browserRatio()));
    res.setValue("isFacingCamera", isFacingCamera());
    res.setValue("isUsingRae", isUsingRadiusAzimuthElevation());
    res.setValue("selectedImages", selectedImages());
    res.setValue("scale", static_cast<double>(_browser->scale()));
    res.setValue("opacities", _browser->opacities());

    std::vector<std::pair<std::string, glm::dvec3>> copies = displayCopies();
    std::vector<std::pair<std::string, bool>> showCopies = _browser->showDisplayCopies();
    ghoul::Dictionary copiesData;
    for (size_t i = 0; i < copies.size(); i++) {
        ghoul::Dictionary copy;
        copy.setValue("position", copies[i].second);
        copy.setValue("show", showCopies[i].second);
        copy.setValue("idShowProperty", showCopies[i].first);
        copiesData.setValue(copies[i].first, copy);
    }
    // Set table for the current target
    res.setValue("displayCopies", copiesData);

    return res;
}

void TargetBrowserPair::selectImage(const ImageData& image, int i) {
    // Load image into browser
    _browser->selectImage(image.imageUrl, i);

    // If the image has coordinates, move the target
    if (image.hasCelestialCoords) {
        // Animate the target to the image coordinate position
        glm::dvec3 galactic = skybrowser::equatorialToGalactic(image.equatorialCartesian);
        startAnimation(galactic * skybrowser::CelestialSphereRadius, image.fov);
    }
}

void TargetBrowserPair::addImageLayerToWwt(const std::string& url, int i) {
    _browser->addImageLayerToWwt(url, i);
}

void TargetBrowserPair::removeSelectedImage(int i) {
    _browser->removeSelectedImage(i);
}

void TargetBrowserPair::loadImageCollection(const std::string& collection) {
    _browser->loadImageCollection(collection);
}

void TargetBrowserPair::setImageOpacity(int i, float opacity) {
    _browser->setImageOpacity(i, opacity);
}

void TargetBrowserPair::hideChromeInterface() {
    _browser->hideChromeInterface();
}

void TargetBrowserPair::sendIdToBrowser() const {
    _browser->setIdInBrowser();
}

void TargetBrowserPair::updateBrowserSize() {
    _browser->updateBrowserSize();
}

std::vector<std::pair<std::string, glm::dvec3>> TargetBrowserPair::displayCopies() const {
    return _browser->displayCopies();
}

bool TargetBrowserPair::isImageCollectionLoaded() {
    return _browser->isImageCollectionLoaded();
}

void TargetBrowserPair::setVerticalFov(double vfov) {
    _browser->setVerticalFov(vfov);
    _targetRenderable->setVerticalFov(vfov);
}

void TargetBrowserPair::setEquatorialAim(const glm::dvec2& aim) {
    aimTargetGalactic(
        skybrowser::equatorialToGalactic(skybrowser::sphericalToCartesian(aim))
    );
    _browser->setEquatorialAim(aim);
}

void TargetBrowserPair::setBorderColor(const glm::ivec3& color) {
    _targetRenderable->setColor(color);
    _browser->setBorderColor(color);
}

void TargetBrowserPair::setBrowserRatio(float ratio) {
    _browser->setRatio(ratio);
    _targetRenderable->setRatio(ratio);
}

void TargetBrowserPair::setVerticalFovWithScroll(float scroll) {
    _browser->setVerticalFovWithScroll(scroll);
}

void TargetBrowserPair::setImageCollectionIsLoaded(bool isLoaded) {
    _browser->setImageCollectionIsLoaded(isLoaded);
}

void TargetBrowserPair::incrementallyAnimateToCoordinate() {
    // Animate the target before the field of view starts to animate
    if (_targetAnimation.isAnimating()) {
        aimTargetGalactic(_targetAnimation.getNewValue());
    }
    else if (!_targetAnimation.isAnimating() && _targetIsAnimating) {
        // Set the finished position
        aimTargetGalactic(_targetAnimation.getNewValue());
        _fovAnimation.start();
        _targetIsAnimating = false;
    }
    if (_fovAnimation.isAnimating()) {
        _browser->setVerticalFov(_fovAnimation.getNewValue());
        _targetRenderable->setVerticalFov(_browser->verticalFov());
    }
}

void TargetBrowserPair::startFading(float goal, float fadeTime) {
    const std::string script = fmt::format(
        "openspace.setPropertyValueSingle('Scene.{0}.Renderable.Fade', {2}, {3});"
        "openspace.setPropertyValueSingle('ScreenSpace.{1}.Fade', {2}, {3});",
        _targetNode->identifier(), _browser->identifier(), goal, fadeTime
    );

    global::scriptEngine->queueScript(
        script,
        scripting::ScriptEngine::RemoteScripting::Yes
    );
}

void TargetBrowserPair::stopAnimations() {
    _fovAnimation.stop();
    _targetAnimation.stop();
}

void TargetBrowserPair::startAnimation(glm::dvec3 galacticCoords, double fovEnd) {
    SkyBrowserModule* module = global::moduleEngine->module<SkyBrowserModule>();
    double fovSpeed = module->browserAnimationSpeed();
    // The speed is given degrees /sec
    double fovTime = abs(_browser->verticalFov() - fovEnd) / fovSpeed;
    // Fov animation
    _fovAnimation = skybrowser::Animation(_browser->verticalFov(), fovEnd, fovTime);

    // Target animation
    glm::dvec3 start = glm::normalize(_targetNode->worldPosition()) *
        skybrowser::CelestialSphereRadius;
    double targetSpeed = module->targetAnimationSpeed();
    double angle = skybrowser::angleBetweenVectors(start, galacticCoords);
    _targetAnimation = skybrowser::Animation(start, galacticCoords, angle / targetSpeed);
    _targetAnimation.start();
    _targetIsAnimating = true;
}

void TargetBrowserPair::centerTargetOnScreen() {
    // Get camera direction in celestial spherical coordinates
    glm::dvec3 viewDirection = skybrowser::cameraDirectionGalactic();
    // Keep the current fov
    double currentFov = verticalFov();
    startAnimation(viewDirection, currentFov);
}

double TargetBrowserPair::targetRoll() const {
    // To remove the lag effect when moving the camera while having a locked
    // target, send the locked coordinates to wwt
    glm::dvec3 normal = glm::normalize(
        _targetNode->worldPosition() -
        global::navigationHandler->camera()->positionVec3()
    );
    glm::dvec3 right = glm::normalize(
        glm::cross(
            global::navigationHandler->camera()->lookUpVectorWorldSpace(),
            normal
        )
    );
    glm::dvec3 up = glm::normalize(glm::cross(normal, right));
    return skybrowser::targetRoll(up, normal);
}

bool TargetBrowserPair::isFacingCamera() const {
    return _browser->isFacingCamera();
}

bool TargetBrowserPair::isUsingRadiusAzimuthElevation() const {
    return _browser->isUsingRaeCoords();
}

SceneGraphNode* TargetBrowserPair::targetNode() const {
    return _targetNode;
}

ScreenSpaceSkyBrowser* TargetBrowserPair::browser() const {
    return _browser;
}

bool operator==(const TargetBrowserPair& lhs, const TargetBrowserPair& rhs) {
    return lhs._targetNode == rhs._targetNode && lhs._browser == rhs._browser;
}

bool operator!=(const TargetBrowserPair& lhs, const TargetBrowserPair& rhs) {
    return !(lhs == rhs);
}

} // namespace openspace