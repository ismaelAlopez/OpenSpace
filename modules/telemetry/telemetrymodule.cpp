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

#include <modules/telemetry/telemetrymodule.h>

#include <modules/telemetry/include/general/anglemodetelemetry.h>
#include <modules/telemetry/include/general/cameratelemetry.h>
#include <modules/telemetry/include/general/focustelemetry.h>
#include <modules/telemetry/include/general/nodestelemetry.h>
#include <modules/telemetry/include/general/timetelemetry.h>
#include <modules/telemetry/include/specific/planetscomparesonification.h>
#include <modules/telemetry/include/specific/planetsoverviewsonification.h>
#include <modules/telemetry/include/specific/planetssonification.h>
#include <ghoul/logging/logmanager.h>
#include <openspace/camera/camera.h>
#include <openspace/documentation/documentation.h>
#include <openspace/engine/globals.h>
#include <openspace/engine/globalscallbacks.h>
#include <openspace/engine/windowdelegate.h>
#include <openspace/rendering/renderengine.h>
#include <openspace/scene/scene.h>

namespace {
    constexpr std::string_view _loggerCat = "TelemetryModule";

    // The default OSC reciver is SuperCollider with these default values.
    // However, the user can define any reciver in the openspace.cfg file as the
    // ModuleConfiguration for the Telemetry module.
    constexpr std::string_view DefaultSuperColliderIp = "127.0.0.1";
    constexpr int DefaultSuperColliderPort = 57120;

    constexpr openspace::properties::Property::PropertyInfo EnabledInfo = {
        "Enabled",
        "Enabled",
        "Enable or disable all gathering of telemetry information"
    };

    constexpr openspace::properties::Property::PropertyInfo IpAddressInfo = {
        "IpAddress",
        "Ip address",
        "The network ip address that the telemetry osc messages is sent to"
    };

    constexpr openspace::properties::Property::PropertyInfo PortInfo = {
        "Port",
        "Port",
        "The network port that the telemetry osc messages is sent to"
    };

    constexpr openspace::properties::Property::PropertyInfo AngleCalculationModeInfo = {
        "AngleCalculationMode",
        "Angle Calculation Mode",
        "This setting changes the method to calculate any angles in the telemetries. "
        "The Horizontal mode, generally works well for flat displays or forward facing "
        "immersive envierments. The Circular mode, generally works well for centered "
        "fisheye displays or omnidirectional immersive environments"
    };

    constexpr openspace::properties::Property::PropertyInfo IncludeElevationAngleInfo = {
        "IncludeElevationAngle",
        "Include Elevation Angle",
        "This setting determines if an additional elevation angle should be calculated "
        "for the telemetries that calculate angles. The method used for this calculation "
        "also depends on the angle calculation mode"
    };

    struct [[codegen::Dictionary(TelemetryModule)]] Parameters {
        // [[codegen::verbatim(IpAddressInfo.description)]]
        std::optional<std::string> ipAddress;

        // [[codegen::verbatim(PortInfo.description)]]
        std::optional<int> port;

        enum class [[codegen::map(openspace::TelemetryModule::AngleCalculationMode)]] AngleCalculationMode {
            Horizontal,
            Circular
        };

        // [[codegen::verbatim(AngleCalculationModeInfo.description)]]
        std::optional<AngleCalculationMode> angleCalculationMode;

        // [[codegen::verbatim(IncludeElevationAngleInfo.description)]]
        std::optional<bool> includeElevationAngle;
    };
#include "telemetrymodule_codegen.cpp"
} // namespace

