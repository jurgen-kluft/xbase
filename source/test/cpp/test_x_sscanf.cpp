#include "xbase/x_runes.h"
#include "xbase/x_printf.h"

#if defined(TARGET_PS3)
	#include <math.h>
#endif

#include "xunittest/xunittest.h"

using namespace xcore;

UNITTEST_SUITE_BEGIN(xsscanf)
{
	UNITTEST_FIXTURE(main)
	{
		UNITTEST_FIXTURE_SETUP()
		{
		}

		UNITTEST_FIXTURE_TEARDOWN()
		{
		}

		UNITTEST_TEST(scan_hash128)
		{
			crunes_t example = "00112233445566778899AABBCCDDEEFF";
			crunes_t fmt = "%02x%02x%02x%02x";

			u8 d[16];
			for (s32 i=0; i<16; i+=4)
			{
				sscanf(example, fmt, va_r_t(&d[i+0]), va_r_t(&d[i+1]), va_r_t(&d[i+2]), va_r_t(&d[i+3]));
			}

			const u8 trusted_vector[] = { 0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff };
			for (s32 i=0;i<16;++i)
				CHECK_EQUAL(trusted_vector[i], d[i]);
		}
	}
}
UNITTEST_SUITE_END
