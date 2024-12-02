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

#include <modules/telemetry/include/specific/planetscomparesonification.h>

#include <openspace/engine/globals.h>
#include <openspace/scene/scenegraphnode.h>
#include <openspace/scripting/scriptengine.h>
#include <openspace/util/memorymanager.h>

namespace {
    constexpr std::string_view _loggerCat = "PlanetsCompareSonification";

    // Property info
    static const openspace::properties::PropertyOwner::PropertyOwnerInfo
        PlanetsCompareSonificationInfo =
    {
        "PlanetsCompareSonification",
        "Planets Compare Sonification",
        "Sonification that compares two different planets to each other in a variety of "
        "aspects"
    };

    constexpr openspace::properties::Property::PropertyInfo SelectedUpscaleInfo = {
        "SelectedUpscale",
        "Selected Planet Upscale Multiplier",
        "When a planet is selected to be compared in any of the drop down menus below, "
        "it is also upscaled as a visual indicator of which planets are currently being "
        "compared. This property determines how much the planet is scaled up as a "
        "multiplier of the original size."
    };

    constexpr openspace::properties::Property::PropertyInfo
        SelectedScaleInterpolationTimeInfo =
    {
        "SelectedScaleInterpolationTimeInfo",
        "Selected Planet Scale Interpolation Time",
        "When a planet is selected to be compared in any of the drop down menus below, "
        "it is also upscaled as a visual indicator of which planets are currently being "
        "compared. This property determines over how many seconds the scaling animation "
        "should play."
    };

    constexpr openspace::properties::Property::PropertyInfo FirstOptionInfo = {
        "FirstOption",
        "Choose a planet to compare",
        "Choose a planet in the given list to compare"
    };

    constexpr openspace::properties::Property::PropertyInfo SecondOptionInfo = {
        "SecondOption",
        "Choose a planet to compare",
        "Choose another planet in the given list to compare"
    };

    constexpr openspace::properties::Property::PropertyInfo ToggleAllInfo = {
        "ToggleAll",
        "All",
        "Toggle all comparing sonification varieties for both selected planets"
    };

    constexpr openspace::properties::Property::PropertyInfo SizeDayInfo = {
        "SizeDay",
        "Size/Day",
        "Toggle size/day sonification for both selected planets"
    };

    constexpr openspace::properties::Property::PropertyInfo GravityInfo = {
        "Gravity",
        "Gravity",
        "Toggle gravity sonification for both selected planets"
    };

    constexpr openspace::properties::Property::PropertyInfo TemperatureInfo = {
        "Temperature",
        "Temperature",
        "Toggle temperature sonification for both selected planets"
    };

    constexpr openspace::properties::Property::PropertyInfo AtmosphereInfo = {
        "Atmosphere",
        "Atmosphere",
        "Toggle atmosphere sonification for both selected planets"
    };

    constexpr openspace::properties::Property::PropertyInfo MoonsInfo = {
        "Moons",
        "Moons",
        "Toggle moons sonification for both selected planets"
    };

    constexpr openspace::properties::Property::PropertyInfo RingsInfo = {
        "Rings",
        "Rings",
        "Toggle rings sonification for both selected planets"
    };
} // namespace

