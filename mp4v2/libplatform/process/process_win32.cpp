#include "libplatform/impl.h"
#include <process.h>

namespace mp4v2
{
    namespace platform
    {
        namespace process
        {

            ///////////////////////////////////////////////////////////////////////////////

            int32_t getpid() { return ::_getpid(); }

            ///////////////////////////////////////////////////////////////////////////////

        } // namespace process
    } // namespace platform
} // namespace mp4v2
