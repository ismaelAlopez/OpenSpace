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

#ifndef __OPENSPACE_MODULE_TELEMETRY___FOCUSTELEMETRY___H__
#define __OPENSPACE_MODULE_TELEMETRY___FOCUSTELEMETRY___H__

#include <modules/telemetry/include/telemetrybase.h>

#include <openspace/properties/optionproperty.h>

namespace openspace {

class FocusTelemetry : public TelemetryBase {
public:
    FocusTelemetry(const std::string& ip, int port);
    virtual ~FocusTelemetry() override = default;

    /**
     * Main update function to gather focus telemetry information (current focus node) and
     * send it via the osc connection.
     *
     * \param camera The camera in the scene (not used in this case)
     */
    virtual void update(const Camera*) override;

    /**
     * Function to stop the gathering of focus telemetry data
     */
    virtual void stop() override;

private:
    // Indices for data items
    static constexpr int NumDataItems = 1;
    static constexpr int FocusNodeIndex = 0;

    /**
     * Gather focus telemetry information (current focus node)
     *
     * \param camera The camera in the scene
     *
     * \return True if the data is new compared to before, otherwise false
     */
    bool getData();

    /**
     * Send the current focus telemetry information over the osc connection
     * Order of data: Current focus node
     */
    void sendData();

    // Variables
    std::string _currentFocus;
};

} // namespace openspace

#endif __OPENSPACE_MODULE_TELEMETRY___FOCUSTELEMETRY___H__