namespace openspace {

PlanetsCompareSonification::PlanetsCompareSonification(const std::string& ip, int port)
    : TelemetryBase(PlanetsCompareSonificationInfo, ip, port)
    , _selectedUpscale(SelectedUpscaleInfo, 2000.0, 0.0, 1e+20)
    , _selectedScaleInterpolationTime(SelectedScaleInterpolationTimeInfo, 1.0, 0.0, 60)
    , _firstPlanet(FirstOptionInfo, properties::OptionProperty::DisplayType::Dropdown)
    , _secondPlanet(SecondOptionInfo, properties::OptionProperty::DisplayType::Dropdown)
    , _toggleAll(ToggleAllInfo, false)
    , _sizeDayEnabled(SizeDayInfo, false)
    , _gravityEnabled(GravityInfo, false)
    , _temperatureEnabled(TemperatureInfo, false)
    , _atmosphereEnabled(AtmosphereInfo, false)
    , _moonsEnabled(MoonsInfo, false)
    , _ringsEnabled(RingsInfo, false)
{
    // Scaling animation properties
    _selectedUpscale.setExponent(15.f);
    _selectedUpscale.onChange([this]() { onUpscaleChanged(); });
    addProperty(_selectedUpscale);
    addProperty(_selectedScaleInterpolationTime);

    // Add all the planet selection options to the drop down menu properties
    for (int i = 0; i < PlanetsOptions.size(); ++i) {
        _firstPlanet.addOption(i, PlanetsOptions[i].data());
        _secondPlanet.addOption(i, PlanetsOptions[i].data());
    }
    _firstPlanet.onChange([this]() { onFirstChanged(); });
    addProperty(_firstPlanet);

    _secondPlanet.onChange([this]() { onSecondChanged(); });
    addProperty(_secondPlanet);

    // Add the sonifiation aspects properties
    _toggleAll.onChange([this]() { onToggleAllChanged(); });
    addProperty(_toggleAll);

    _sizeDayEnabled.onChange([this]() { sendSettings(); });
    addProperty(_sizeDayEnabled);

    _gravityEnabled.onChange([this]() { sendSettings(); });
    addProperty(_gravityEnabled);

    _temperatureEnabled.onChange([this]() { sendSettings(); });
    addProperty(_temperatureEnabled);

    _atmosphereEnabled.onChange([this]() { sendSettings(); });
    addProperty(_atmosphereEnabled);

    _moonsEnabled.onChange([this]() { sendSettings(); });
    addProperty(_moonsEnabled);

    _ringsEnabled.onChange([this]() { sendSettings(); });
    addProperty(_ringsEnabled);
}

PlanetsCompareSonification::~PlanetsCompareSonification() {
    stop();
}

osc::Blob PlanetsCompareSonification::createSettingsBlob() const {
    int8_t* settings = reinterpret_cast<int8_t*>(
        global::memoryManager->TemporaryMemory.allocate(NumSettings)
    );

    settings[SizeDayIndex] = _sizeDayEnabled;
    settings[GravityIndex] = _gravityEnabled;
    settings[TemperatureIndex] = _temperatureEnabled;
    settings[AtmosphereIndex] = _atmosphereEnabled;
    settings[MoonsIndex] = _moonsEnabled;
    settings[RingsIndex] = _ringsEnabled;

    return osc::Blob(settings, NumSettings);
}

void PlanetsCompareSonification::sendSettings() {
    if (!_enabled) {
        return;
    }

    std::string label = "/Compare";
    std::vector<OscDataType> data(NumDataItems);

    data[FirstPlanetIndex] = _firstPlanet;
    data[SecondPlanetIndex] = _secondPlanet;
    data[SettingsIndex] = createSettingsBlob();

    _connection->send(label, data);
}

void PlanetsCompareSonification::planetSelectionChanged(
                                                properties::OptionProperty& changedPlanet,
                                             properties::OptionProperty& notChangedPlanet,
                                                        std::string& prevChangedPlanet)
{
    if (changedPlanet != 0 && changedPlanet == notChangedPlanet) {
        LINFO("Cannot compare a planet to itself");
        changedPlanet.setValue(0);
        return;
    }

    if (!prevChangedPlanet.empty()) {
        // Reset the scale of the previously compared planet
        scalePlanet(prevChangedPlanet, 1.0, _selectedScaleInterpolationTime);
    }

    if (changedPlanet != 0) {
        scalePlanet(
            changedPlanet.getDescriptionByValue(changedPlanet.value()),
            _selectedUpscale,
            _selectedScaleInterpolationTime
        );

        prevChangedPlanet = changedPlanet.getDescriptionByValue(changedPlanet.value());
    }
    else {
        prevChangedPlanet = "";
    }

    sendSettings();
}

void PlanetsCompareSonification::scalePlanet(const std::string& planet, double scale,
                                             double interpolationTime)
{
    // Scale the selected planet to visually indicate which planets are currently compared
    std::string script = std::format(
        "openspace.setPropertyValueSingle('Scene.{}.Scale.Scale', {}, {});",
        planet, scale, interpolationTime
    );
    global::scriptEngine->queueScript(script);
}

void PlanetsCompareSonification::onUpscaleChanged() {
    // Update the scale value for the first planet if something is selected
    if (_firstPlanet != 0) {
        scalePlanet(
            _firstPlanet.getDescriptionByValue(_firstPlanet.value()),
            _selectedUpscale,
            0.0
        );
    }

    // Update the scale value for the second planet if something is selected
    if (_secondPlanet != 0) {
        scalePlanet(
            _secondPlanet.getDescriptionByValue(_secondPlanet.value()),
            _selectedUpscale,
            0.0
        );
    }
}

void PlanetsCompareSonification::onFirstChanged() {
    planetSelectionChanged(_firstPlanet, _secondPlanet, _oldFirst);
}

void PlanetsCompareSonification::onSecondChanged() {
    planetSelectionChanged(_secondPlanet, _firstPlanet, _oldSecond);
}

void PlanetsCompareSonification::onToggleAllChanged() {
    _sizeDayEnabled.setValue(_toggleAll);
    _gravityEnabled.setValue(_toggleAll);
    _temperatureEnabled.setValue(_toggleAll);
    _atmosphereEnabled.setValue(_toggleAll);
    _moonsEnabled.setValue(_toggleAll);
    _ringsEnabled.setValue(_toggleAll);
}

void PlanetsCompareSonification::update(const Camera*) {}

void PlanetsCompareSonification::stop() {
    _toggleAll = false;

    _firstPlanet = 0;
    _secondPlanet = 0;
}

} // namespace openspace
