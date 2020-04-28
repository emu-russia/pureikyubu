// addcx

// Carry goes to XER[CA]

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

		TEST_METHOD(Addc)
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
			bf << Bits(10, 9);		// Secondary opcode
			bf << Bits(0, 1);	// Rc

			Gekko::AnalyzeInfo info = { 0 };
			Gekko::Analyzer::Analyze(pc, bf.GetBits32(), &info);
			std::string text = Gekko::GekkoDisasm::Disasm(pc, &info);

			Logger::WriteMessage(text.c_str());

			// Interpreter

			Gekko::Gekko->regs.gpr[1] = 0xff;
			Gekko::Gekko->regs.gpr[2] = 1;
			Gekko::Gekko->regs.gpr[3] = 2;

			Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] &= ~GEKKO_XER_CA;

			Gekko::Gekko->ExecuteOpcodeDebug(pc, bf.GetBits32());

			Assert::IsTrue(Gekko::Gekko->regs.gpr[1] == 3);
			Assert::IsTrue((Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] & GEKKO_XER_CA) == 0);

			// Carry

			Gekko::Gekko->regs.gpr[1] = 0xff;
			Gekko::Gekko->regs.gpr[2] = 0xffff'ffff;
			Gekko::Gekko->regs.gpr[3] = 2;

			Gekko::Gekko->ExecuteOpcodeDebug(pc, bf.GetBits32());

			Assert::IsTrue(Gekko::Gekko->regs.gpr[1] == 1);
			Assert::IsTrue((Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] & GEKKO_XER_CA) != 0);

			delete Gekko::Gekko;
		}

		TEST_METHOD(Addcd)
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
			bf << Bits(10, 9);		// Secondary opcode
			bf << Bits(1, 1);	// Rc

			Gekko::AnalyzeInfo info = { 0 };
			Gekko::Analyzer::Analyze(pc, bf.GetBits32(), &info);
			std::string text = Gekko::GekkoDisasm::Disasm(pc, &info);

			Logger::WriteMessage(text.c_str());

			// Interpreter

			// Result < 0, no overflow

			Gekko::Gekko->regs.cr &= ~0x0fff'ffff;			// Clear CR0
			Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] &= ~GEKKO_XER_SO;
			Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] &= ~GEKKO_XER_OV;
			Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] &= ~GEKKO_XER_CA;

			Gekko::Gekko->regs.gpr[1] = 0xff;
			Gekko::Gekko->regs.gpr[2] = -5;
			Gekko::Gekko->regs.gpr[3] = -6;			// Carry!

			Gekko::Gekko->ExecuteOpcodeDebug(pc, bf.GetBits32());

			Assert::IsTrue(Gekko::Gekko->regs.gpr[1] == -11);
			Assert::IsTrue((Gekko::Gekko->regs.cr & GEKKO_CR0_LT) != 0);
			Assert::IsTrue((Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] & GEKKO_XER_SO) == 0);
			Assert::IsTrue((Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] & GEKKO_XER_OV) == 0);
			Assert::IsTrue((Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] & GEKKO_XER_CA) != 0);

			// Result > 0, no overflow

			Gekko::Gekko->regs.cr &= ~0x0fff'ffff;			// Clear CR0
			Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] &= ~GEKKO_XER_SO;
			Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] &= ~GEKKO_XER_OV;
			Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] &= ~GEKKO_XER_CA;

			Gekko::Gekko->regs.gpr[1] = 0xff;
			Gekko::Gekko->regs.gpr[2] = 1;
			Gekko::Gekko->regs.gpr[3] = 2;		// No Carry

			Gekko::Gekko->ExecuteOpcodeDebug(pc, bf.GetBits32());

			Assert::IsTrue(Gekko::Gekko->regs.gpr[1] == 3);
			Assert::IsTrue((Gekko::Gekko->regs.cr & GEKKO_CR0_GT) != 0);
			Assert::IsTrue((Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] & GEKKO_XER_SO) == 0);
			Assert::IsTrue((Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] & GEKKO_XER_OV) == 0);
			Assert::IsTrue((Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] & GEKKO_XER_CA) == 0);

			// Result == 0, no overflow

			Gekko::Gekko->regs.cr &= ~0x0fff'ffff;			// Clear CR0
			Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] &= ~GEKKO_XER_SO;
			Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] &= ~GEKKO_XER_OV;
			Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] &= ~GEKKO_XER_CA;

			Gekko::Gekko->regs.gpr[1] = 0xff;
			Gekko::Gekko->regs.gpr[2] = 0;
			Gekko::Gekko->regs.gpr[3] = 0;		// No Carry

			Gekko::Gekko->ExecuteOpcodeDebug(pc, bf.GetBits32());

			Assert::IsTrue(Gekko::Gekko->regs.gpr[1] == 0);
			Assert::IsTrue((Gekko::Gekko->regs.cr & GEKKO_CR0_EQ) != 0);
			Assert::IsTrue((Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] & GEKKO_XER_SO) == 0);
			Assert::IsTrue((Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] & GEKKO_XER_OV) == 0);
			Assert::IsTrue((Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] & GEKKO_XER_CA) == 0);

			// Result Overflow
			// Overflow is copied from XER[SO].
			// + Carry

			Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] &= ~GEKKO_XER_CA;

			Gekko::Gekko->regs.gpr[1] = 0xff;
			Gekko::Gekko->regs.gpr[2] = 0x7fff'ffff;
			Gekko::Gekko->regs.gpr[3] = 0x7fff'ffff;		// No Carry!

			Gekko::Gekko->ExecuteOpcodeDebug(pc, bf.GetBits32());

			Assert::IsTrue(Gekko::Gekko->regs.gpr[1] == 0xFFFFFFFE);
			Assert::IsTrue((Gekko::Gekko->regs.cr & GEKKO_CR0_SO) == 0);
			Assert::IsTrue((Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] & GEKKO_XER_SO) == 0);
			Assert::IsTrue((Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] & GEKKO_XER_OV) == 0);
			Assert::IsTrue((Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] & GEKKO_XER_CA) == 0);

			// XER[SO] = 1
			Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] &= ~GEKKO_XER_CA;
			Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] |= GEKKO_XER_SO;		// Just copied

			Gekko::Gekko->regs.gpr[1] = 0xff;
			Gekko::Gekko->regs.gpr[2] = 0x7fff'ffff;
			Gekko::Gekko->regs.gpr[3] = 0xffff'ffff;		// Carry

			Gekko::Gekko->ExecuteOpcodeDebug(pc, bf.GetBits32());

			Assert::IsTrue(Gekko::Gekko->regs.gpr[1] == 0x7FFFFFFE);
			Assert::IsTrue((Gekko::Gekko->regs.cr & GEKKO_CR0_SO) != 0);
			Assert::IsTrue((Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] & GEKKO_XER_SO) != 0);
			Assert::IsTrue((Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] & GEKKO_XER_OV) == 0);
			Assert::IsTrue((Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] & GEKKO_XER_CA) != 0);

			delete Gekko::Gekko;
		}

		TEST_METHOD(Addco)
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
			bf << Bits(10, 9);		// Secondary opcode
			bf << Bits(0, 1);	// Rc

			Gekko::AnalyzeInfo info = { 0 };
			Gekko::Analyzer::Analyze(pc, bf.GetBits32(), &info);
			std::string text = Gekko::GekkoDisasm::Disasm(pc, &info);

			Logger::WriteMessage(text.c_str());

			// Interpreter

			Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] &= ~GEKKO_XER_SO;
			Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] &= ~GEKKO_XER_OV;
			Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] &= ~GEKKO_XER_CA;

			Gekko::Gekko->regs.gpr[1] = 0xff;
			Gekko::Gekko->regs.gpr[2] = 1;
			Gekko::Gekko->regs.gpr[3] = 2;

			Gekko::Gekko->ExecuteOpcodeDebug(pc, bf.GetBits32());

			Assert::IsTrue(Gekko::Gekko->regs.gpr[1] == 3);
			Assert::IsTrue((Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] & GEKKO_XER_SO) == 0);
			Assert::IsTrue((Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] & GEKKO_XER_OV) == 0);
			Assert::IsTrue((Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] & GEKKO_XER_CA) == 0);

			Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] &= ~GEKKO_XER_CA;

			Gekko::Gekko->regs.gpr[1] = 0xff;
			Gekko::Gekko->regs.gpr[2] = 0x7fff'ffff;
			Gekko::Gekko->regs.gpr[3] = 0x7fff'ffff;

			Gekko::Gekko->ExecuteOpcodeDebug(pc, bf.GetBits32());

			Assert::IsTrue(Gekko::Gekko->regs.gpr[1] == 0xFFFFFFFE);
			Assert::IsTrue((Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] & GEKKO_XER_SO) != 0);
			Assert::IsTrue((Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] & GEKKO_XER_OV) != 0);
			Assert::IsTrue((Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] & GEKKO_XER_CA) == 0);

			delete Gekko::Gekko;
		}

		TEST_METHOD(Addcod)
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
			bf << Bits(10, 9);		// Secondary opcode
			bf << Bits(1, 1);	// Rc

			Gekko::AnalyzeInfo info = { 0 };
			Gekko::Analyzer::Analyze(pc, bf.GetBits32(), &info);
			std::string text = Gekko::GekkoDisasm::Disasm(pc, &info);

			Logger::WriteMessage(text.c_str());

			// Interpreter

			// Result < 0, no overflow

			Gekko::Gekko->regs.cr &= ~0x0fff'ffff;			// Clear CR0
			Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] &= ~GEKKO_XER_SO;
			Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] &= ~GEKKO_XER_OV;
			Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] &= ~GEKKO_XER_CA;

			Gekko::Gekko->regs.gpr[1] = 0xff;
			Gekko::Gekko->regs.gpr[2] = -5;
			Gekko::Gekko->regs.gpr[3] = -6;			// No Overflow, Carry

			Gekko::Gekko->ExecuteOpcodeDebug(pc, bf.GetBits32());

			Assert::IsTrue(Gekko::Gekko->regs.gpr[1] == -11);
			Assert::IsTrue((Gekko::Gekko->regs.cr & GEKKO_CR0_LT) != 0);
			Assert::IsTrue((Gekko::Gekko->regs.cr & GEKKO_CR0_SO) == 0);
			Assert::IsTrue((Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] & GEKKO_XER_SO) == 0);
			Assert::IsTrue((Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] & GEKKO_XER_OV) == 0);
			Assert::IsTrue((Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] & GEKKO_XER_CA) != 0);

			// Result > 0, no overflow

			Gekko::Gekko->regs.cr &= ~0x0fff'ffff;			// Clear CR0
			Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] &= ~GEKKO_XER_SO;
			Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] &= ~GEKKO_XER_OV;
			Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] &= ~GEKKO_XER_CA;

			Gekko::Gekko->regs.gpr[1] = 0xff;
			Gekko::Gekko->regs.gpr[2] = 1;
			Gekko::Gekko->regs.gpr[3] = 2;		// No Carry

			Gekko::Gekko->ExecuteOpcodeDebug(pc, bf.GetBits32());

			Assert::IsTrue(Gekko::Gekko->regs.gpr[1] == 3);
			Assert::IsTrue((Gekko::Gekko->regs.cr & GEKKO_CR0_GT) != 0);
			Assert::IsTrue((Gekko::Gekko->regs.cr & GEKKO_CR0_SO) == 0);
			Assert::IsTrue((Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] & GEKKO_XER_SO) == 0);
			Assert::IsTrue((Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] & GEKKO_XER_OV) == 0);
			Assert::IsTrue((Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] & GEKKO_XER_CA) == 0);

			// Result == 0, no overflow

			Gekko::Gekko->regs.cr &= ~0x0fff'ffff;			// Clear CR0
			Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] &= ~GEKKO_XER_SO;
			Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] &= ~GEKKO_XER_OV;
			Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] &= ~GEKKO_XER_CA;

			Gekko::Gekko->regs.gpr[1] = 0xff;
			Gekko::Gekko->regs.gpr[2] = 0;
			Gekko::Gekko->regs.gpr[3] = 0;		// No Carry

			Gekko::Gekko->ExecuteOpcodeDebug(pc, bf.GetBits32());

			Assert::IsTrue(Gekko::Gekko->regs.gpr[1] == 0);
			Assert::IsTrue((Gekko::Gekko->regs.cr & GEKKO_CR0_EQ) != 0);
			Assert::IsTrue((Gekko::Gekko->regs.cr & GEKKO_CR0_SO) == 0);
			Assert::IsTrue((Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] & GEKKO_XER_SO) == 0);
			Assert::IsTrue((Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] & GEKKO_XER_OV) == 0);
			Assert::IsTrue((Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] & GEKKO_XER_CA) == 0);

			// Result Overflow
			// + Carry

			Gekko::Gekko->regs.cr &= ~0x0fff'ffff;			// Clear CR0
			Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] &= ~GEKKO_XER_SO;
			Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] &= ~GEKKO_XER_OV;
			Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] &= ~GEKKO_XER_CA;

			Gekko::Gekko->regs.gpr[1] = 0xff;
			Gekko::Gekko->regs.gpr[2] = 0x7fff'ffff;
			Gekko::Gekko->regs.gpr[3] = 0x7fff'ffff;		// Overflow, No Carry

			Gekko::Gekko->ExecuteOpcodeDebug(pc, bf.GetBits32());

			Assert::IsTrue(Gekko::Gekko->regs.gpr[1] == 0xFFFFFFFE);
			Assert::IsTrue((Gekko::Gekko->regs.cr & GEKKO_CR0_SO) != 0);
			Assert::IsTrue((Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] & GEKKO_XER_SO) != 0);
			Assert::IsTrue((Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] & GEKKO_XER_OV) != 0);
			Assert::IsTrue((Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] & GEKKO_XER_CA) == 0);

			Gekko::Gekko->regs.cr &= ~0x0fff'ffff;			// Clear CR0
			Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] &= ~GEKKO_XER_SO;
			Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] &= ~GEKKO_XER_OV;
			Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] &= ~GEKKO_XER_CA;

			Gekko::Gekko->regs.gpr[1] = 0xff;
			Gekko::Gekko->regs.gpr[2] = 0x7fff'ffff;
			Gekko::Gekko->regs.gpr[3] = 0xffff'ffff;		// No Overflow, Carry

			Gekko::Gekko->ExecuteOpcodeDebug(pc, bf.GetBits32());

			Assert::IsTrue(Gekko::Gekko->regs.gpr[1] == 0x7FFFFFFE);
			Assert::IsTrue((Gekko::Gekko->regs.cr & GEKKO_CR0_SO) == 0);
			Assert::IsTrue((Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] & GEKKO_XER_SO) == 0);
			Assert::IsTrue((Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] & GEKKO_XER_OV) == 0);
			Assert::IsTrue((Gekko::Gekko->regs.spr[(int)Gekko::SPR::XER] & GEKKO_XER_CA) != 0);

			delete Gekko::Gekko;
		}

	};
}
