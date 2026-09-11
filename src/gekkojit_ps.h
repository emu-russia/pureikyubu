/*

Paired-Single -> SSE/SSE2 recompiler for the Gekko core.

Paired-Single instructions are what the SDK's matrix and vector code is built
from (psq_l / ps_mul / ps_madd / ps_sum0 / ps_merge / psq_st), and a game that
runs a lot of them spends most of its time here. They are translated in this
separate module so that the optimisation can be enabled and disabled on its own:
build with -DGEKKO_JIT_PS=0 and every PS instruction goes back to the
interpreter fallback, with the integer translator in gekkojit.cpp unchanged.

## How a PS register maps onto an XMM register

The emulated register file keeps a PS register as two independent doubles:
PS0 in fpr[n] and PS1 in ps1[n] (see the PS0/PS1 macros in gekkoc.h). Both hold
the value the guest stored widened to double, and that is exactly what the
interpreter's arithmetic works on - ps_madd evaluates (a * c) + b in double, not
in single precision - so the translation has to reproduce double arithmetic to
stay bit-identical with it.

A double is 64 bits, so a PS0/PS1 pair is a 128 bit quantity and one packed
SSE2 operation handles both lanes at once:

    movsd  xmm, [fpr + 8*n]     ; lane 0 = PS0, upper half cleared
    movhpd xmm, [ps1 + 8*n]     ; lane 1 = PS1
    ...  addpd / mulpd / subpd / divpd / sqrtpd / andpd / orpd / xorpd ...
    movsd  [fpr + 8*n], xmm     ; write PS0 back
    movhpd [ps1 + 8*n], xmm     ; write PS1 back

That is two instructions to gather and two to scatter around a single packed
operation, against the interpreter fallback, which decodes, dispatches, ticks
and - the expensive part - ends the basic block, so the block cache is walked
again for the next instruction.

## What decides the translation

MSR[FP] is checked when the block is compiled rather than at run time: mtmsr and
rfi end the block that contains them and an exception invalidates every block,
so MSR cannot change inside one. When it is clear the instruction is left to the
interpreter, which raises the FP-unavailable exception exactly as before.

The recording forms (ps_add_d and friends) only add COMPUTE_CR1(), which is
translated inline; the comparison forms (ps_cmpu*) and the quantised loads and
stores (psq_*) are not translated yet and stay on the fallback.

## Floating point

addpd and friends round to nearest-even like the interpreter's C++ doubles, and
both use the same MXCSR, so the results are bit-identical - as long as the
interpreter is not built with FP contraction enabled, which would fuse
(a * c) + b into an FMA in ps_madd/ps_madds0 and make the two engines disagree
by one ulp. The default x86-64 baseline has no FMA, so the usual builds are
fine; -march=native (or -mfma) on a Haswell or newer host would break PS.

## NaN payloads are not comparable, and that is not fixable here

When *both* operands of an operation are NaN with different payloads or signs,
which one is propagated is not specified by C++, and the interpreter's own
answer depends on how the host compiler allocated registers for its expression:
in one and the same GCC build, ps_mul propagates the first operand and ps_muls0
the second. A packed SSE instruction propagates the first operand (the
destination register), which is the order written in the source here, so the two
engines can disagree on the *sign or payload* of a NaN result - never on NaN
versus a number, and never on a non-NaN value.

This is why the Paired-Single test in testing/gekko_bench counts those cases
separately instead of failing on them: 264 of 54571 cases over all forms, all of
them two-NaN operations. Pinning it down would mean matching one compiler's
register allocation, and the interpreter's NaN payloads are not a property of
the emulated machine anyway.

*/

#pragma once

#include "pch.h"

// Paired-Single translations, on by default. -DGEKKO_JIT_PS=0 builds them out
// and leaves every PS instruction to the interpreter.
#ifndef GEKKO_JIT_PS
#define GEKKO_JIT_PS 1
#endif

namespace Gekko
{
	class GekkoCore;
	struct DecoderInfo;

	namespace X64
	{
		class Emitter;
	}

	namespace JitPs
	{
		// What the caller has to do after the instruction was translated.
		enum class PsResult
		{
			// Not translated: run the instruction through the interpreter.
			NotHandled,

			// Translated and complete; the block can continue with the next one.
			Done,

			// Translated, but a helper ran and may have raised a memory exception.
			// The caller has to emit its exception check before the block executes
			// anything else, exactly as it does for its own load and store
			// translations.
			DoneMayExcept,
		};

		// Emit the translation of one Paired-Single instruction into the block being
		// compiled.
		PsResult Translate(X64::Emitter& e, GekkoCore* core, const DecoderInfo& di);
	}
}
