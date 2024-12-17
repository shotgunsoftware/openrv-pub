#include "libplatform/impl.h"

namespace mp4v2
{
    namespace platform
    {
        namespace sys
        {

            ///////////////////////////////////////////////////////////////////////////////

            const char* getErrorStr(int errno_)
            {
                return std::strerror(errno_);
            }

            ///////////////////////////////////////////////////////////////////////////////

            int getLastError() { return errno; }

            ///////////////////////////////////////////////////////////////////////////////

            const char* getLastErrorStr() { return strerror(errno); }

            ///////////////////////////////////////////////////////////////////////////////

        } // namespace sys
    } // namespace platform
} // namespace mp4v2
