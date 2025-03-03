#ifndef MP4V2_PLATFORM_SYS_ERROR_H
#define MP4V2_PLATFORM_SYS_ERROR_H

namespace mp4v2
{
    namespace platform
    {
        namespace sys
        {

            ///////////////////////////////////////////////////////////////////////////////

            MP4V2_EXPORT int getLastError();
            MP4V2_EXPORT const char* getLastErrorStr();
            MP4V2_EXPORT const char* getErrorStr(int);

            ///////////////////////////////////////////////////////////////////////////////

        } // namespace sys
    } // namespace platform
} // namespace mp4v2

#endif // MP4V2_PLATFORM_SYS_ERROR_H
