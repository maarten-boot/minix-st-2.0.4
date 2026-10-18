/* <stdint.h> for building the ACK passes as Minix programs (mkpass.py):
 * Minix 2.0.4 has none.  The rosco ABI is gcc -mshort: char 8, short and
 * int 16, long and pointers 32 bits.
 */
#ifndef _ROSCO_STDINT_H
#define _ROSCO_STDINT_H

typedef signed char		int8_t;
typedef short			int16_t;
typedef long			int32_t;
typedef long long		int64_t;
typedef unsigned char		uint8_t;
typedef unsigned short		uint16_t;
typedef unsigned long		uint32_t;
typedef unsigned long long	uint64_t;

typedef int8_t		int_least8_t;
typedef int16_t		int_least16_t;
typedef int32_t		int_least32_t;
typedef int64_t		int_least64_t;
typedef uint8_t		uint_least8_t;
typedef uint16_t	uint_least16_t;
typedef uint32_t	uint_least32_t;
typedef uint64_t	uint_least64_t;

typedef int16_t		int_fast8_t;
typedef int16_t		int_fast16_t;
typedef int32_t		int_fast32_t;
typedef int64_t		int_fast64_t;
typedef uint16_t	uint_fast8_t;
typedef uint16_t	uint_fast16_t;
typedef uint32_t	uint_fast32_t;
typedef uint64_t	uint_fast64_t;

typedef long		intptr_t;
typedef unsigned long	uintptr_t;
typedef long long	intmax_t;
typedef unsigned long long uintmax_t;

#define INT8_MIN	(-128)
#define INT8_MAX	127
#define UINT8_MAX	255
#define INT16_MIN	(-32767-1)
#define INT16_MAX	32767
#define UINT16_MAX	65535U
#define INT32_MIN	(-2147483647L-1)
#define INT32_MAX	2147483647L
#define UINT32_MAX	4294967295UL
#define INT64_MIN	(-9223372036854775807LL-1)
#define INT64_MAX	9223372036854775807LL
#define UINT64_MAX	18446744073709551615ULL
#define INTPTR_MIN	INT32_MIN
#define INTPTR_MAX	INT32_MAX
#define UINTPTR_MAX	UINT32_MAX
#define INTMAX_MIN	INT64_MIN
#define INTMAX_MAX	INT64_MAX
#define UINTMAX_MAX	UINT64_MAX
#define SIZE_MAX	65535U		/* size_t is unsigned int */

#define INT8_C(c)	c
#define INT16_C(c)	c
#define INT32_C(c)	c ## L
#define INT64_C(c)	c ## LL
#define UINT8_C(c)	c
#define UINT16_C(c)	c ## U
#define UINT32_C(c)	c ## UL
#define UINT64_C(c)	c ## ULL
#define INTMAX_C(c)	c ## LL
#define UINTMAX_C(c)	c ## ULL

#endif /* _ROSCO_STDINT_H */
