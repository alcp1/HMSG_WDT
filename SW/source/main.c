//----------------------------------------------------------------------------//
//                               OBJECT HISTORY                               //
//----------------------------------------------------------------------------//
//  REVISION |    DATE     |                               |      AUTHOR      //
//----------------------------------------------------------------------------//
//  1.00     | 21/May/2026 |                               | ALCP             //
// - Initial Version.                                                         //
//----------------------------------------------------------------------------//

/*
* Includes
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "manager.h"

int main(int argc, char *argv[])
{    
    // disable buffering on stdout: Needed for immediate debug
    setbuf(stdout, NULL);     
    // Init manager    
    managerInit();
}