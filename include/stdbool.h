/* C99 stdbool.h. This compiler has no native _Bool type, so bool is just
   an unsigned char (0 or 1) -- a #define, not a typedef, matching the
   real stdbool.h so `#undef bool` still works for code that wants the
   identifier back.

   olduino.h (widely #included across this codebase) already #defines
   true/false itself (guarded by #ifndef olduino_H, no __bool_true_false_
   are_defined-style guard of its own) with the same values used here, so
   the guards below just make sure both headers can be included together
   in either order without a "redefined" warning -- not just relying on
   the preprocessor's usual identical-redefinition tolerance, since that
   still isn't guaranteed to be quiet on every cpp. */
#ifndef _STDBOOL_H
#define _STDBOOL_H

#ifndef bool
#define bool unsigned char
#endif
#ifndef true
#define true 1
#endif
#ifndef false
#define false 0
#endif

#define __bool_true_false_are_defined 1

#endif
