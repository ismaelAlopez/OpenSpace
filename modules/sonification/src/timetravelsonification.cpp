#include <modules/sonification/include/timetravelsonification.h>

#include <openspace/engine/globals.h>
#include <openspace/engine/windowdelegate.h>

namespace {
    static const openspace::properties::PropertyOwner::PropertyOwnerInfo
        TimeTravelSonificationInfo =
    {
       "TimeTravelSonification",
       "Time Travel Sonification",
       "Sonification that keeps track of the cameras current speed"
    };
} // namespace

namespace openspace {
    TimeTravelSonification::TimeTravelSonification(const std::string& ip, int port)
        : SonificationBase(TimeTravelSonificationInfo, ip, port)
    {}

    void TimeTravelSonification::update(const Camera*) {
        if (!_enabled) {
            return;
        }

        float speed = global::windowDelegate->averageDeltaTime();
        std::string label = "/TimeTravel";
        std::vector<OscDataType> data(1);
        data[0] = speed;
        _connection->send(label, data);
    }

    void TimeTravelSonification::stop() {}

} // namespace openspace
