//----------------------------------------------------------------------
//
//  DEBUG macro 
//

#include <iostream>
using namespace std;

#ifdef NDEBUG
#define FTGL_ABORT ;
#define FTGL_GLDEBUG
#else
#define FTGL_ABORT abort()
#define FTGL_GLDEBUG                            \
    if (GLuint err = glGetError())              \
    {                                           \
        cerr << "ERROR: in "                    \
             << __FILE__                        \
             << ", function "                   \
             << __FUNCTION__                    \
             << ", line "                       \
             << __LINE__                        \
             << ": "                            \
             << gluErrorString(err)             \
             << endl;                           \
        FTGL_ABORT;                             \
    }
#endif

