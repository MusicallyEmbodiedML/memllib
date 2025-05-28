# MEMLLIB

Abstractions to make use of the MEMLNaut hardware.


## The Screen

We use Bodmer's TFT_eSPI library.  

In the Arduino/libraries/TFT_eSPI folder, there a file ```User_Setup_Select.h```, which points to a header file that configures display and hardware platform.

Copy the file ```hardware/memlnaut/Setup9999_MEMLNaut.h.renameme``` to the ```User_Setups``` folder, lose the '.renameme' part and then edit ``User_Setup_Select.h``` to include this file only.

```
//#include <User_Setup.h>           // Default setup is root library folder
#include "User_Setups/Setup9999_MEMLNaut.h"
```
