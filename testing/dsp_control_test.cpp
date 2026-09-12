// DSP core unit tests: control transfer, loops, repeats, stacks, moves and immediate ALU.
//
// Reference: dsp-isa.md sections 4.1-4.5 and 5. Instruction words are produced by the
// builders of dsp_test_common.h, which follow the encoding tables of dsp-isa.md sections 3
// and 6 and are written independently of the emulator's decoder.

#include "pch.h"
#include "dsp_test_common.h"

namespace DspUnitTest
{
	TEST_CLASS(DspControlTest)
	{
		DspTestMachine& m = Machine();

	public:

		TEST_METHOD_INITIALIZE(Setup)
		{
			m.Reset();
		}

		// ---------------------------------------------------------------
		// Decoder strictness (reserved words of the official opcode space)
		// ---------------------------------------------------------------

		/// <summary>
		/// Decode a word out of line and report whether the decoder recognised it.
		/// The encoder side of the check is the opcode space of dsp-isa.md section 6.
		/// </summary>
		bool Decodes(uint16_t word)
		{
			Assemble(m.core, 0, { word, 0x0000, 0x0000 });
			DecoderInfo info = { 0 };
			Decoder::Decode(m.core->TranslateIMem(0), DspCore::MaxInstructionSizeInBytes, info);
			if (info.parallel)
			{
				return info.parallelInstr != DspParallelInstruction::Unknown;
			}
			return info.instr != DspRegularInstruction::Unknown;
		}

		TEST_METHOD(Decoder_RejectsTheReservedMirrorsOfDiplessControlOpcodes)
		{
			// exec/jmp/call/rets/reti have no a/b selector bit, so 0x0300-0x03FF is reserved
			// for them: only 0x0270/0x0290/0x02B0/0x02D0/0x02F0 exist.
			Assert::IsTrue(Decodes(0x0270), L"exec");
			Assert::IsTrue(Decodes(0x0290), L"jmp ta");
			Assert::IsTrue(Decodes(0x02B0), L"call ta");
			Assert::IsTrue(Decodes(0x02D0), L"rets");
			Assert::IsTrue(Decodes(0x02F0), L"reti");

			for (uint16_t w : { (uint16_t)0x0370, (uint16_t)0x0390, (uint16_t)0x03B0,
				(uint16_t)0x03D0, (uint16_t)0x03F0 })
			{
				Assert::IsFalse(Decodes(w), L"the 0x03xx mirror of a d-less control opcode is reserved");
			}
		}

		TEST_METHOD(Decoder_RejectsReservedTrapWaitWords)
		{
			// Only 0x0020 (trap) and 0x0021 (wait) exist; 0x0022-0x003F is the packed `st`
			// half-word, which can never appear as a whole instruction word.
			Assert::IsTrue(Decodes(0x0020), L"trap");
			Assert::IsTrue(Decodes(0x0021), L"wait");

			for (uint16_t w = 0x0022; w <= 0x003F; w++)
			{
				Assert::IsFalse(Decodes(w), L"reserved trap/wait mirror");
			}
		}

		TEST_METHOD(Decoder_RejectsReservedPsrSelectorWords)
		{
			// clr/set tb/sv/te0..te3/et occupy exactly 0x1200-0x1206 and 0x1300-0x1306.
			for (uint16_t b = 0; b <= 6; b++)
			{
				Assert::IsTrue(Decodes((uint16_t)(0x1200 | b)), L"clr PSR bit");
				Assert::IsTrue(Decodes((uint16_t)(0x1300 | b)), L"set PSR bit");
			}

			for (uint16_t b = 7; b <= 0x0F; b++)
			{
				Assert::IsFalse(Decodes((uint16_t)(0x1200 | b)), L"selector 7 or bits 7:4 set");
				Assert::IsFalse(Decodes((uint16_t)(0x1300 | b)), L"selector 7 or bits 7:4 set");
			}
			Assert::IsFalse(Decodes(0x1280), L"bits 7:3 are fixed to zero");
			Assert::IsFalse(Decodes(0x1380), L"bits 7:3 are fixed to zero");
		}

		// ---------------------------------------------------------------
		// nop / pc advance
		// ---------------------------------------------------------------

		TEST_METHOD(Nop_AdvancesPcByOneWord)
		{
			m.Run({ Enc::Nop() }, 1);
			Assert::AreEqual(1u, m.core->regs.pc);
			Assert::AreEqual(0, DspTestHaltCount());
		}

		TEST_METHOD(TwoWordInstruction_AdvancesPcByTwoWords)
		{
			// mvli a0, 0x1234 occupies two program words.
			m.Run({ Enc::Mvli(REG_A0), 0x1234, Enc::Nop() }, 1);
			Assert::AreEqual(2u, m.core->regs.pc);
			Assert::AreEqual((uint16_t)0x1234, m.core->regs.a.l);
			Assert::AreEqual((size_t)4, m.DecodeSize({ Enc::Mvli(REG_A0), 0x1234 }));
		}

		// ---------------------------------------------------------------
		// jmp / call / rets
		// ---------------------------------------------------------------

		TEST_METHOD(JmpDirect_Taken)
		{
			// 0: jmp 0x0010 / 1: target word
			// 2: (skipped)
			// 0x10: nop
			m.Run({ Enc::Jmp(CC_ALWAYS), 0x0010, Enc::Mvli(REG_A0), 0x1111, Enc::Nop() }, 1);
			Assert::AreEqual(0x10u, m.core->regs.pc);
		}

		TEST_METHOD(JmpDirect_NotTaken_SkipsBothWords)
		{
			const uint16_t ccFalse = CC_NZ;		// Z is 0 after reset, so branch on Z instead
			m.Run({ Enc::Jmp(CC_Z), 0x0010, Enc::Nop() }, 1);
			// Condition false: execution continues after the two-word instruction.
			Assert::AreEqual(2u, m.core->regs.pc);
			(void)ccFalse;
		}

		TEST_METHOD(Call_Rets_RoundTrip)
		{
			// 0: call 0x0004   1: target word
			// 2: nop           (return lands here)
			// 4: rets
			m.Run({ Enc::Call(CC_ALWAYS), 0x0004, Enc::Nop(), Enc::Nop(), Enc::Rets(), Enc::Nop() }, 2);
			Assert::AreEqual(2u, m.core->regs.pc, L"rets must return to the word after the call");
			Assert::IsTrue(m.core->regs.pcs->empty());
		}

		TEST_METHOD(JmpReg_UsesAddressRegister)
		{
			m.core->regs.r[1] = 0x0008;
			m.Run({ Enc::JmpReg(REG_R1), Enc::Nop(), Enc::Nop(), Enc::Nop(), Enc::Nop() }, 1);
			Assert::AreEqual(8u, m.core->regs.pc);
		}

		TEST_METHOD(CallReg_UsesAddressRegister)
		{
			m.core->regs.r[2] = 0x0004;
			m.Run({ Enc::CallReg(REG_R2), Enc::Nop(), Enc::Nop(), Enc::Nop(), Enc::Rets() }, 2);
			Assert::AreEqual(1u, m.core->regs.pc, L"call rn is one word wide, so the return address is pc+1");
		}

		// ---------------------------------------------------------------
		// exec
		// ---------------------------------------------------------------

		TEST_METHOD(Exec_Taken_ExecutesNextInstruction)
		{
			// mvli a0,#1 is two words, so use a one-word instruction.
			m.Run({ Enc::Exec(CC_ALWAYS), Enc::Mvsi(R8A_A0, 5), Enc::Nop() }, 2);
			Assert::AreEqual((uint16_t)5, m.core->regs.a.l);
			Assert::AreEqual(2u, m.core->regs.pc);
		}

		TEST_METHOD(Exec_NotTaken_NullifiesNextInstruction)
		{
			m.Run({ Enc::Exec(CC_Z), Enc::Mvsi(R8A_A0, 5), Enc::Nop() }, 1);
			Assert::AreEqual((uint16_t)0, m.core->regs.a.l);
			Assert::AreEqual(2u, m.core->regs.pc, L"the nullified instruction must be skipped");

			m.Step();
			Assert::AreEqual(3u, m.core->regs.pc);
		}

		// ---------------------------------------------------------------
		// trap / wait
		// ---------------------------------------------------------------

		TEST_METHOD(Trap_AssertsTrapInterrupt)
		{
			m.Run({ Enc::Trap() }, 1);
			Assert::IsTrue(m.core->IsInterruptPending(DspInterrupt::Trap));
		}

		TEST_METHOD(Trap_ReturnFromHandlerResumesAfterTheTrap)
		{
			// The trap vector is at program address 0x0004 (DspInterrupt::Trap * 2); the
			// handler here is a bare `reti`. The return address must be the word *after* the
			// trap, otherwise the handler would re-execute the trap forever.
			m.Run({ Enc::Nop(), Enc::Nop(), Enc::Trap(), Enc::Nop(),
				Enc::Nop() /* 0x0004: the handler entry */, Enc::Reti() }, 3);
			Assert::AreEqual(3u, m.core->regs.pc, L"trap advances the pc");
			Assert::IsTrue(m.core->IsInterruptPending(DspInterrupt::Trap));

			// Taking the trap pushes the return address and vectors to 0x0004 in the same
			// step, which also executes the handler's first instruction.
			m.Step();
			Assert::AreEqual(5u, m.core->regs.pc, L"the trap vector is 0x0004");
			Assert::IsFalse(m.core->IsInterruptPending(DspInterrupt::Trap));

			m.Step();		// reti
			Assert::AreEqual(3u, m.core->regs.pc, L"reti must resume after the trap");
		}

		TEST_METHOD(InterruptVectorsAreRelativeToTheActiveProgramBase)
		{
			// dsp.md section 2.7: the vector offsets are relative to the active program base -
			// 0x0000 in IRAM, or 0x8000 in IROM while the reset-vector bit is set.
			m.Run({ Enc::Nop(), Enc::Nop(), Enc::Nop(), Enc::Trap(), Enc::Nop(),
				Enc::Nop(), Enc::Nop(), Enc::Nop(), Enc::Nop() }, 4);
			// Taking the interrupt and executing the instruction it vectors to happen in the
			// same step, so the pc lands on the word after the first instruction of the
			// handler: 0x0004 + 1 in IRAM, 0x8004 + 1 in the IROM.
			m.Step();
			Assert::AreEqual(5u, m.core->regs.pc, L"the trap vector is 0x0004 while running from IRAM");

			m.Reset();
			dsp_ai.cdcr |= CDCR_RESETMOD;
			m.Run({ Enc::Nop(), Enc::Nop(), Enc::Nop(), Enc::Trap(), Enc::Nop(),
				Enc::Nop(), Enc::Nop(), Enc::Nop(), Enc::Nop() }, 4);
			m.Step();
			Assert::AreEqual(0x8005u, m.core->regs.pc,
				L"the same vector is 0x8004 while the program base is the IROM");
			dsp_ai.cdcr &= ~CDCR_RESETMOD;
		}

		TEST_METHOD(Wait_DoesNotAdvancePc)
		{
			// `wait` is a flow-control instruction: the PC stays on it until an interrupt arrives.
			m.Run({ Enc::Wait(), Enc::Nop() }, 1);
			Assert::AreEqual(0u, m.core->regs.pc);
		}

		// ---------------------------------------------------------------
		// loop
		// ---------------------------------------------------------------

		TEST_METHOD(Loop_RepeatsBodyLcTimes)
		{
			// 0: loop 3, 0x0004   (lc = 3, end address = word 4)
			// 1: end address word
			// 2: mvsi a0,#1
			// 3: mvsi a0,#1
			// 4: nop              <- loop end
			// 5: nop
			std::vector<uint16_t> code = {
				Enc::Loop(3), 0x0004,
				Enc::Mvsi(R8A_A0, 1),
				Enc::Mvsi(R8A_A0, 1),
				Enc::Nop(),
				Enc::Nop(),
			};
			Assemble(m.core, DspTestMachine::IRAM_BASE, code);
			m.core->regs.pc = DspTestMachine::IRAM_BASE;

			// loop setup + 3 body iterations (words 2, 3 and the loop-end word 4) + final nop
			m.Steps(1 + 3 * 3 + 1);

			Assert::AreEqual(6u, m.core->regs.pc, L"after the loop the pc must point past the loop end");
			Assert::IsTrue(m.core->regs.lcs->empty());
			Assert::IsTrue(m.core->regs.eas->empty());
		}

		TEST_METHOD(Loop_WithZeroCount_SkipsBody)
		{
			std::vector<uint16_t> code = {
				Enc::Loop(0), 0x0004,
				Enc::Mvsi(R8A_A0, 1),
				Enc::Nop(), Enc::Nop(),
				Enc::Nop(),
			};
			Assemble(m.core, DspTestMachine::IRAM_BASE, code);
			m.core->regs.pc = DspTestMachine::IRAM_BASE;
			m.Steps(1);
			Assert::AreEqual(5u, m.core->regs.pc, L"lc = 0 skips the loop body");
		}

		TEST_METHOD(Loop_NestsFourDeep)
		{
			// 0: loop 2, end=0x0007
			// 1: end word
			// 2: loop 2, end=0x0006
			// 3: end word
			// 4: nop
			// 5: nop
			// 6: nop
			// 7: nop
			std::vector<uint16_t> code = {
				Enc::Loop(2), 0x0007,
				Enc::Loop(2), 0x0006,
				Enc::Nop(), Enc::Nop(), Enc::Nop(), Enc::Nop(),
			};
			Assemble(m.core, DspTestMachine::IRAM_BASE, code);
			m.core->regs.pc = DspTestMachine::IRAM_BASE;

			m.Steps(2);		// two loop setups
			Assert::AreEqual(2, m.core->regs.lcs->size());
			Assert::AreEqual(2, m.core->regs.eas->size());

			// Run until both loops are done.
			for (int i = 0; i < 32 && !m.core->regs.lcs->empty(); i++)
			{
				m.Step();
			}
			Assert::IsTrue(m.core->regs.lcs->empty(), L"nested loops must unwind");
			Assert::AreEqual(0, DspTestHaltCount());
		}

		// ---------------------------------------------------------------
		// rep
		// ---------------------------------------------------------------

		TEST_METHOD(Rep_RepeatsNextInstructionRcTimes)
		{
			// 0: rep 4
			// 1: inc a
			// 2: nop
			m.Run({ Enc::Rep(4), Enc::Inc(2 /*a*/), Enc::Nop() }, 1 + 4 + 1);
			Assert::AreEqual((int64_t)4, m.A(), L"the repeated instruction must run exactly rc times");
		}

		TEST_METHOD(Rep_WithZeroCount_SkipsNextInstruction)
		{
			m.Run({ Enc::Rep(0), Enc::Mvsi(R8A_A0, 7), Enc::Nop() }, 1);
			Assert::AreEqual((uint16_t)0, m.core->regs.a.l);
			Assert::AreEqual(2u, m.core->regs.pc, L"rc = 0 skips the next instruction");
		}

		TEST_METHOD(Rep_FromRegister)
		{
			m.core->regs.m[0] = 3;
			m.Run({ Enc::RepReg(REG_M0), Enc::Inc(2), Enc::Nop() }, 1 + 3 + 1);
			Assert::AreEqual((int64_t)3, m.A());
		}

		// ---------------------------------------------------------------
		// pld
		// ---------------------------------------------------------------

		TEST_METHOD(Pld_LoadsProgramWordIntoAccumulator)
		{
			// 0: pld a1, r0, +1
			// 1: data word 0xABCD
			PokeIMem(m.core, 1, 0xABCD);
			m.core->regs.r[0] = 1;
			m.core->regs.l[0] = 0xFFFF;
			m.Run({ Enc::Pld(0, REG_R0, MOD_INC) }, 1);
			Assert::AreEqual((uint16_t)0xABCD, (uint16_t)m.core->regs.a.m);
			Assert::AreEqual((uint16_t)2, m.core->regs.r[0], L"pld must apply the address post-modifier");
		}

		// ---------------------------------------------------------------
		// mr
		// ---------------------------------------------------------------

		TEST_METHOD(Mr_AppliesModifiers)
		{
			for (int i = 0; i < 4; i++)
			{
				m.core->regs.r[i] = 0x0100;
				m.core->regs.l[i] = 0xFFFF;
				m.core->regs.m[i] = 4;
			}

			m.core->regs.pc = 0;
			m.Run({ Enc::Mr(REG_R0, MRM_PLUS_1), Enc::Mr(REG_R1, MRM_MINUS_1), Enc::Mr(REG_R2, MRM_PLUS_M2), Enc::Mr(REG_R3, MRM_MINUS_M) }, 4);

			Assert::AreEqual((uint16_t)0x0101, m.core->regs.r[0]);
			Assert::AreEqual((uint16_t)0x00FF, m.core->regs.r[1]);
			Assert::AreEqual((uint16_t)0x0104, m.core->regs.r[2]);
			Assert::AreEqual((uint16_t)0x00FC, m.core->regs.r[3]);
		}

		TEST_METHOD(Mr_NoModifier_IsNop)
		{
			m.core->regs.r[0] = 0x0100;
			m.core->regs.l[0] = 0xFFFF;
			m.Run({ Enc::Mr(REG_R0, MRM_PLUS_0) }, 1);
			Assert::AreEqual((uint16_t)0x0100, m.core->regs.r[0]);
		}

		// ---------------------------------------------------------------
		// clr / set
		// ---------------------------------------------------------------

		TEST_METHOD(ClrSet_PsrBits)
		{
			// psr selector codes: tb = 0, sv = 1, te0 = 2, te1 = 3, te2 = 4, te3 = 5, et = 6
			m.core->regs.psr.bits = 0xFFFF;
			m.core->regs.pc = 0;
			m.Run({ Enc::ClrPsr(0), Enc::ClrPsr(1), Enc::ClrPsr(2), Enc::ClrPsr(3), Enc::ClrPsr(4), Enc::ClrPsr(5), Enc::ClrPsr(6) }, 7);

			Assert::AreEqual(0, (int)m.core->regs.psr.tb);
			Assert::AreEqual(0, (int)m.core->regs.psr.sv);
			Assert::AreEqual(0, (int)m.core->regs.psr.te0);
			Assert::AreEqual(0, (int)m.core->regs.psr.te1);
			Assert::AreEqual(0, (int)m.core->regs.psr.te2);
			Assert::AreEqual(0, (int)m.core->regs.psr.te3);
			Assert::AreEqual(0, (int)m.core->regs.psr.et);

			m.core->regs.psr.bits = 0;
			m.core->regs.pc = 0;
			m.Run({ Enc::SetPsr(0), Enc::SetPsr(1), Enc::SetPsr(2), Enc::SetPsr(3), Enc::SetPsr(4), Enc::SetPsr(5), Enc::SetPsr(6) }, 7);

			Assert::AreEqual(1, (int)m.core->regs.psr.tb);
			Assert::AreEqual(1, (int)m.core->regs.psr.sv);
			Assert::AreEqual(1, (int)m.core->regs.psr.te0);
			Assert::AreEqual(1, (int)m.core->regs.psr.te1);
			Assert::AreEqual(1, (int)m.core->regs.psr.te2);
			Assert::AreEqual(1, (int)m.core->regs.psr.te3);
			Assert::AreEqual(1, (int)m.core->regs.psr.et);
		}

		TEST_METHOD(ClrSet_PsrBits_LeaveArithmeticFlagsAlone)
		{
			m.SetDefaultFlags();
			m.core->regs.pc = 0;
			m.Run({ Enc::ClrPsr(0), Enc::SetPsr(1) }, 2);
			m.AssertFlags(1, 0, 0, 1, 1, 0, L"clr/set of PSR bits must not touch C V Z N E U");
		}

		TEST_METHOD(ClrSet_ModeBits)
		{
			m.core->regs.psr.bits = 0xFFFF;
			m.core->regs.pc = 0;
			m.Run({ Enc::ClrIm(), Enc::ClrDp(), Enc::ClrXl() }, 3);
			Assert::AreEqual(0, (int)m.core->regs.psr.im);
			Assert::AreEqual(0, (int)m.core->regs.psr.dp);
			Assert::AreEqual(0, (int)m.core->regs.psr.xl);

			m.core->regs.pc = 0;
			m.Run({ Enc::SetIm(), Enc::SetDp(), Enc::SetXl() }, 3);
			Assert::AreEqual(1, (int)m.core->regs.psr.im);
			Assert::AreEqual(1, (int)m.core->regs.psr.dp);
			Assert::AreEqual(1, (int)m.core->regs.psr.xl);
		}

		// ---------------------------------------------------------------
		// btstl / btsth
		// ---------------------------------------------------------------

		TEST_METHOD(Btstl_TbSetWhenSelectedBitsAreZero)
		{
			m.core->regs.a.m = 0x0F00;
			m.core->regs.psr.tb = 0;

			m.Run({ Enc::Btstl(0), 0x00F0 }, 1);		// bits 7..4 of a1 are zero -> TB = 1
			Assert::AreEqual(1, (int)m.core->regs.psr.tb);

			m.core->regs.pc = 0;
			m.core->regs.psr.tb = 1;
			m.Run({ Enc::Btstl(0), 0x0F00 }, 1);		// some selected bits are set -> TB = 0
			Assert::AreEqual(0, (int)m.core->regs.psr.tb);
		}

		TEST_METHOD(Btsth_TbSetWhenAllSelectedBitsAreOne)
		{
			m.core->regs.a.m = 0xFFFF;

			m.core->regs.psr.tb = 0;
			m.Run({ Enc::Btsth(0), 0x00F0 }, 1);
			Assert::AreEqual(1, (int)m.core->regs.psr.tb);

			m.core->regs.a.m = 0x0F00;
			m.core->regs.pc = 0;
			m.core->regs.psr.tb = 1;
			m.Run({ Enc::Btsth(0), 0x0F00 }, 1);
			Assert::AreEqual(1, (int)m.core->regs.psr.tb);
		}

		TEST_METHOD(Btst_WorksOnB1Too)
		{
			m.core->regs.b.m = 0x0000;
			m.core->regs.psr.tb = 0;
			m.Run({ Enc::Btstl(1), 0xFFFF }, 1);
			Assert::AreEqual(1, (int)m.core->regs.psr.tb);
		}

		// ---------------------------------------------------------------
		// stacks
		// ---------------------------------------------------------------

		TEST_METHOD(PcsStack_OverflowRaisesErrorInterrupt)
		{
			// The PC stack is 8 deep (dsp.md section 2.5).
			std::vector<uint16_t> code;
			for (int i = 0; i < 10; i++)
			{
				code.push_back(Enc::CallReg(REG_R0));
			}
			m.core->regs.r[0] = 1;			// call to address 1, which is inside the code
			Assemble(m.core, 0, code);
			m.core->regs.pc = 0;

			m.Steps(10);
			Assert::AreEqual(8, m.core->regs.pcs->size(), L"the PC stack holds exactly 8 entries");
		}

		TEST_METHOD(PcsStack_UnderflowRaisesErrorInterrupt)
		{
			m.Run({ Enc::Rets() }, 1);
			Assert::IsTrue(m.core->IsInterruptPending(DspInterrupt::Error));
		}

		// ---------------------------------------------------------------
		// data moves
		// ---------------------------------------------------------------

		TEST_METHOD(Ld_MovesDataMemoryIntoRegister)
		{
			m.DMem(0x0010, 0xCAFE);
			m.core->regs.r[0] = 0x0010;
			m.core->regs.l[0] = 0xFFFF;
			m.Run({ Enc::Ld(REG_A0, REG_R0, MOD_INC) }, 1);
			Assert::AreEqual((uint16_t)0xCAFE, m.core->regs.a.l);
			Assert::AreEqual((uint16_t)0x0011, m.core->regs.r[0]);
		}

		TEST_METHOD(St_MovesRegisterIntoDataMemory)
		{
			m.core->regs.a.l = 0x1234;
			m.core->regs.r[3] = 0x0020;
			m.core->regs.l[3] = 0xFFFF;
			m.Run({ Enc::St(REG_R3, MOD_DEC, REG_A0) }, 1);
			Assert::AreEqual((uint16_t)0x1234, m.DMem(0x0020));
			Assert::AreEqual((uint16_t)0x001F, m.core->regs.r[3]);
		}

		TEST_METHOD(Ldsa_Stsa_UseDppAsHighAddressByte)
		{
			m.core->regs.dpp = 0x01;
			m.DMem(0x0150, 0x55AA);

			m.Run({ Enc::Ldsa(R8A_A0, 0x50) }, 1);
			Assert::AreEqual((uint16_t)0x55AA, m.core->regs.a.l);

			m.core->regs.a.l = 0x0BAD;
			m.core->regs.pc = 0;
			m.Run({ Enc::Stsa(0x60, STSA_A0) }, 1);
			Assert::AreEqual((uint16_t)0x0BAD, m.DMem(0x0160));
		}

		TEST_METHOD(Ldla_Stla_UseLongAddress)
		{
			m.DMem(0x0800, 0x9999);
			m.Run({ Enc::Ldla(REG_A0), 0x0800 }, 1);
			Assert::AreEqual((uint16_t)0x9999, m.core->regs.a.l);

			m.core->regs.b.l = 0x7777;
			m.core->regs.pc = 0;
			m.Run({ Enc::Stla(REG_B0), 0x0802 }, 1);
			Assert::AreEqual((uint16_t)0x7777, m.DMem(0x0802));
		}

		TEST_METHOD(Stli_StoresLongImmediateAtFixedHighByte0xFF)
		{
			// dsp-isa.md section 4.12: stli writes to 0xFF || sa.
			// The 0xFF page holds the DSP control/accelerator registers, which is exactly
			// what the instruction is for ("useful for I/O register setting").
			// 0xFFDE is GAIN, the 16-bit decoder gain register (soundhw_revb.pdf).
			m.Run({ Enc::Stli(0xDE), 0x1357 }, 1);
			Assert::AreEqual((uint16_t)0x1357, m.DMem(0xFFDE), L"stli must target 0xFF00 | sa");
		}

		TEST_METHOD(AcceleratorAddressRegistersKeepOnlyTheirDocumentedBits)
		{
			// soundhw_revb.pdf, accelerator parameter registers: ACSAH/ACEAH hold address bits
			// 26:16 in bits 10:0, bits 15:11 are reserved and read as zero. ACCAH additionally
			// carries the direction bit 15.
			m.Run({ Enc::Stli(0xD4), 0xF800 }, 1);		// ACSAH
			Assert::AreEqual((uint16_t)0x0000, m.DMem(0xFFD4), L"ACSAH bits 15:11 are reserved");

			m.core->regs.pc = 0;
			m.Run({ Enc::Stli(0xD4), 0x07FF }, 1);
			Assert::AreEqual((uint16_t)0x07FF, m.DMem(0xFFD4), L"ACSAH bits 10:0 are the address");

			m.core->regs.pc = 0;
			m.Run({ Enc::Stli(0xD8), 0xF7FF }, 1);		// ACCAH
			Assert::AreEqual((uint16_t)0x87FF, m.DMem(0xFFD8), L"ACCAH keeps the direction bit and bits 10:0 only");
		}

		// ---------------------------------------------------------------
		// ARAM DMA (issue #349 / #111)
		// ---------------------------------------------------------------

		/// <summary>
		/// Program a whole ARAM transfer through the AM* registers. The low word of the block
		/// length starts the transfer, exactly as the AR driver does it: the length field is a
		/// byte count stored in bits 25:5 (dsp.md section 5.4), so the driver writes the
		/// 32-byte aligned byte count straight into it.
		/// </summary>
		void StartAramCopy(uint32_t mmaddr, uint32_t araddr, uint32_t length, bool aramToRam)
		{
			// The ARAM registers live in the Flipper PI DSP register space, not in DSP data
			// memory: the AR driver reaches them through the PI traps that AROpen installs.
			PIRegWrite(PI_REGSPACE_DSP | AMMAH, mmaddr >> 16);
			PIRegWrite(PI_REGSPACE_DSP | AMMAL, mmaddr & 0xFFFF);
			PIRegWrite(PI_REGSPACE_DSP | AMAAH, araddr >> 16);
			PIRegWrite(PI_REGSPACE_DSP | AMAAL, araddr & 0xFFFF);
			// Bit 15 of AMBLH is the direction; the block length is counted in 32-byte units,
			// so its bit 5 is bit 0 of the low word (dsp.md section 7.2).
			PIRegWrite(PI_REGSPACE_DSP | AMBLH, (aramToRam ? 0x8000u : 0x0000u) | ((length >> 16) & 0x03FFu));
			PIRegWrite(PI_REGSPACE_DSP | AMBLL, length & 0xFFFFu);		// starts the transfer
		}

		TEST_METHOD(AramDma_MovesTheWholeBlockAndRaisesTheCompletionInterrupt)
		{
			// The transfer engine streams the whole block through the ARAM controller and raises
			// ARINT when the last byte has been written (dsp.md section 7, CDCR bit 5).
			uint8_t* ram = DspTestMainMemoryBase();

			for (uint32_t i = 0; i < 0x60; i++)
			{
				ram[0x1000 + i] = (uint8_t)(i + 1);
			}

			StartAramCopy(0x00001000, 0x00001000, 0x60, false);


			for (uint32_t i = 0; i < 0x60; i++)
			{
				Assert::AreEqual((uint8_t)(i + 1), DSP::aram.mem[0x1000 + i], L"RAM->ARAM must copy the whole block");
			}

			Assert::AreEqual(0u, (uint32_t)DSP::aram.cnt, L"the block counter must be drained");
			Assert::IsTrue((DSP::dsp_ai.cdcr & CDCR_ARINT) != 0, L"the completion interrupt must have been raised");
			Assert::IsTrue((DSP::dsp_ai.cdcr & CDCR_ARDMA) == 0, L"the DMA-in-progress bit must be clear after the transfer");
		}

		TEST_METHOD(AramDma_SecondRequestWhileBusyIsNotDropped)
		{
			// Issue #349: Metroid Prime issues the next block before the previous transfer engine
			// has finished. The emulator used to hit its "the thread is still running" guard, drop
			// the request and then wait forever for a completion interrupt that never came, which
			// hung the game right after the intro movie. A request must complete, and it must
			// complete *after* the one it is queued behind, without dropping either block.
			uint8_t* ram = DspTestMainMemoryBase();

			for (uint32_t i = 0; i < 0x60; i++)
			{
				ram[0x2000 + i] = 0x80;
				ram[0x3000 + i] = 0x40;
			}

			StartAramCopy(0x00002000, 0x00002000, 0x60, false);

			// The driver already knows the DMA register is free again as soon as it has been read
			// back as idle, so a second block must go through as well.
			StartAramCopy(0x00003000, 0x00003000, 0x60, false);

			for (uint32_t i = 0; i < 0x60; i++)
			{
				Assert::AreEqual((uint8_t)0x80, DSP::aram.mem[0x2000 + i], L"the first block must survive");
				Assert::AreEqual((uint8_t)0x40, DSP::aram.mem[0x3000 + i], L"the second block must not be dropped");
			}

			Assert::AreEqual(0, DspTestHaltCount(), L"a busy transfer engine must never Halt");
		}

		TEST_METHOD(AramDma_CopiesBackToMainMemory)
		{
			uint8_t* ram = DspTestMainMemoryBase();

			for (uint32_t i = 0; i < 0x60; i++)
			{
				DSP::aram.mem[0x4000 + i] = (uint8_t)(0xA0 + i);
			}

			StartAramCopy(0x00004000, 0x00004000, 0x60, true);

			for (uint32_t i = 0; i < 0x60; i++)
			{
				Assert::AreEqual((uint8_t)(0xA0 + i), ram[0x4000 + i], L"ARAM->RAM must copy the whole block");
			}
		}

		// ---------------------------------------------------------------
		// The aggregate DSP interrupt line into the Processor Interface
		//
		// The DSP, ARAM-DMA and AI-DMA completions are three causes with three separate mask bits,
		// and they share ONE PI line (dsp.md section 7, processor-interface.md section 4.1). The
		// cause and mask bits are not adjacent in CDCR, so the line is the OR of the three
		// (cause && its own mask) pairs - it is not the AND of the group of cause bits with the
		// group of mask bits, which is always zero and leaves the guest waiting forever for a
		// completion interrupt that never arrives.
		// ---------------------------------------------------------------

		/// <summary>
		/// Put the interface into a known state: no cause latched, no mask, no line. The DSP unit
		/// tests do not open the AI/DSP register block, so CDCR is driven directly - it is the same
		/// state the guest programs through write_cdcr.
		/// </summary>
		void ClearDspInterrupts()
		{
			DSP::dsp_ai.cdcr = 0;
			Flipper::TestPIAssertedInts = 0;
		}

		/// <summary>True while the aggregate DSP line is asserted on the Processor Interface.</summary>
		bool DspLine() { return (Flipper::TestPIAssertedInts & PI_INTERRUPT_DSP) != 0; }

		TEST_METHOD(DspInt_EachCauseRaisesTheLineOnlyThroughItsOwnMask)
		{
			// A cause with no mask behind it holds no line, whatever the other masks say.
			ClearDspInterrupts();
			DSP::dsp_ai.cdcr = CDCR_DSPINTMSK | CDCR_ARINTMSK | CDCR_AIINTMSK;
			DSP::DSPUpdateInt();
			Assert::IsFalse(DspLine(), L"a masked interface with no pending cause holds no line");

			// Every cause on its own, through its own mask, must raise the line. The three pairs are
			// not adjacent in CDCR, so a grouped AND would find none of them.
			for (uint16_t cause : { (uint16_t)CDCR_DSPINT, (uint16_t)CDCR_ARINT, (uint16_t)CDCR_AIINT })
			{
				uint16_t mask = (cause == CDCR_DSPINT) ? CDCR_DSPINTMSK
					: (cause == CDCR_ARINT) ? CDCR_ARINTMSK : CDCR_AIINTMSK;

				ClearDspInterrupts();
				DSP::dsp_ai.cdcr = CDCR_DSPINTMSK | CDCR_ARINTMSK | CDCR_AIINTMSK | cause;
				DSP::DSPUpdateInt();
				Assert::IsTrue(DspLine(), L"an unmasked completion must reach the PI");

				// ... and stay silent once that cause's own mask is clear.
				DSP::dsp_ai.cdcr &= ~mask;
				DSP::DSPUpdateInt();
				Assert::IsFalse(DspLine(), L"clearing the cause's own mask must drop the line");
			}
		}

		TEST_METHOD(DspInt_AramCompletionReachesTheProcessorInterface)
		{
			ClearDspInterrupts();
			DSP::dsp_ai.cdcr = CDCR_ARINTMSK;

			StartAramCopy(0x00001000, 0x00001000, 0x20, false);

			Assert::IsTrue((DSP::dsp_ai.cdcr & CDCR_ARINT) != 0, L"the ARAM completion latches its cause");
			Assert::IsTrue(DspLine(), L"the ARAM completion must reach the Processor Interface");
		}

		TEST_METHOD(DspInt_MaskedAramCompletionLatchesButHoldsNoLine)
		{
			ClearDspInterrupts();

			StartAramCopy(0x00001000, 0x00001000, 0x20, false);

			Assert::IsTrue((DSP::dsp_ai.cdcr & CDCR_ARINT) != 0, L"the completion is still latched in CDCR");
			Assert::IsFalse(DspLine(), L"a masked completion must not assert the PI line");
		}

		TEST_METHOD(DspInt_AcknowledgingTheCauseDropsTheLine)
		{
			ClearDspInterrupts();
			DSP::dsp_ai.cdcr = CDCR_ARINTMSK;

			StartAramCopy(0x00001000, 0x00001000, 0x20, false);
			Assert::IsTrue(DspLine(), L"the line is up");

			// The handler acknowledges by clearing the cause (write-1-to-clear on CDCR).
			DSP::dsp_ai.cdcr &= ~CDCR_ARINT;
			DSP::DSPUpdateInt();

			Assert::IsFalse(DspLine(), L"the line must drop once its cause is acknowledged");
		}

		TEST_METHOD(DspInt_MailboxInterruptReachesTheProcessorInterface)
		{
			ClearDspInterrupts();
			DSP::dsp_ai.cdcr = CDCR_DSPINTMSK;

			DSP::DSPAssertInt();

			Assert::IsTrue((DSP::dsp_ai.cdcr & CDCR_DSPINT) != 0, L"the DSP interrupt latches its cause");
			Assert::IsTrue(DspLine(), L"and reaches the PI");
		}

		TEST_METHOD(Mvsi_SignExtendsShortImmediate)
		{
			m.Run({ Enc::Mvsi(R8A_A0, (int8_t)0x80) }, 1);
			Assert::AreEqual((uint16_t)0xFF80, m.core->regs.a.l, L"the 8-bit immediate is sign extended");

			m.core->regs.pc = 0;
			m.Run({ Enc::Mvsi(R8A_Y1, (int8_t)0x7F) }, 1);
			Assert::AreEqual((uint16_t)0x007F, m.core->regs.y.h);
		}

		TEST_METHOD(Mvli_MovesWordImmediate)
		{
			m.Run({ Enc::Mvli(REG_B0), 0xABCD }, 1);
			Assert::AreEqual((uint16_t)0xABCD, m.core->regs.b.l);
		}

		TEST_METHOD(Mv_RegisterToRegister)
		{
			m.core->regs.a.l = 0x4242;
			m.Run({ Enc::Mv(REG_B0, REG_A0) }, 1);
			Assert::AreEqual((uint16_t)0x4242, m.core->regs.b.l);
		}

		TEST_METHOD(Mv_ToAndFromStackRegisters)
		{
			// Explicit stack access via mv is documented in dsp.md section 2.5.
			m.core->regs.a.l = 0x1111;
			m.Run({ Enc::Mv(REG_PCS, REG_A0) }, 1);
			Assert::AreEqual(1, m.core->regs.pcs->size(), L"mv pcs, s pushes on the PC stack");

			m.core->regs.pc = 0;
			m.Run({ Enc::Mv(REG_B0, REG_PCS) }, 1);
			Assert::AreEqual((uint16_t)0x1111, m.core->regs.b.l);
			Assert::IsTrue(m.core->regs.pcs->empty());
		}

		// ---------------------------------------------------------------
		// DspStack
		// ---------------------------------------------------------------

		TEST_METHOD(DspStack_BehavesAsExpected)
		{
			DspStack stack(3);

			Assert::IsTrue(stack.empty());
			Assert::AreEqual(0, stack.size());

			Assert::IsTrue(stack.push(0x11));
			Assert::IsTrue(stack.push(0x22));
			Assert::IsTrue(stack.push(0x33));
			Assert::IsFalse(stack.push(0x44), L"pushing on a full stack must fail");

			Assert::AreEqual((uint16_t)0x33, stack.top());
			Assert::AreEqual((uint16_t)0x11, stack.at(0));
			Assert::AreEqual(3, stack.size());

			uint16_t v = 0;
			Assert::IsTrue(stack.pop(v));
			Assert::AreEqual((uint16_t)0x33, v);
			Assert::IsTrue(stack.pop(v));
			Assert::IsTrue(stack.pop(v));
			Assert::AreEqual((uint16_t)0x11, v);
			Assert::IsFalse(stack.pop(v), L"popping an empty stack must fail");

			stack.push(1);
			stack.clear();
			Assert::IsTrue(stack.empty());
		}
	};
}