namespace openspace {

TelemetryModule::TelemetryModule()
    : OpenSpaceModule("Telemetry")
    , _enabled(EnabledInfo, false)
    , _ipAddress(IpAddressInfo, DefaultSuperColliderIp.data())
    , _port(PortInfo, DefaultSuperColliderPort, 1025, 65536)
    , _modeOptions(
        AngleCalculationModeInfo,
        properties::OptionProperty::DisplayType::Dropdown
    )
    , _includeElevationAngle(IncludeElevationAngleInfo, false)
{
    addProperty(_enabled);

    _ipAddress.setReadOnly(true);
    addProperty(_ipAddress);

    _port.setReadOnly(true);
    addProperty(_port);

    // Add options to the drop down menu
    _modeOptions.addOptions({
        { 0, "Horizontal" },
        { 1, "Circular" }
    });
    _modeOptions.onChange([this]() { guiOnChangeAngleCalculationMode(); });

    // Select Horizontal angle calculation mode as the default
    _modeOptions.setValue(static_cast<int>(AngleCalculationMode::Horizontal));
    addProperty(_modeOptions);

    addProperty(_includeElevationAngle);
}

TelemetryModule::~TelemetryModule() {
    // Clear the telemetries list
    for (TelemetryBase* telemetry : _telemetries) {
        delete telemetry;
    }
}

void TelemetryModule::guiOnChangeAngleCalculationMode() {
    _angleCalculationMode = static_cast<AngleCalculationMode>(_modeOptions.value());
}

void TelemetryModule::internalInitialize(const ghoul::Dictionary& dictionary) {
    const Parameters p = codegen::bake<Parameters>(dictionary);

    _ipAddress = p.ipAddress.value_or(_ipAddress);
    _port = p.port.value_or(_port);

    if (p.angleCalculationMode.has_value()) {
        Parameters::AngleCalculationMode mode =
            Parameters::AngleCalculationMode(*p.angleCalculationMode);
        _angleCalculationMode = codegen::map<AngleCalculationMode>(mode);
    }

    // Fill telemetry list
    TelemetryBase* telemetry = new AngleModeTelemetry(_ipAddress, _port);
    addTelemetry(telemetry);

    telemetry = new CameraTelemetry(_ipAddress, _port);
    addTelemetry(telemetry);

    telemetry = new FocusTelemetry(_ipAddress, _port);
    addTelemetry(telemetry);

    telemetry = new TimeTelemetry(_ipAddress, _port);
    addTelemetry(telemetry);

    telemetry = new NodesTelemetry(_ipAddress, _port);
    addTelemetry(telemetry);

    telemetry = new PlanetsCompareSonification(_ipAddress, _port);
    addTelemetry(telemetry);

    telemetry = new PlanetsOverviewSonification(_ipAddress, _port);
    addTelemetry(telemetry);

    telemetry = new PlanetsSonification(_ipAddress, _port);
    addTelemetry(telemetry);

    // Only the master runs the TelemetryModule update thread
    if (global::windowDelegate->isMaster()) {
        _isRunning = true;
        _updateThread = std::thread([this]() { update(std::ref(_isRunning)); });

        // Make sure the telemetry thread is synced with the main thread
        global::callback::postSyncPreDraw->emplace_back([this]() {
            // Tell the telemetry thread that a new frame is starting
            //LDEBUG("The main thread signals to the telemetry thread");
            syncToMain.notify_one();
        });
    }
}

void TelemetryModule::addTelemetry(TelemetryBase* telemetry) {
    _telemetries.push_back(telemetry);
    addPropertySubOwner(telemetry);
}

void TelemetryModule::internalDeinitialize() {
    // Stop the loop before joining and tell the thread it is ok to run last itteration
    _isRunning = false;
    syncToMain.notify_one();

    // Join the thread
    _updateThread.join();
}

const std::vector<TelemetryBase*>& TelemetryModule::telemetries() const {
    return _telemetries;
}

const TelemetryBase* TelemetryModule::telemetry(std::string id) const {
    for (const TelemetryBase* t : _telemetries) {
        if (t->identifier() == id) {
            return t;
        }
    }
    return nullptr;
}

TelemetryBase* TelemetryModule::telemetry(std::string id) {
    for (TelemetryBase* t : _telemetries) {
        if (t->identifier() == id) {
            return t;
        }
    }
    return nullptr;
}

TelemetryModule::AngleCalculationMode TelemetryModule::angleCalculationMode() const {
    return _angleCalculationMode;
}

bool TelemetryModule::includeElevationAngle() const {
    return _includeElevationAngle;
}

void TelemetryModule::update(std::atomic<bool>& isRunning) {
    Scene* scene = nullptr;
    Camera* camera = nullptr;
    bool isInitialized = false;

    while (isRunning) {
        // Wait for the main thread
        //LDEBUG("The telemetry thread is waiting for a signal from the main thread");
        std::unique_lock<std::mutex> lk(mutexLock);
        syncToMain.wait(lk);
        //LDEBUG(
        //    "The telemetry thread is working after having received a signal from "
        //    "the main thread"
        //);

        // Check if the module is even enabled
        if (!_enabled) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }

        // Initialize the scena and camera information if that has not already been done
        if (!isInitialized) {
            // Find the scene
            if (!scene) {
                scene = global::renderEngine->scene();
            }

            // Find the camera in the scene
            if (!camera) {
                camera = scene ? scene->camera() : nullptr;
            }

            // Check status
            if (!scene || scene->isInitializing() || scene->root()->children().empty() ||
                !camera ||glm::length(camera->positionVec3()) <
                std::numeric_limits<glm::f64>::epsilon())
            {
                isInitialized = false;
            }
            else {
                isInitialized = true;
            }
        }

        // Process the telemetries
        if (isInitialized) {
            for (TelemetryBase* telemetry : _telemetries) {
                if (!telemetry) {
                    continue;
                }
                telemetry->update(camera);
            }
        }
    }
}

std::vector<scripting::LuaLibrary> TelemetryModule::luaLibraries() const {
    return {
        NodesTelemetry::luaLibrary(),
        PlanetsSonification::luaLibrary()
    };
}

} // namespace openspace
