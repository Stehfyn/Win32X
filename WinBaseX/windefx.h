#ifndef WINDEFX_H
#define WINDEFX_H

#include <windows.h>

/* Sign predicates for a signed integer expression. The relational/equality operators already yield
   canonical 0/1, so no cast to BOOL is needed (CONV-4.7). */
#define IsNegative(x)     (0 >  (x))
#define IsNonNegative(x)  (0 <= (x))
#define IsPositive(x)     (0 <  (x))
#define IsNonPositive(x)  (0 >= (x))
#define IsZero(x)         (0 == (x))
#define IsNonZero(x)      (0 != (x))

/* Absolute value of a signed integer expression. */
#define ABS(x) (IsNegative(x) ? -(x) : (x))

/* Clamp x into the inclusive range [lo, hi]. Equivalent to
   max(min(x, hi), lo) but without pulling in <windows.h>. */
#define BOUND(x, lo, hi) (((x) < (lo)) ? (lo) : (((hi) < (x)) ? (hi) : (x)))

/* Bit-flag operations on an lvalue integer `obj`. `f` is the mask. */
#define SetFlag(obj, f)     do { (obj) |=  (f); } while (0)
#define ToggleFlag(obj, f)  do { (obj) ^=  (f); } while (0)
#define ClearFlag(obj, f)   do { (obj) &= ~(f); } while (0)

/* Presence tests. "Set" means every bit in mask `f` is present in
   `obj`; for a single-bit mask the result is the obvious one. The
   == and != operators already produce canonical 0/1, so no cast to
   BOOL is needed (CONV-4.7). */
#define IsFlagSet(obj, f)    (((f) & (obj)) == (f))
#define IsFlagClear(obj, f)  (((f) & (obj)) != (f))

/* Width and height of a RECT. ABS guards against inverted
   rectangles produced by GDI/text-measurement edge cases. */
#define RECTWIDTH(rc)  (ABS((rc).right  - (rc).left))
#define RECTHEIGHT(rc) (ABS((rc).bottom - (rc).top))

/* Extract the min (top-left) or max (bottom-right) corner of a RECT
   into an lvalue POINT. */
#define RECTMINPOINT(rc, pt) \
    do { (pt).x = (rc).left;  (pt).y = (rc).top;    } while (0)

#define RECTMAXPOINT(rc, pt) \
    do { (pt).x = (rc).right; (pt).y = (rc).bottom; } while (0)

/* Mirror a POINT across a RECT's horizontal or vertical midline. */
#define MIRRORPOINT_HORZ(rc, pt) \
    do { (pt).x = ((rc).right  + (rc).left) - (pt).x; } while (0)

#define MIRRORPOINT_VERT(rc, pt) \
    do { (pt).y = ((rc).bottom + (rc).top)  - (pt).y; } while (0)

#endif /* WINDEFX_H */
