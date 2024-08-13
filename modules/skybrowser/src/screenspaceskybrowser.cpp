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

#include <modules/skybrowser/include/screenspaceskybrowser.h>

#include <modules/skybrowser/skybrowsermodule.h>
#include <modules/skybrowser/include/utility.h>
#include <openspace/engine/globals.h>
#include <openspace/engine/moduleengine.h>
#include <openspace/engine/windowdelegate.h>
#include <openspace/rendering/renderengine.h>
#include <ghoul/logging/logmanager.h>
#include <ghoul/misc/dictionaryjsonformatter.h>
#include <ghoul/opengl/texture.h>
#include <optional>

namespace {
    constexpr openspace::properties::Property::PropertyInfo TextureQualityInfo = {
        "TextureQuality",
        "Quality of Texture",
        "A parameter to set the resolution of the texture. 1 is full resolution and "
        "slower frame rate. Lower value means lower resolution of texture and faster "
        "frame rate.",
        openspace::properties::Property::Visibility::AdvancedUser
    };

    constexpr openspace::properties::Property::PropertyInfo DisplayCopyInfo = {
        "DisplayCopy",
        "Display Copy Position",
        "Display a copy of this sky browser at an additional position. This copy will "
        "not be interactive. The position is in RAE (Radius, Azimuth, Elevation) "
        "coordinates or Cartesian, depending on if the browser uses RAE or Cartesian "
        "coordinates.",
        openspace::properties::Property::Visibility::User
    };

    constexpr openspace::properties::Property::PropertyInfo DisplayCopyShowInfo = {
        "ShowDisplayCopy",
        "Show Display Copy",
        "Show the display copy.",
        openspace::properties::Property::Visibility::AdvancedUser
    };

    constexpr openspace::properties::Property::PropertyInfo IsHiddenInfo = {
        "IsHidden",
        "Is Hidden",
        "If checked, the browser will be not be displayed. If it is not checked, it will "
        "be.",
        openspace::properties::Property::Visibility::AdvancedUser
    };

    constexpr openspace::properties::Property::PropertyInfo VerticalFovInfo = {
        "VerticalFov",
        "Vertical Field Of View",
        "The vertical field of view of the target.",
        openspace::properties::Property::Visibility::AdvancedUser
    };

    constexpr openspace::properties::Property::PropertyInfo SelectedImagesUrlsInfo = {
        "SelectedImagesUrls",
        "Selected Images Urls",
        "Urls of the images that have been selected for this Sky Browser.",
        openspace::properties::Property::Visibility::User
    };

    constexpr openspace::properties::Property::PropertyInfo SelectedImagesOpacitiesInfo = {
        "SelectedImagesOpacities",
        "Selected Images Opacities",
        "Opacities of the images that have been selected for this Sky Browser.",
        openspace::properties::Property::Visibility::AdvancedUser
    };

    constexpr openspace::properties::Property::PropertyInfo RollInfo = {
        "Roll",
        "Roll",
        "The roll of the sky browser view.",
        openspace::properties::Property::Visibility::AdvancedUser
    };

    constexpr openspace::properties::Property::PropertyInfo EquatorialAimInfo = {
        "EquatorialAim",
        "Equatorial Aim",
        "The aim of the Sky Browser, given in equatorial coordinates Right Ascension "
        "(Ra) and declination (dec).",
        openspace::properties::Property::Visibility::AdvancedUser
    };

    constexpr openspace::properties::Property::PropertyInfo RatioInfo = {
        "Ratio",
        "Ratio",
        "The ratio of the dimensions of the sky browser. This is defined as width "
        "divided by height.",
        openspace::properties::Property::Visibility::Developer
    };

    // This `ScreenSpaceRenderable` is used to display a screen space window showing the
    // integrated World Wide Telescope view. The view will be dynamically updated when
    // interacting with the view or with images in the SkyBrowser panel.
    //
    // A `ScreenSpaceSkyBrowser` should not be created from a `.asset` file, but is rather
    // created from interacting with the SkyBrowser user interface panel. If created in
    // an asset, it requires some extra scripting to work with the SkyBrowser feature.
    struct [[codegen::Dictionary(ScreenSpaceSkyBrowser)]] Parameters {
        // [[codegen::verbatim(TextureQualityInfo.description)]]
        std::optional<float> textureQuality;

