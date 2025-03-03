#include "libplatform/impl.h"
#include <stdlib.h>

namespace mp4v2
{
    namespace platform
    {
        namespace number
        {

            ///////////////////////////////////////////////////////////////////////////////

            uint32_t random32() { return uint32_t(::rand() << 16 | ::rand()); }

            ///////////////////////////////////////////////////////////////////////////////

            void srandom(uint32_t seed) { ::srand(seed); }

            ///////////////////////////////////////////////////////////////////////////////

        } // namespace number
    } // namespace platform
} // namespace mp4v2
