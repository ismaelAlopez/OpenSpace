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

#include <modules/softwareintegration/softwareintegrationmodule.h>

#include <modules/softwareintegration/rendering/renderablepointscloud.h>
#include <openspace/documentation/documentation.h>
#include <openspace/engine/globals.h>
#include <openspace/engine/globalscallbacks.h>
#include <openspace/interaction/navigationhandler.h>
#include <openspace/rendering/renderengine.h>
#include <openspace/scene/scene.h>
#include <openspace/scripting/scriptengine.h>
#include <openspace/util/factorymanager.h>
#include <openspace/query/query.h>
#include <ghoul/logging/logmanager.h>
#include <ghoul/misc/dictionaryluaformatter.h>

#include <iomanip>

using namespace std::string_literals;

namespace {
    constexpr const char* _loggerCat = "SoftwareIntegrationModule";
} // namespace

namespace openspace {

SoftwareIntegrationModule::SoftwareIntegrationModule() : OpenSpaceModule(Name) {}

void SoftwareIntegrationModule::internalInitialize(const ghoul::Dictionary&) {
    auto fRenderable = FactoryManager::ref().factory<Renderable>();
    ghoul_assert(fRenderable, "No renderable factory existed");

    fRenderable->registerClass<RenderablePointsCloud>("RenderablePointsCloud");

    // Open port
    start(4700);

    global::callback::preSync->emplace_back([this]() { preSyncUpdate(); });
}

void SoftwareIntegrationModule::internalDeinitialize() {
    stop();
}

void SoftwareIntegrationModule::preSyncUpdate() {
    if (_onceNodeExistsCallbacks.empty()) {
        return;
    }

    // Check if the scene graph node has been created. If so, call the corresponding
    // callback function to set up any subscriptions
    auto it = _onceNodeExistsCallbacks.begin();
    while (it != _onceNodeExistsCallbacks.end()) {
        const std::string& identifier = it->first;
        const std::function<void()>& callback = it->second;
        const SceneGraphNode* sgn =
            global::renderEngine->scene()->sceneGraphNode(identifier);

        if (sgn) {
            callback();
            it = _onceNodeExistsCallbacks.erase(it);
            continue;
        }
        it++;
    }
}

void SoftwareIntegrationModule::start(int port) {
    _socketServer.listen(port);

    _serverThread = std::thread([this]() { handleNewPeers(); });
    _eventLoopThread = std::thread([this]() { eventLoop(); });
}

void SoftwareIntegrationModule::stop() {
    _shouldStop = true;
    _socketServer.close();

    if (_serverThread.joinable()) {
        _serverThread.join();
    }
    if (_eventLoopThread.joinable()) {
        _eventLoopThread.join();
    }
}

bool SoftwareIntegrationModule::isConnected(const Peer& peer) const {
    return peer.status != SoftwareConnection::Status::Connecting &&
        peer.status != SoftwareConnection::Status::Disconnected;
}

std::shared_ptr<SoftwareIntegrationModule::Peer> SoftwareIntegrationModule::peer(size_t id) {
    std::lock_guard<std::mutex> lock(_peerListMutex);
    auto it = _peers.find(id);
    if (it == _peers.end()) {
        return nullptr;
    }
    return it->second;
}

void SoftwareIntegrationModule::disconnect(Peer& peer) {
    if (isConnected(peer)) {
        _nConnections -= 1;
    }

    peer.connection.disconnect();
    peer.thread.join();
    _peers.erase(peer.id);
}

void SoftwareIntegrationModule::eventLoop() {
    while (!_shouldStop) {
        if (!_incomingMessages.empty()) {
            PeerMessage pm = _incomingMessages.pop();
            handlePeerMessage(std::move(pm));
        }
    }
}

void SoftwareIntegrationModule::handleNewPeers() {
    while (!_shouldStop) {
        std::unique_ptr<ghoul::io::TcpSocket> socket =
            _socketServer.awaitPendingTcpSocket();

        if (!socket) {
            return;
        }

        socket->startStreams();

        const size_t id = _nextConnectionId++;
        std::shared_ptr<Peer> p = std::make_shared<Peer>(Peer{
            id,
            "",
            std::thread(),
            SoftwareConnection(std::move(socket)),
            SoftwareConnection::Status::Connecting
        });
        auto it = _peers.emplace(p->id, p);
        it.first->second->thread = std::thread([this, id]() {
            handlePeer(id);
        });
    }
}

void SoftwareIntegrationModule::handlePeer(size_t id) {
    while (!_shouldStop) {
        std::shared_ptr<Peer> p = peer(id);
        if (!p) {
            return;
        }

        if (!p->connection.isConnectedOrConnecting()) {
            return;
        }
        try {
            SoftwareConnection::Message m = p->connection.receiveMessage();
            _incomingMessages.push({ id, m });
        }
        catch (const SoftwareConnection::SoftwareConnectionLostError&) {
            LERROR(fmt::format("Connection lost to {}", p->id));
            _incomingMessages.push({
                id,
                SoftwareConnection::Message(
                    SoftwareConnection::MessageType::Disconnection, std::vector<char>()
                )
            });
            return;
        }
    }
}

void SoftwareIntegrationModule::handlePeerMessage(PeerMessage peerMessage) {
    const size_t peerId = peerMessage.peerId;
    const std::shared_ptr<Peer>& peerPtr = peer(peerId);

    const SoftwareConnection::MessageType messageType = peerMessage.message.type;
    std::vector<char>& message = peerMessage.message.content;

    _messageOffset = 0; // Resets message offset

    switch (messageType) {
    case SoftwareConnection::MessageType::Connection: {
        const std::string software(message.begin(), message.end());
        LINFO(fmt::format("OpenSpace has connected with {} through socket.", software));
        break;
    }
    case SoftwareConnection::MessageType::ReadPointData: {
        const std::string sgnMessage(message.begin(), message.end());
        LDEBUG(fmt::format("Message recieved.. Point Data: {}", sgnMessage));

        // The following order of creating variables is the exact order they're received
        // in the message. If the order is not the same, the global variable
        // 'message offset' will be wrong
        const std::string identifier = readString(message);
        const glm::vec3 color = readColor(message);
        const float opacity = readFloatValue(message);
        const float size = readFloatValue(message);
        const std::string guiName = readString(message);

        // 9 first bytes is the length of the data
        const int lengthOffset = _messageOffset + 9;
        std::string length;
        for (int i = _messageOffset; i < lengthOffset; i++) {
            length.push_back(message[i]);
            _messageOffset++;
        }

        const int nPoints = stoi(length);

        const std::vector<float> xCoordinates = readFloatData(message, nPoints);
        const std::vector<float> yCoordinates = readFloatData(message, nPoints);
        const std::vector<float> zCoordinates = readFloatData(message, nPoints);

        // Do some simple checking to make sure the data was loaded correctly
        // @TODO make this check more clever to avoid trying to read all data
        // if something goes wrong
        bool equalSize = (xCoordinates.size() == yCoordinates.size()) &&
            (xCoordinates.size() == zCoordinates.size());

        if (!equalSize || (nPoints != xCoordinates.size())) {
            LERROR("Something went wrong when loading the data!");
            return;
        }

        // TODO: if huge number of points, save to a file instead
        ghoul::Dictionary pointDataDictonary;
        for (int i = 0; i < xCoordinates.size(); i++) {
            float x = xCoordinates[i];
            float y = yCoordinates[i];
            float z = zCoordinates[i];

            const std::string key = fmt::format("[{}]", i + 1);
            pointDataDictonary.setValue<glm::dvec3>(key, { x, y, z });
        }

        // Create a renderable
        ghoul::Dictionary renderable;
        renderable.setValue("Type", "RenderablePointsCloud"s);
        renderable.setValue("Color", static_cast<glm::dvec3>(color));
        renderable.setValue("Opacity", static_cast<double>(opacity));
        renderable.setValue("Size", static_cast<double>(size));
        renderable.setValue("Data", pointDataDictonary);

        ghoul::Dictionary gui;
        gui.setValue("Name", guiName);
        gui.setValue("Path", "/Software Integration"s);

        ghoul::Dictionary node;
        node.setValue("Identifier", identifier);
        node.setValue("Renderable", renderable);
        node.setValue("GUI", gui);

        openspace::global::scriptEngine->queueScript(
            "openspace.addSceneGraphNode(" + ghoul::formatLua(node) + ")",
            scripting::ScriptEngine::RemoteScripting::Yes
        );

        openspace::global::scriptEngine->queueScript(
            "openspace.setPropertyValueSingle('NavigationHandler.OrbitalNavigator.RetargetAnchor', nil)"
            "openspace.setPropertyValueSingle('NavigationHandler.OrbitalNavigator.Anchor', '" + identifier + "')"
            "openspace.setPropertyValueSingle('NavigationHandler.OrbitalNavigator.Aim', '')",
            scripting::ScriptEngine::RemoteScripting::Yes
        );

        // We have to wait until the renderable exists before we can subscribe to
        // changes in its properties
        auto callback = [this, identifier, peerId]() {
            subscribeToRenderableUpdates(identifier, peerId);
        };
        _onceNodeExistsCallbacks.emplace(identifier, callback);

        // Create renderable
        break;
    }
    case SoftwareConnection::MessageType::RemoveSceneGraphNode: {
        const std::string identifier(message.begin(), message.end());
        LDEBUG(fmt::format("Message recieved.. Delete SGN: {}", identifier));

        const std::string currentAnchor =
            global::navigationHandler->orbitalNavigator().anchorNode()->identifier();

        if (currentAnchor == identifier) {
            // If the deleted node is the current anchor, first change focus to the Sun
            openspace::global::scriptEngine->queueScript(
                "openspace.setPropertyValueSingle('NavigationHandler.OrbitalNavigator.Anchor', 'Sun')"
                "openspace.setPropertyValueSingle('NavigationHandler.OrbitalNavigator.Aim', '')",
                scripting::ScriptEngine::RemoteScripting::Yes
            );
        }
        openspace::global::scriptEngine->queueScript(
            "openspace.removeSceneGraphNode('" + identifier + "');",
            scripting::ScriptEngine::RemoteScripting::Yes
        );
        LDEBUG(fmt::format("Scene graph node '{}' removed.", identifier));
        break;
    }
    case SoftwareConnection::MessageType::Color: {
        const std::string colorMessage(message.begin(), message.end());
        LDEBUG(fmt::format("Message recieved.. New Color: {}", colorMessage));
        const std::string identifier = readString(message);
        const glm::vec3 color = readColor(message);

        // Get color of renderable
        const Renderable* myRenderable = renderable(identifier);
        properties::Property* colorProperty = myRenderable->property("Color");
        auto propertyAny = colorProperty->get();
        glm::vec3 propertyColor = std::any_cast<glm::vec3>(propertyAny);
        bool isUpdated = (propertyColor != color);

        // Update color of renderable
        if (isUpdated) {
            colorProperty->set(color);
        }
        break;
    }
    case SoftwareConnection::MessageType::Opacity: {
        const std::string opacityMessage(message.begin(), message.end());
        LDEBUG(fmt::format("Message recieved.. New Opacity: {}", opacityMessage));
        const std::string identifier = readString(message);
        const float opacity = readFloatValue(message);

        // Get opacity of renderable
        const Renderable* myRenderable = renderable(identifier);
        properties::Property* opacityProperty = myRenderable->property("Opacity");
        auto propertyAny = opacityProperty->get();
        float propertyOpacity = std::any_cast<float>(propertyAny);
        bool isUpdated = (propertyOpacity != opacity);

        // Update opacity of renderable
        if (isUpdated) {
            opacityProperty->set(opacity);
        }
        break;
    }
    case SoftwareConnection::MessageType::Size: {
        const std::string sizeMessage(message.begin(), message.end());
        LDEBUG(fmt::format("Message recieved.. New Size: {}", sizeMessage));
        const std::string identifier = readString(message);
        float size = readFloatValue(message);

        // Get size of renderable
        const Renderable* myRenderable = renderable(identifier);
        properties::Property* sizeProperty = myRenderable->property("Size");
        auto propertyAny = sizeProperty->get();
        float propertySize = std::any_cast<float>(propertyAny);
        bool isUpdated = (propertySize != size);

        // Update size of renderable
        if (isUpdated) {
            sizeProperty->set(size);
        }
        break;
    }
    case SoftwareConnection::MessageType::Visibility: {
        const std::string visibilityMessage(message.begin(), message.end());
        LDEBUG(fmt::format("Message recieved.. New Visibility: {}", visibilityMessage));
        const std::string identifier = readString(message);
        std::string visibility;
        visibility.push_back(message[_messageOffset]);
        bool boolValue = (visibility == "F") ? false : true;

        // Toggle visibility of renderable
        const Renderable* myRenderable = renderable(identifier);
        properties::Property* visibilityProperty =
            myRenderable->property("ToggleVisibility");

        visibilityProperty->set(boolValue);
        break;
    }
    case SoftwareConnection::MessageType::Disconnection: {
        disconnect(*peerPtr);
        break;
    }
    default:
        LERROR(fmt::format(
            "Unsupported message type: {}", static_cast<int>(messageType)
        ));
        break;
    }
}

std::string formatUpdateMessage(const std::string& messageType,
                                const std::string& identifier,
                                const std::string& value)
{
    const int lengthOfIdentifier = identifier.length();
    const int lengthOfValue = value.length();
    std::string subject = std::to_string(lengthOfIdentifier);
    subject += identifier;
    subject += std::to_string(lengthOfValue);
    subject += value;

    // Format length of subject to always be 4 digits
    std::ostringstream os;
    os << std::setfill('0') << std::setw(4) << subject.length();
    const std::string lengthOfSubject = os.str();

    return messageType + lengthOfSubject + subject;
}

void SoftwareIntegrationModule::subscribeToRenderableUpdates(std::string identifier,
                                                             size_t peerId)
{
    const Renderable* aRenderable = renderable(identifier);
    if (!aRenderable) {
        LERROR(fmt::format("Renderable with identifier '{}' doesn't exist", identifier));
        return;
    }

    std::shared_ptr<Peer> peer = this->peer(peerId);
    if (!peer) {
        LERROR(fmt::format("Peer connection with id '{}' could not be found", peerId));
        return;
    }

    properties::Property* colorProperty = aRenderable->property("Color");
    properties::Property* opacityProperty = aRenderable->property("Opacity");
    properties::Property* sizeProperty = aRenderable->property("Size");
    properties::Property* visibilityProperty = aRenderable->property("ToggleVisibility");

    // Update color of renderable
    auto updateColor = [colorProperty, identifier, peer]() {
        const std::string value = colorProperty->getStringValue();
        const std::string message = formatUpdateMessage("UPCO", identifier, value);
        peer->connection.sendMessage(message);
    };
    if (colorProperty) {
        colorProperty->onChange(updateColor);
    }

    // Update opacity of renderable
    auto updateOpacity = [opacityProperty, identifier, peer]() {
        const std::string value = opacityProperty->getStringValue();
        const std::string message = formatUpdateMessage("UPOP", identifier, value);
        peer->connection.sendMessage(message);
    };
    if (opacityProperty) {
        opacityProperty->onChange(updateOpacity);
    }

    // Update size of renderable
    auto updateSize = [sizeProperty, identifier, peer]() {
        const std::string value = sizeProperty->getStringValue();
        const std::string message = formatUpdateMessage("UPSI", identifier, value);
        peer->connection.sendMessage(message);
    };
    if (sizeProperty) {
        sizeProperty->onChange(updateSize);
    }

    // Toggle visibility of renderable
    auto toggleVisibility = [visibilityProperty, identifier, peer]() {
        const std::string lengthOfIdentifier = std::to_string(identifier.length());
        const std::string messageType = "TOVI";

        bool isVisible = visibilityProperty->getStringValue() == "true";

        const std::string visibilityFlag = isVisible ? "T" : "F";
        const std::string subject = lengthOfIdentifier + identifier + visibilityFlag;
        // We don't need a lengthOfValue here because it will always be 1 character

       // @TODO (emmbr 2021-02-02) make sure this message has the same format as the
       // others, so the 'formatUpdateMessage(..)' function can be used here

        // Format length of subject to always be 4 digits
        std::ostringstream os;
        os << std::setfill('0') << std::setw(4) << subject.length();
        const std::string lengthOfSubject = os.str();

        const std::string message = messageType + lengthOfSubject + subject;
        peer->connection.sendMessage(message);
    };
    if (visibilityProperty) {
        visibilityProperty->onChange(toggleVisibility);
    }
}

float SoftwareIntegrationModule::readFloatValue(std::vector<char>& message) {
    std::string length;
    length.push_back(message[_messageOffset]);
    _messageOffset++;

    int lengthOfValue = stoi(length);
    std::string value;
    int counter = 0;
    while (counter != lengthOfValue) {
        value.push_back(message[_messageOffset]);
        _messageOffset++;
        counter++;
    }
    return std::stof(value);
}

glm::vec3 SoftwareIntegrationModule::readColor(std::vector<char>& message) {
    std::string lengthOfColor; // Not used for now, but sent in message
    lengthOfColor.push_back(message[_messageOffset]);
    _messageOffset++;
    lengthOfColor.push_back(message[_messageOffset]);
    _messageOffset++;

    // Color is recieved in a string-format of (redValue, greenValue, blueValue)
    // Therefore, we have to iterate through the message and ignore characters
    // "( , )" and separate the values in the string
    std::string red;
    while (message[_messageOffset] != ',') {
        if (message[_messageOffset] == '(') {
            _messageOffset++;
        }
        else {
            red.push_back(message[_messageOffset]);
            _messageOffset++;
        }
    }
    _messageOffset++;

    std::string green;
    while (message[_messageOffset] != ',') {
        green.push_back(message[_messageOffset]);
        _messageOffset++;
    }
    _messageOffset++;

    std::string blue;
    while (message[_messageOffset] != ')') {
        blue.push_back(message[_messageOffset]);
        _messageOffset++;
    }
    _messageOffset++;

    // Convert red, green, blue strings to floats
    float r = std::stof(red);
    float g = std::stof(green);
    float b = std::stof(blue);

    return glm::vec3(r, g, b);
}

std::string SoftwareIntegrationModule::readString(std::vector<char>& message) {
    std::string length;
    length.push_back(message[_messageOffset]);
    _messageOffset++;
    length.push_back(message[_messageOffset]);
    _messageOffset++;

    int lengthOfValue = stoi(length);
    std::string value;
    int counter = 0;
    while (counter != lengthOfValue) {
        value.push_back(message[_messageOffset]);
        _messageOffset++;
        counter++;
    }

    return value;
}

std::vector<float> SoftwareIntegrationModule::readFloatData(std::vector<char>& message,
                                                            int nValues)
{
    std::vector<float> data;

    for (int counter = 0; counter < nValues; ++counter) {
        std::string value;
        while (message[_messageOffset] != ',') {
            value.push_back(message[_messageOffset]);
            _messageOffset++;
        }

        try {
            float dataValue = stof(value);
            data.push_back(dataValue);
        }
        catch (const std::invalid_argument& ia) {
            LERROR(fmt::format(
                "Error reading value {}. Invalid argument: {} ",
                counter + 1,
                ia.what()
            ));
            return std::vector<float>();
        }

        _messageOffset++;
    }

    return data;
}

size_t SoftwareIntegrationModule::nConnections() const {
    return _nConnections;
}

std::vector<documentation::Documentation>
SoftwareIntegrationModule::documentations() const
{
    return {
        RenderablePointsCloud::Documentation(),
    };
}

} // namespace openspace