        // [[codegen::verbatim(IsHiddenInfo.description)]]
        std::optional<bool> isHidden;

        // [[codegen::verbatim(VerticalFovInfo.description)]]
        std::optional<double> verticalFov;

        // [[codegen::verbatim(SelectedImagesUrlsInfo.description)]]
        std::optional<std::vector<std::string>> selectedImagesUrls;

        // [[codegen::verbatim(SelectedImagesOpacitiesInfo.description)]]
        std::optional<std::vector<double>> selectedImagesOpacities;

        // [[codegen::verbatim(RollInfo.description)]]
        std::optional<double> roll;

        // [[codegen::verbatim(EquatorialAimInfo.description)]]
        std::optional<glm::dvec2> equatorialAim;

        // [[codegen::verbatim(RatioInfo.description)]]
        std::optional<float> ratio;

    };
#include "screenspaceskybrowser_codegen.cpp"
} // namespace

namespace openspace {

documentation::Documentation ScreenSpaceSkyBrowser::Documentation() {
    return codegen::doc<Parameters>("skybrowser_screenspaceskybrowser");
}

ScreenSpaceSkyBrowser::ScreenSpaceSkyBrowser(const ghoul::Dictionary& dictionary)
    : ScreenSpaceRenderable(dictionary)
    , _wwtCommunicator(dictionary)
    , _selectedImagesUrls(SelectedImagesUrlsInfo)
    , _selectedImagesOpacities(SelectedImagesOpacitiesInfo)
    , _roll(RollInfo, 0.0, 0.0, 180.0)
    , _equatorialAim(EquatorialAimInfo,
        glm::dvec2(0.0), glm::dvec2(0.0, -90.0), glm::dvec2(360.0, 90.0)
    )
    , _verticalFov(VerticalFovInfo, 10.0, 0.00000000001, 70.0)
    , _textureQuality(TextureQualityInfo, 1.f, 0.25f, 1.f)
    , _isHidden(IsHiddenInfo, true)
    , _ratio(RatioInfo, 1.f, 0.01f, 1.0f)
{
    _identifier = makeUniqueIdentifier(_identifier);

    // Handle target dimension property
    const Parameters p = codegen::bake<Parameters>(dictionary);

    _textureQuality = p.textureQuality.value_or(_textureQuality);
    _isHidden = p.isHidden.value_or(_isHidden);
    _verticalFov = p.verticalFov.value_or(_verticalFov);
    _equatorialAim = p.equatorialAim.value_or(_equatorialAim);
    _roll = p.roll.value_or(_roll);
    _selectedImagesOpacities = p.selectedImagesOpacities.value_or(_selectedImagesOpacities);
    _selectedImagesUrls = p.selectedImagesUrls.value_or(_selectedImagesUrls);
    _ratio = p.ratio.value_or(_ratio);

    addProperty(_isHidden);
    addProperty(_textureQuality);
    addProperty(_verticalFov);
    addProperty(_equatorialAim);
    addProperty(_roll);
    addProperty(_selectedImagesOpacities);
    addProperty(_selectedImagesUrls);
    addProperty(_ratio);

    addPropertySubOwner(_wwtCommunicator);

    _useRadiusAzimuthElevation.onChange(
        [this]() {
            std::for_each(
                _displayCopies.begin(),
                _displayCopies.end(),
                [this](std::unique_ptr<properties::Vec3Property>& copy) {
                    if (_useRadiusAzimuthElevation) {
                        *copy = sphericalToRae(cartesianToSpherical(copy->value()));

                    }
                    else {
                        *copy = sphericalToCartesian(raeToSpherical(copy->value()));
                    }
                });
        });

    
    _wwtCommunicator.property("Reload")->onChange([this]() {
        _isImageCollectionLoaded = false;
        _isInitialized = false;
    });

    _textureQuality.onChange([this]() {
        updateTextureResolution();
    });

    _verticalFov.onChange([this]() {
        _equatorialAimIsDirty = true;
    });

    _equatorialAim.onChange([this]() {
        _equatorialAimIsDirty = true;
    });

    _ratio.onChange([this]() {
        _wwtCommunicator.setRatio(_ratio);
        updateTextureResolution();
    });
    
    _lastUpdateTime = std::chrono::system_clock::now();

    // Set the size of the renderable to the browser dimensions
    _objectSize = glm::ivec3(_wwtCommunicator.browserDimensions(), 1);
}

ScreenSpaceSkyBrowser::~ScreenSpaceSkyBrowser() {
    SkyBrowserModule* module = global::moduleEngine->module<SkyBrowserModule>();
    if (module && module->pair(identifier())) {
        module->removeTargetBrowserPair(identifier());
    }
}

bool ScreenSpaceSkyBrowser::initializeGL() {
    _wwtCommunicator.initializeGL();
    ScreenSpaceRenderable::initializeGL();
    return true;
}

bool ScreenSpaceSkyBrowser::isInitialized() const {
    return _isInitialized;
}

void ScreenSpaceSkyBrowser::setIdInBrowser() const {
    int currentNode = global::windowDelegate->currentNode();
    _wwtCommunicator.setIdInBrowser(std::format("{}_{}", identifier(), currentNode));
}

void ScreenSpaceSkyBrowser::setIsInitialized(bool isInitialized) {
    _isInitialized = isInitialized;
}

void ScreenSpaceSkyBrowser::updateTextureResolution() {
    // Can't divide by zero
    if (_lastTextureQuality < glm::epsilon<float>()) {
        return;
    }
    const float diffTextureQuality = _textureQuality / _lastTextureQuality;
    const glm::vec2 res = glm::vec2(_wwtCommunicator.browserDimensions()) * diffTextureQuality;
    _wwtCommunicator.setBrowserDimensions(glm::ivec2(res));
    _lastTextureQuality = _textureQuality.value();
    _objectSize = glm::ivec3(_wwtCommunicator.browserDimensions(), 1);
}

void ScreenSpaceSkyBrowser::addDisplayCopy(const glm::vec3& raePosition, int nCopies) {
    const size_t start = _displayCopies.size();
    for (int i = 0; i < nCopies; i++) {
        openspace::properties::Property::PropertyInfo info = DisplayCopyInfo;
        const float azimuth = i * glm::two_pi<float>() / nCopies;
        const glm::vec3 position = raePosition + glm::vec3(0.f, azimuth, 0.f);
        // @TODO(abock) I think the lifetime for this string is a bit tricky. I don't
        // think it will live long enough to be actually usable
        const std::string idDisplayCopy = "DisplayCopy" + std::to_string(start + i);
        info.identifier = idDisplayCopy.c_str();
        _displayCopies.push_back(
            std::make_unique<properties::Vec3Property>(
                info,
                position,
                glm::vec3(-4.f, -4.f, -10.f),
                glm::vec3(4.f, 4.f, glm::half_pi<float>())
            )
        );
        openspace::properties::Property::PropertyInfo showInfo = DisplayCopyShowInfo;
        // @TODO(abock) I think the lifetime for this string is a bit tricky. I don't
        // think it will live long enough to be actually usable
        const std::string idDispCpyVis = "ShowDisplayCopy" + std::to_string(start + i);
        showInfo.identifier = idDispCpyVis.c_str();
        _showDisplayCopies.push_back(
            std::make_unique<properties::BoolProperty>(showInfo, true)
        );
        addProperty(_displayCopies.back().get());
        addProperty(_showDisplayCopies.back().get());
    }
}

std::vector<std::string> ScreenSpaceSkyBrowser::selectedImages() const {
    std::vector<std::string> selectedImagesVector;
    selectedImagesVector.resize(_selectedImages.size());
    std::transform(
        _selectedImages.cbegin(),
        _selectedImages.cend(),
        selectedImagesVector.begin(),
        [](const std::pair<std::string, double>& image) { return image.first; }
    );
    return selectedImagesVector;
}


void ScreenSpaceSkyBrowser::setBorderColor(glm::ivec3 color) {
    _borderColorIsDirty = true;
    _borderColor = glm::vec3(color) / 255.f;
}

void ScreenSpaceSkyBrowser::removeDisplayCopy() {
    if (!_displayCopies.empty()) {
        removeProperty(_displayCopies.back().get());
        removeProperty(_showDisplayCopies.back().get());
        _displayCopies.pop_back();
        _showDisplayCopies.pop_back();
    }
}

std::vector<std::pair<std::string, glm::dvec3>>
ScreenSpaceSkyBrowser::displayCopies() const
{
    std::vector<std::pair<std::string, glm::dvec3>> vec;
    vec.reserve(_displayCopies.size());
    for (const std::unique_ptr<properties::Vec3Property>& copy : _displayCopies) {
        vec.emplace_back(copy->identifier(), copy->value());
    }
    return vec;
}

std::vector<std::pair<std::string, bool>>
ScreenSpaceSkyBrowser::showDisplayCopies() const
{
    std::vector<std::pair<std::string, bool>> vec;
    vec.reserve(_showDisplayCopies.size());
    for (const std::unique_ptr<properties::BoolProperty>& copy : _showDisplayCopies) {
        vec.emplace_back(copy->identifier(), copy->value());
    }
    return vec;
}

bool ScreenSpaceSkyBrowser::deinitializeGL() {
    ScreenSpaceRenderable::deinitializeGL();
    _wwtCommunicator.deinitializeGL();
    return true;
}

void ScreenSpaceSkyBrowser::render(const RenderData& renderData) {
    _wwtCommunicator.render();

    if (!_isHidden) {
        const glm::mat4 mat =
            globalRotationMatrix() *
            translationMatrix() *
            localRotationMatrix() *
            scaleMatrix();
        draw(mat, renderData);
    }

    // Render the display copies
    for (size_t i = 0; i < _displayCopies.size(); i++) {
        if (_showDisplayCopies[i]->value()) {
            glm::vec3 coordinates = _displayCopies[i]->value();
            if (_useRadiusAzimuthElevation) {
                coordinates = sphericalToCartesian(raeToSpherical(coordinates));
            }
            glm::mat4 localRotation = glm::mat4(1.f);
            if (_faceCamera) {
                localRotation = glm::inverse(glm::lookAt(
                    glm::vec3(0.f),
                    glm::normalize(coordinates),
                    glm::vec3(0.f, 1.f, 0.f)
                ));
            }

            const glm::mat4 mat =
                globalRotationMatrix() *
                glm::translate(glm::mat4(1.f), coordinates) *
                localRotation *
                scaleMatrix();
            draw(mat, renderData);
        }
    }
}

float ScreenSpaceSkyBrowser::browserRatio() const {
    return _wwtCommunicator.browserRatio();
}

void ScreenSpaceSkyBrowser::selectImage(const std::string& url) {
    // Ensure there are no duplicates
    auto it = findSelectedImage(url);

    if (it == _selectedImages.end()) {
        // Push newly selected image to front
        _selectedImages.emplace_front(url, 1.0);

        // If wwt has not loaded the collection yet, wait with passing the message
        if (_isImageCollectionLoaded) {
            _wwtCommunicator.addImageLayerToWwt(url);
        }
    }
}

void ScreenSpaceSkyBrowser::update() {
    // Cap how messages are passed
    const std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    const std::chrono::system_clock::duration timeSinceLastUpdate = now - _lastUpdateTime;

    if (timeSinceLastUpdate > TimeUpdateInterval) {
        if (_equatorialAimIsDirty) {
            _wwtCommunicator.setAim(_equatorialAim, _verticalFov, _roll);
            _equatorialAimIsDirty = false;
        }
        if (_borderColorIsDirty) {
            _wwtCommunicator.setBorderColor(glm::ivec3(_borderColor.value() * 255.f));
            _borderColorIsDirty = false;
        }
        _lastUpdateTime = std::chrono::system_clock::now();
    }
    _wwtCommunicator.update();

    ScreenSpaceRenderable::update();

}

void ScreenSpaceSkyBrowser::bindTexture() {
    _wwtCommunicator.bindTexture();
}

glm::mat4 ScreenSpaceSkyBrowser::scaleMatrix() {
    // To ensure the plane has the right ratio
    // The _scale tells us how much of the windows height the browser covers: e.g. a
    // browser that covers 0.25 of the height of the window will have scale = 0.25

    glm::mat4 scale = glm::scale(
        glm::mat4(1.f),
        glm::vec3(_wwtCommunicator.browserRatio() * _scale, _scale, 1.f)
    );
    return scale;
}

float ScreenSpaceSkyBrowser::opacity() const noexcept {
    return _opacity;
}

// This Sky Browser is now controlled by a TargetBrowserPair
// Therefore we are disabling UI interaction with the properties from the UI
void ScreenSpaceSkyBrowser::setAsPaired() {
    // These properties should only be set from the UI, scripting or Target Browser Pair
    _verticalFov.setReadOnly(true);
    _borderColor.setReadOnly(true);
    _roll.setReadOnly(true);
    _equatorialAim.setReadOnly(true);
    _selectedImagesOpacities.setReadOnly(true);
    _selectedImagesUrls.setReadOnly(true);
}

glm::ivec3 ScreenSpaceSkyBrowser::borderColor() const {
    return glm::ivec3(_borderColor.value() * 255.f);
}

void ScreenSpaceSkyBrowser::removeSelectedImage(const std::string& imageUrl) {
    // Remove from selected list
    auto it = findSelectedImage(imageUrl);
    if (it != _selectedImages.end()) {
        _selectedImages.erase(it);
        _wwtCommunicator.removeSelectedImage(imageUrl);
    }
}

void ScreenSpaceSkyBrowser::hideChromeInterface() {
    _wwtCommunicator.hideChromeInterface();
}

void ScreenSpaceSkyBrowser::addImageLayerToWwt(std::string imageUrl) {
    _wwtCommunicator.addImageLayerToWwt(imageUrl);
}

void ScreenSpaceSkyBrowser::reload() {
    _wwtCommunicator.reload();
}

void ScreenSpaceSkyBrowser::setRatio(float ratio) {
    _wwtCommunicator.setRatio(ratio);
}

std::vector<double> ScreenSpaceSkyBrowser::opacities() const {
    std::vector<double> opacities;
    opacities.resize(_selectedImages.size());
    std::transform(
        _selectedImages.cbegin(),
        _selectedImages.cend(),
        opacities.begin(),
        [](const std::pair<std::string, double>& image) { return image.second; }
    );
    return opacities;
}

void ScreenSpaceSkyBrowser::setTargetRoll(double roll) {
    _roll = roll;
}

void ScreenSpaceSkyBrowser::loadImageCollection(const std::string& collection) {
    if (!_isImageCollectionLoaded) {
        _wwtCommunicator.loadImageCollection(collection);
    }
}

SelectedImageDeque::iterator ScreenSpaceSkyBrowser::findSelectedImage(
    const std::string& imageUrl)
{
    auto it = std::find_if(
        _selectedImages.begin(),
        _selectedImages.end(),
        [imageUrl](const std::pair<std::string, double>& pair) {
            return pair.first == imageUrl;
        }
    );
    return it;
}

bool ScreenSpaceSkyBrowser::isImageCollectionLoaded() const {
    return _isImageCollectionLoaded;
}

void ScreenSpaceSkyBrowser::setImageOpacity(const std::string& imageUrl, float opacity) {
    auto it = findSelectedImage(imageUrl);
    it->second = opacity;
    _wwtCommunicator.setImageOpacity(imageUrl, opacity);
}

void ScreenSpaceSkyBrowser::setImageCollectionIsLoaded(bool isLoaded) {
    _isImageCollectionLoaded = isLoaded;
}

void ScreenSpaceSkyBrowser::setImageOrder(const std::string& imageUrl, int order) {
    // Find in selected images list
    auto current = findSelectedImage(imageUrl);
    const int currentIndex = static_cast<int>(
        std::distance(_selectedImages.begin(), current)
        );

    std::deque<std::pair<std::string, double>> newDeque;

    for (int i = 0; i < static_cast<int>(_selectedImages.size()); i++) {
        if (i == currentIndex) {
            continue;
        }
        else if (i == order) {
            if (order < currentIndex) {
                newDeque.push_back(*current);
                newDeque.push_back(_selectedImages[i]);
            }
            else {
                newDeque.push_back(_selectedImages[i]);
                newDeque.push_back(*current);
            }
        }
        else {
            newDeque.push_back(_selectedImages[i]);
        }
    }

    _selectedImages = newDeque;
    const int reverseOrder = static_cast<int>(_selectedImages.size()) - order - 1;
    _wwtCommunicator.setImageOrder(imageUrl, reverseOrder);
}

} // namespace openspace
