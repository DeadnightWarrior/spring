/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include "FPUCheck.h"
#include <cstddef>
#include "System/Log/ILog.h"
#include "lib/streflop/streflop_cond.h"

/**
	@brief checks FPU control registers.
	@return true if everything is fine, false otherwise

	Can be used in an assert() to check the FPU control registers MXCSR and FPUCW,
	e.g. `assert(good_fpu_control_registers());'

	For reference, the layout of the MXCSR register:
        FZ:RC:RC:PM:UM:OM:ZM:DM:IM:Rsvd:PE:UE:OE:ZE:DE:IE
        15 14 13 12 11 10  9  8  7   6   5  4  3  2  1  0
Spring1: 0  0  0  1  1  1  0  1  0   0   0  0  0  0  0  0 = 0x1D00 = 7424
Spring2: 0  0  0  1  1  1  1  1  1   0   0  0  0  0  0  0 = 0x1F80 = 8064
Default: 0  0  0  1  1  1  1  1  1   0   0  0  0  0  0  0 = 0x1F80 = 8064
MaskRsvd:1  1  1  1  1  1  1  1  1   0   0  0  0  0  0  0 = 0xFF80

	And the layout of the 387 FPU control word register:
        Rsvd:Rsvd:Rsvd:X:RC:RC:PC:PC:Rsvd:Rsvd:PM:UM:OM:ZM:DM:IM
         15   14   13 12 11 10  9  8   7    6   5  4  3  2  1  0
Spring1:  0    0    0  0  0  0  0  0   0    0   1  1  1  0  1  0 = 0x003A = 58
Spring2:  0    0    0  0  0  0  0  0   0    0   1  1  1  1  1  1 = 0x003F = 63
Default:  0    0    0  0  0  0  1  1   0    0   1  1  1  1  1  1 = 0x033F = 831
MaskRsvd: 0    0    0  1  1  1  1  1   0    0   1  1  1  1  1  1 = 0x1F3F

	Where:
		Rsvd - Reserved
		FZ   - Flush to Zero
		RC   - Rounding Control
		PM   - Precision Mask
		UM   - Underflow Mask
		OM   - Overflow Mask
		ZM   - Zerodivide Mask
		DM   - Denormal Mask
		IM   - Invalid Mask
		PE   - Precision Exception
		UE   - Underflow Exception
		OE   - Overflow Exception
		ZE   - Zerodivide Exception
		DE   - Denormal Exception
		IE   - Invalid Exception
		X    - Infinity control (unused on 387 and higher)
		PC   - Precision Control

		Spring1  - Control word used by spring in code in CGame::SimFrame().
		Spring2  - Control word used by spring in code everywhere else.
		Default  - Default control word according to Intel.
		MaskRsvd - Masks out the reserved bits.

	Source: Intel Architecture Software Development Manual, Volume 1, Basic Architecture
*/
void good_fpu_control_registers(const char* text)
{
	// We are paranoid.
	// We don't trust the enumeration constants from streflop / (g)libc.

	// accepted/syncsafe FPU states:
#if defined(__SUPPORT_SNAN__) && !defined(USE_GML)	// -fsignaling-nans
	static const int sse_a = (0x1937 & 0xFF80);
	static const int sse_b = (0x1925 & 0xFF80);
	static const int x87_a = (0x0072 & 0x1F3F);
	static const int x87_b = 0x003F;
#else
	static const int sse_a = 0x1D00;
	static const int sse_b = 0x1F80;
	static const int x87_a = 0x003A;
	static const int x87_b = 0x003F;
#endif

#if defined(STREFLOP_SSE)
	// struct
	streflop::fpenv_t fenv;
	streflop::fegetenv(&fenv);

	bool ret = ((fenv.sse_mode & 0xFF80) == sse_a || (fenv.sse_mode & 0xFF80) == sse_b) &&
	           ((fenv.x87_mode & 0x1F3F) == x87_a || (fenv.x87_mode & 0x1F3F) == x87_b);

	if (!ret) {
		LOG_L(L_WARNING, "[%s] Sync warning: (env.sse_mode) MXCSR 0x%04X instead of 0x%04X or 0x%04X (\"%s\")", __FUNCTION__, fenv.sse_mode, sse_a, sse_b, text);
		LOG_L(L_WARNING, "[%s] Sync warning: (env.x87_mode) FPUCW 0x%04X instead of 0x%04X or 0x%04X (\"%s\")", __FUNCTION__, fenv.x87_mode, x87_a, x87_b, text);

		// Set single precision floating point math.
		streflop_init<streflop::Simple>();
	#if defined(__SUPPORT_SNAN__) && !defined(USE_GML)
		streflop::feraiseexcept(streflop::FPU_Exceptions(FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW));
	#endif
	}

#elif defined(STREFLOP_X87)
	// short int
	streflop::fpenv_t fenv;
	streflop::fegetenv(&fenv);

	bool ret = (fenv & 0x1F3F) == x87_a || (fenv & 0x1F3F) == x87_b;

	if (!ret) {
		LOG_L(L_WARNING, "[%s] Sync warning: FPUCW 0x%04X instead of 0x%04X or 0x%04X (\"%s\")", __FUNCTION__, fenv, x87_a, x87_b, text);

		// Set single precision floating point math.
		streflop_init<streflop::Simple>();
	#if defined(__SUPPORT_SNAN__) && !defined(USE_GML)
		streflop::feraiseexcept(streflop::FPU_Exceptions(FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW));
	#endif
	}
#endif
}