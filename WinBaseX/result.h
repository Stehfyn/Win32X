#ifndef RESULT_H
#define RESULT_H

/*
 * result.h -- internal early-return guard macros. Each expands to a do/while(0) statement that
 * returns when its condition holds. The base forms (RETURN_VOID_IF / RETURN_VALUE_IF) carry the
 * control flow; everything else is a named convenience over them. Use the precondition forms for
 * null/success guards; they keep guard rows uniform and one-line.
 */

#define RETURN_VOID_IF(expr) \
    do \
    { \
        if ((expr)) \
        { \
            return; \
        } \
    } while (0)

#define RETURN_VALUE_IF(expr, value) \
    do \
    { \
        if ((expr)) \
        { \
            return (value); \
        } \
    } while (0)

#define RETURN_VOID_IF_NOT(expr)  RETURN_VOID_IF(!(expr))
#define RETURN_VOID_IF_NULL(ptr)  RETURN_VOID_IF(NULL == (ptr))

#define RETURN_VALUE_IF_NOT(expr, value)        RETURN_VALUE_IF(!(expr), value)
#define RETURN_VALUE_IF_NULL(ptr, value)        RETURN_VALUE_IF(NULL == (ptr), value)
#define RETURN_VALUE_IF_NOT_POSITIVE(v, value)  RETURN_VALUE_IF(0 >= (v), value)
#define RETURN_VALUE_IF_NEGATIVE(v, value)      RETURN_VALUE_IF(0 > (v), value)

#define RETURN_IF(expr)                RETURN_VOID_IF(expr)
#define RETURN_IF_NOT(expr)            RETURN_VOID_IF_NOT(expr)
#define RETURN_IF_NULL(ptr)            RETURN_VOID_IF_NULL(ptr)
#define RETURN_IF_ZERO(expr)           RETURN_VOID_IF(0 == (expr))
#define RETURN_IF_NOT_POSITIVE(value)  RETURN_VOID_IF(0 >= (value))
#define RETURN_IF_NEGATIVE(value)      RETURN_VOID_IF(0 > (value))

#define RETURN_FALSE_IF(expr)           RETURN_VALUE_IF(expr, 0)
#define RETURN_FALSE_IF_NOT(expr)       RETURN_VALUE_IF_NOT(expr, 0)
#define RETURN_FALSE_IF_NULL(ptr)       RETURN_VALUE_IF_NULL(ptr, 0)
#define RETURN_FALSE_IF_ZERO(expr)      RETURN_VALUE_IF(0 == (expr), 0)
#define RETURN_FALSE_IF_NOT_POSITIVE(v) RETURN_VALUE_IF(0 >= (v), 0)
#define RETURN_FALSE_IF_NEGATIVE(v)     RETURN_VALUE_IF(0 > (v), 0)

#define RETURN_TRUE_IF(expr)            RETURN_VALUE_IF(expr, 1)
#define RETURN_TRUE_IF_NOT(expr)        RETURN_VALUE_IF_NOT(expr, 1)
#define RETURN_TRUE_IF_NULL(ptr)        RETURN_VALUE_IF_NULL(ptr, 1)
#define RETURN_TRUE_IF_ZERO(expr)       RETURN_VALUE_IF(0 == (expr), 1)
#define RETURN_TRUE_IF_NOT_POSITIVE(v)  RETURN_VALUE_IF(0 >= (v), 1)
#define RETURN_TRUE_IF_NEGATIVE(v)      RETURN_VALUE_IF(0 > (v), 1)

#define RETURN_NULL_IF(expr)            RETURN_VALUE_IF(expr, NULL)
#define RETURN_NULL_IF_NOT(expr)        RETURN_VALUE_IF_NOT(expr, NULL)
#define RETURN_NULL_IF_NULL(ptr)        RETURN_VALUE_IF_NULL(ptr, NULL)
#define RETURN_NULL_IF_NOT_POSITIVE(v)  RETURN_VALUE_IF(0 >= (v), NULL)
#define RETURN_NULL_IF_NEGATIVE(v)      RETURN_VALUE_IF(0 > (v), NULL)

#define RETURN_ZERO_IF(expr)            RETURN_VALUE_IF(expr, 0)
#define RETURN_ZERO_IF_NOT(expr)        RETURN_VALUE_IF_NOT(expr, 0)
#define RETURN_ZERO_IF_NULL(ptr)        RETURN_VALUE_IF_NULL(ptr, 0)
#define RETURN_ZERO_IF_NOT_POSITIVE(v)  RETURN_VALUE_IF(0 >= (v), 0)
#define RETURN_ZERO_IF_NEGATIVE(v)      RETURN_VALUE_IF(0 > (v), 0)

#endif /* RESULT_H */
