#include "libplatform/impl.h"

namespace mp4v2
{
    namespace platform
    {
        namespace time
        {

            ///////////////////////////////////////////////////////////////////////////////

            seconds_t getLocalTimeSeconds()
            {
                return getLocalTimeMilliseconds() / 1000;
            }

            ///////////////////////////////////////////////////////////////////////////////

        } // namespace time
    } // namespace platform
} // namespace mp4v2
