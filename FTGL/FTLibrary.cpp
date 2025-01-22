/*
 * FTGL - OpenGL font library
 *
 * Copyright (c) 2001-2004 Henry Maddocks <ftgl@opengl.geek.nz>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "config.h"
#include <pthread.h>

#include "FTLibrary.h"

static pthread_once_t threadInit = PTHREAD_ONCE_INIT;
static pthread_key_t threadKey;

static void thread_once(void)
{
    if (pthread_key_create(&threadKey, NULL) != 0)
    {
        cout << "ERRRO: pthread_key_create failed: in " << __FUNCTION__ << ", "
             << __FILE__ << ", line " << __LINE__ << endl;
    }
}

const FTLibrary& FTLibrary::Instance()
{
    pthread_once(&threadInit, thread_once);

    FTLibrary* l = (FTLibrary*)pthread_getspecific(threadKey);

    if (!l)
    {
        l = new FTLibrary();
        pthread_setspecific(threadKey, l);
    }

    return *l;
}

FTLibrary::~FTLibrary()
{
    if (library != 0)
    {
        FT_Done_FreeType(*library);

        delete library;
        library = 0;
    }

    //  if(manager != 0)
    //  {
    //      FTC_Manager_Done(manager);
    //
    //      delete manager;
    //      manager= 0;
    //  }
}

FTLibrary::FTLibrary()
    : library(0)
    , err(0)
{
    Initialise();
}

bool FTLibrary::Initialise()
{
    if (library != 0)
        return true;

    library = new FT_Library;
    err = FT_Init_FreeType(library);

    if (err)
    {
        cout << "ERROR: FTLibrary::Initialise failed" << endl;
        delete library;
        library = 0;
        return false;
    }

    return true;
}
