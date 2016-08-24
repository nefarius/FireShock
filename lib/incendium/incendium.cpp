// incendium.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "incendium.h"


// This is an example of an exported variable
INCENDIUM_API int nincendium=0;

// This is an example of an exported function.
INCENDIUM_API int fnincendium(void)
{
    return 42;
}

// This is the constructor of a class that has been exported.
// see incendium.h for the class definition
Cincendium::Cincendium()
{
    return;
}
