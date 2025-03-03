#include "libplatform/impl.h"
#include <stdlib.h>

namespace mp4v2
{
    namespace platform
    {
        namespace number
        {

            ///////////////////////////////////////////////////////////////////////////////

            uint32_t random32() { return uint32_t(::random()); }

            ///////////////////////////////////////////////////////////////////////////////

            void srandom(uint32_t seed) { ::srandom(seed); }

            ///////////////////////////////////////////////////////////////////////////////

        } // namespace number
    } // namespace platform
} // namespace mp4v2
