#include "gl/winGL.h"

int isExtensionSupported(const char *extstring)
{
    if (char *s = (char*) glGetString(GL_EXTENSIONS)) //Get our extension-string
    {
        char *temp = strstr(s, extstring);            //Is our extension a string?
        return temp != NULL;                          //Return false.
    }

    return 0;
}

