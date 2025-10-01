#ifndef TYPE_DEF_H_
#define TYPE_DEF_H_

#include <nw/types.h>

#include <cafe.h>
#include <cafe/gfd.h>

#define EFT_MEMUTIL_CAFE_DCBZ_OFFSET(addr, offset)	asm("	dcbz	%0,%1" : "+g"(addr), "+g"(offset) )

#include <nw/math.h>
#include <nw/ut/ut_Color.h>

#endif // TYPE_DEF_H_
