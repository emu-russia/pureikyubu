// addx

//A human need only remember that, when doing signed math, adding
//two numbers of the same sign must produce a result of the same sign,
//otherwise overflow happened.

#include "../pch.h"
#include "CppUnitTest.h"
#include "BitFactory.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GekkoCoreUnitTest
{
	TEST_CLASS(GekkoIsaUnitTest)
	{
	public:

		TEST_METHOD(Add)
		{
			Gekko::Gekko = new Gekko::GekkoCore();
			uint32_t pc = 0x8000'0000;

			// Disasm

			BitFactory bf;

			bf << Bits(31, 6);	// Primary opcode
			bf << Bits(1, 5);
			bf << Bits(2, 5);
			bf << Bits(3, 5);
			bf << Bits(0, 1);	// OE
			bf << Bits(266, 9);		// Secondary opcode
			bf << Bits(0, 1);	// Rc

			Gekko::AnalyzeInfo info = { 0 };
			Gekko::Analyzer::Analyze(pc, bf.GetBits32(), &info);
			std::string text = Gekko::GekkoDisasm::Disasm(pc, &info, true, true);

			Logger::WriteMessage(text.c_str());

			// Interpreter

			Gekko::Gekko->regs.gpr[1] = 0xff;
			Gekko::Gekko->regs.gpr[2] = 0x12345678;
			Gekko::Gekko->regs.gpr[3] = 0xABCDEF12;

			Gekko::Gekko->ExecuteOpcodeDebug(pc, bf.GetBits32());

			Assert::IsTrue(Gekko::Gekko->regs.gpr[1] == 0xBE02458A);

			delete Gekko::Gekko;
		}

		TEST_METHOD(Addd)
		{
			Gekko::Gekko = new Gekko::GekkoCore();
			uint32_t pc = 0x8000'0000;

			// Disasm

			BitFactory bf;

			bf << Bits(31, 6);	// Primary opcode
			bf << Bits(1, 5);
			bf << Bits(2, 5);
			bf << Bits(3, 5);
			bf << Bits(0, 1);	// OE
			bf << Bits(266, 9);		// Secondary opcode
			bf << Bits(1, 1);	// Rc

			Gekko::AnalyzeInfo info = { 0 };
			Gekko::Analyzer::Analyze(pc, bf.GetBits32(), &info);
			std::string text = Gekko::GekkoDisasm::Disasm(pc, &info, true, true);

			Logger::WriteMessage(text.c_str());

			// Interpreter

			// Result < 0, no overflow

			Gekko::Gekko->regs.cr &= ~0x0fff'ffff;			// Clear CR0
			Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] &= ~GEKKO_XER_SO;
			Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] &= ~GEKKO_XER_OV;

			Gekko::Gekko->regs.gpr[1] = 0xff;
			Gekko::Gekko->regs.gpr[2] = -5;
			Gekko::Gekko->regs.gpr[3] = -6;

			Gekko::Gekko->ExecuteOpcodeDebug(pc, bf.GetBits32());

			Assert::IsTrue(Gekko::Gekko->regs.gpr[1] == -11);
			Assert::IsTrue((Gekko::Gekko->regs.cr & GEKKO_CR0_LT) != 0);
			Assert::IsTrue((Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] & GEKKO_XER_SO) == 0);
			Assert::IsTrue((Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] & GEKKO_XER_OV) == 0);

			// Result > 0, no overflow

			Gekko::Gekko->regs.cr &= ~0x0fff'ffff;			// Clear CR0
			Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] &= ~GEKKO_XER_SO;
			Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] &= ~GEKKO_XER_OV;

			Gekko::Gekko->regs.gpr[1] = 0xff;
			Gekko::Gekko->regs.gpr[2] = 1;
			Gekko::Gekko->regs.gpr[3] = 2;

			Gekko::Gekko->ExecuteOpcodeDebug(pc, bf.GetBits32());

			Assert::IsTrue(Gekko::Gekko->regs.gpr[1] == 3);
			Assert::IsTrue((Gekko::Gekko->regs.cr & GEKKO_CR0_GT) != 0);
			Assert::IsTrue((Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] & GEKKO_XER_SO) == 0);
			Assert::IsTrue((Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] & GEKKO_XER_OV) == 0);

			// Result == 0, no overflow

			Gekko::Gekko->regs.cr &= ~0x0fff'ffff;			// Clear CR0
			Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] &= ~GEKKO_XER_SO;
			Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] &= ~GEKKO_XER_OV;

			Gekko::Gekko->regs.gpr[1] = 0xff;
			Gekko::Gekko->regs.gpr[2] = 0;
			Gekko::Gekko->regs.gpr[3] = 0;

			Gekko::Gekko->ExecuteOpcodeDebug(pc, bf.GetBits32());

			Assert::IsTrue(Gekko::Gekko->regs.gpr[1] == 0);
			Assert::IsTrue((Gekko::Gekko->regs.cr & GEKKO_CR0_EQ) != 0);
			Assert::IsTrue((Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] & GEKKO_XER_SO) == 0);
			Assert::IsTrue((Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] & GEKKO_XER_OV) == 0);

			// Result Overflow
			// Overflow is copied from XER[SO].

			Gekko::Gekko->regs.gpr[1] = 0xff;
			Gekko::Gekko->regs.gpr[2] = 0x7fff'ffff;
			Gekko::Gekko->regs.gpr[3] = 0x7fff'ffff;

			Gekko::Gekko->ExecuteOpcodeDebug(pc, bf.GetBits32());

			Assert::IsTrue(Gekko::Gekko->regs.gpr[1] == 0xFFFFFFFE);
			Assert::IsTrue((Gekko::Gekko->regs.cr & GEKKO_CR0_SO) == 0);
			Assert::IsTrue((Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] & GEKKO_XER_SO) == 0);
			Assert::IsTrue((Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] & GEKKO_XER_OV) == 0);

			// XER[SO] = 1
			Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] |= GEKKO_XER_SO;

			Gekko::Gekko->regs.gpr[1] = 0xff;
			Gekko::Gekko->regs.gpr[2] = 0x7fff'ffff;
			Gekko::Gekko->regs.gpr[3] = 0x7fff'ffff;

			Gekko::Gekko->ExecuteOpcodeDebug(pc, bf.GetBits32());

			Assert::IsTrue(Gekko::Gekko->regs.gpr[1] == 0xFFFFFFFE);
			Assert::IsTrue((Gekko::Gekko->regs.cr & GEKKO_CR0_SO) != 0);
			Assert::IsTrue((Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] & GEKKO_XER_SO) != 0);
			Assert::IsTrue((Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] & GEKKO_XER_OV) == 0);

			delete Gekko::Gekko;
		}

		TEST_METHOD(Addo)
		{
			Gekko::Gekko = new Gekko::GekkoCore();
			uint32_t pc = 0x8000'0000;

			// Disasm

			BitFactory bf;

			bf << Bits(31, 6);	// Primary opcode
			bf << Bits(1, 5);
			bf << Bits(2, 5);
			bf << Bits(3, 5);
			bf << Bits(1, 1);	// OE
			bf << Bits(266, 9);		// Secondary opcode
			bf << Bits(0, 1);	// Rc

			Gekko::AnalyzeInfo info = { 0 };
			Gekko::Analyzer::Analyze(pc, bf.GetBits32(), &info);
			std::string text = Gekko::GekkoDisasm::Disasm(pc, &info, true, true);

			Logger::WriteMessage(text.c_str());

			// Interpreter

			Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] &= ~GEKKO_XER_SO;
			Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] &= ~GEKKO_XER_OV;

			Gekko::Gekko->regs.gpr[1] = 0xff;
			Gekko::Gekko->regs.gpr[2] = 1;
			Gekko::Gekko->regs.gpr[3] = 2;

			Gekko::Gekko->ExecuteOpcodeDebug(pc, bf.GetBits32());

			Assert::IsTrue(Gekko::Gekko->regs.gpr[1] == 3);
			Assert::IsTrue((Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] & GEKKO_XER_SO) == 0);
			Assert::IsTrue((Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] & GEKKO_XER_OV) == 0);

			Gekko::Gekko->regs.gpr[1] = 0xff;
			Gekko::Gekko->regs.gpr[2] = 0x7fff'ffff;
			Gekko::Gekko->regs.gpr[3] = 0x7fff'ffff;

			Gekko::Gekko->ExecuteOpcodeDebug(pc, bf.GetBits32());

			Assert::IsTrue(Gekko::Gekko->regs.gpr[1] == 0xFFFFFFFE);
			Assert::IsTrue((Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] & GEKKO_XER_SO) != 0);
			Assert::IsTrue((Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] & GEKKO_XER_OV) != 0);

			delete Gekko::Gekko;
		}

		TEST_METHOD(Addod)
		{
			Gekko::Gekko = new Gekko::GekkoCore();
			uint32_t pc = 0x8000'0000;

			// Disasm

			BitFactory bf;

			bf << Bits(31, 6);	// Primary opcode
			bf << Bits(1, 5);
			bf << Bits(2, 5);
			bf << Bits(3, 5);
			bf << Bits(1, 1);	// OE
			bf << Bits(266, 9);		// Secondary opcode
			bf << Bits(1, 1);	// Rc

			Gekko::AnalyzeInfo info = { 0 };
			Gekko::Analyzer::Analyze(pc, bf.GetBits32(), &info);
			std::string text = Gekko::GekkoDisasm::Disasm(pc, &info, true, true);

			Logger::WriteMessage(text.c_str());

			// Interpreter

			Gekko::Gekko->regs.cr &= ~0x0fff'ffff;			// Clear CR0
			Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] &= ~GEKKO_XER_SO;
			Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] &= ~GEKKO_XER_OV;

			Gekko::Gekko->regs.gpr[1] = 0xff;
			Gekko::Gekko->regs.gpr[2] = 1;
			Gekko::Gekko->regs.gpr[3] = 2;

			Gekko::Gekko->ExecuteOpcodeDebug(pc, bf.GetBits32());

			Assert::IsTrue(Gekko::Gekko->regs.gpr[1] == 3);
			Assert::IsTrue((Gekko::Gekko->regs.cr & GEKKO_CR0_SO) == 0);
			Assert::IsTrue((Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] & GEKKO_XER_SO) == 0);
			Assert::IsTrue((Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] & GEKKO_XER_OV) == 0);

			Gekko::Gekko->regs.gpr[1] = 0xff;
			Gekko::Gekko->regs.gpr[2] = 0x7fff'ffff;
			Gekko::Gekko->regs.gpr[3] = 0x7fff'ffff;

			Gekko::Gekko->ExecuteOpcodeDebug(pc, bf.GetBits32());

			Assert::IsTrue(Gekko::Gekko->regs.gpr[1] == 0xFFFFFFFE);
			Assert::IsTrue((Gekko::Gekko->regs.cr & GEKKO_CR0_SO) != 0);
			Assert::IsTrue((Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] & GEKKO_XER_SO) != 0);
			Assert::IsTrue((Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] & GEKKO_XER_OV) != 0);

			delete Gekko::Gekko;
		}

	};
}
