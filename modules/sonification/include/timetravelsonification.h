#ifndef __OPENSPACE_MODULE_SONIFICATION___TIMETRAVELSONIFICATION___H__
#define __OPENSPACE_MODULE_SONIFICATION___TIMETRAVELSONIFICATION___H__

#include <modules/sonification/include/sonificationbase.h>
namespace openspace {

    class TimeTravelSonification : public SonificationBase {
    public:
        TimeTravelSonification(const std::string& ip, int port);
        virtual ~TimeTravelSonification() override = default;

        /**
         * Update function for the camera speed. Checks the frame time and updates
         * the current camera speed. Then sends it via the osc connection.
         *
         * \param camera pointer to the camera in the scene (not used in this sonification)
         */
        virtual void update(const Camera*) override;

        /**
         * Function to stop the sonification
         */
        virtual void stop() override;

    private:
        std::string _prevFocus;
    };

} // namespace openspace

#endif __OPENSPACE_MODULE_SONIFICATION___TIMETRAVELSONIFICATION___H__
