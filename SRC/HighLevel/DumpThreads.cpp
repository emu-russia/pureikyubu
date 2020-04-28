// Dump DolphinOS threads.

// Details on the DolphinOS threads can be found in \Docs\RE\thread.txt. Fairly simple and clean design.
// We catch on to the list of active threads (__OSLinkActive) and display them in turn. You can use the DumpContext command to display context.

#include "pch.h"

namespace HLE
{

	static bool LoadOsThread(uint32_t ea, OSThread* thread)
	{
		// Translate effective address

		uint32_t threadPa = Gekko::Gekko->EffectiveToPhysical(ea, Gekko::MmuAccess::Read);
		if (threadPa == Gekko::BadAddress)
		{
			DBReport("Invalid thread effective address: 0x%08X\n", ea);
			return false;
		}

		uint8_t* ptr = MITranslatePhysicalAddress(threadPa, sizeof(OSThread));

		if (ptr == nullptr)
		{
			DBReport("Invalid thread physical address: 0x%08X\n", threadPa);
			return false;
		}

		// Load thread struct and swap values

		*thread = *(OSThread*)ptr;

		thread->state = _byteswap_ushort(thread->state);
		thread->attr = _byteswap_ushort(thread->attr);
		thread->suspend = _byteswap_ulong(thread->suspend);
		thread->priority = _byteswap_ulong(thread->priority);
		thread->base = _byteswap_ulong(thread->base);
		thread->val = _byteswap_ulong(thread->val);

		thread->linkActive.next = _byteswap_ulong(thread->linkActive.next);
		thread->linkActive.prev = _byteswap_ulong(thread->linkActive.prev);

		thread->stackBase = _byteswap_ulong(thread->stackBase);
		thread->stackEnd = _byteswap_ulong(thread->stackEnd);

		// No need for other properties

		return true;
	}

	static void DumpOsThread(size_t count, OSThread* thread, uint32_t threadEa)
	{
		DBReport("Thread %zi, context: 0x%08X:\n", count, threadEa);
		DBReport("state: 0x%04X, attr: 0x%04X\n", thread->state, thread->attr);
		DBReport("suspend: %i, priority: 0x%08X, base: 0x%08X, val: 0x%08X\n", (int)thread->suspend, thread->priority, thread->base, thread->val);
	}

	Json::Value* DumpDolphinOsThreads(bool displayOnScreen)
	{
		// Get pointer to __OSLinkActive

		uint32_t linkActiveEffectiveAddr = OS_LINK_ACTIVE;

		uint32_t linkActivePa = Gekko::Gekko->EffectiveToPhysical(linkActiveEffectiveAddr, Gekko::MmuAccess::Read);
		if (linkActivePa == Gekko::BadAddress)
		{
			if (displayOnScreen)
			{
				DBReport("Invalid active threads link effective address: 0x%08X\n", linkActiveEffectiveAddr);
			}
			return nullptr;
		}

		uint8_t* ptr = MITranslatePhysicalAddress(linkActivePa, sizeof(OSThreadLink));

		if (ptr == nullptr)
		{
			if (displayOnScreen)
			{
				DBReport("Invalid active threads link physical address: 0x%08X\n", linkActivePa);
			}
			return nullptr;
		}

		OSThreadLink linkActive = *(OSThreadLink*)ptr;

		linkActive.next = _byteswap_ulong(linkActive.next);
		linkActive.prev = _byteswap_ulong(linkActive.prev);

		// Walk active threads

		Json::Value* output = new Json::Value();
		output->type = Json::ValueType::Array;

		if (displayOnScreen)
		{
			DBReport("Dumping active DolphinOS threads:\n\n");
		}

		size_t activeThreadsCount = 0;
		uint32_t threadEa = linkActive.next;

		while (threadEa != 0)
		{
			OSThread thread = { 0 };

			if (!LoadOsThread(threadEa, &thread))
				break;

			if (displayOnScreen)
			{
				DumpOsThread(activeThreadsCount, &thread, threadEa);
			}

			threadEa = thread.linkActive.next;
			activeThreadsCount++;

			output->AddUInt32(nullptr, threadEa);

			if (displayOnScreen)
			{
				DBReport("\n");
			}
		}

		if (displayOnScreen)
		{
			DBReport("Active threads: %zi. Use DumpContext command to dump thread context.\n", activeThreadsCount);
		}

		return output;
	}

	Json::Value* DumpDolphinOsContext(uint32_t effectiveAddr, bool displayOnScreen)
	{
		// Get context pointer

		uint32_t physAddr = Gekko::Gekko->EffectiveToPhysical(effectiveAddr, Gekko::MmuAccess::Read);

		if (physAddr == Gekko::BadAddress)
		{
			if (displayOnScreen)
			{
				DBReport("Invalid context effective address: 0x%08X\n", effectiveAddr);
			}
			return nullptr;
		}

		uint8_t* ptr = MITranslatePhysicalAddress(physAddr, sizeof(OSContext));

		if (ptr == nullptr)
		{
			if (displayOnScreen)
			{
				DBReport("Invalid context physical address: 0x%08X\n", physAddr);
			}
			return nullptr;
		}

		// Copyout context and swap values

		OSContext context = *(OSContext*)ptr;

		for (int i = 0; i < 32; i++)
		{
			context.gpr[i] = _byteswap_ulong(context.gpr[i]);
			context.fprAsUint[i] = _byteswap_uint64(context.fprAsUint[i]);
			context.psrAsUint[i] = _byteswap_uint64(context.psrAsUint[i]);
		}

		for (int i = 0; i < 8; i++)
		{
			context.gqr[i] = _byteswap_ulong(context.gqr[i]);
		}

		context.cr = _byteswap_ulong(context.cr);
		context.lr = _byteswap_ulong(context.lr);
		context.ctr = _byteswap_ulong(context.ctr);
		context.xer = _byteswap_ulong(context.xer);

		context.fpscr = _byteswap_ulong(context.fpscr);

		context.srr[0] = _byteswap_ulong(context.srr[0]);
		context.srr[1] = _byteswap_ulong(context.srr[1]);

		context.mode = _byteswap_ushort(context.mode);
		context.state = _byteswap_ushort(context.state);

		// Dump contents

		if (displayOnScreen)
		{
			for (int i = 0; i < 32; i++)
			{
				DBReport("gpr[%i] = 0x%08X\n", i, context.gpr[i]);
			}

			for (int i = 0; i < 32; i++)
			{
				DBReport("fpr[%i] = %f (0x%llx)\n", i, context.fpr[i], context.fprAsUint[i]);
			}

			for (int i = 0; i < 32; i++)
			{
				DBReport("psr[%i] = %f (0x%llx)\n", i, context.psr[i], context.psrAsUint[i]);
			}

			for (int i = 0; i < 8; i++)
			{
				DBReport("gqr[%i] = 0x%08X\n", i, context.gqr[i]);
			}

			DBReport("cr = 0x%08X\n", context.cr);
			DBReport("lr = 0x%08X\n", context.lr);
			DBReport("ctr = 0x%08X\n", context.ctr);
			DBReport("xer = 0x%08X\n", context.xer);

			DBReport("fpscr = 0x%08X\n", context.fpscr);

			DBReport("srr[0] = 0x%08X\n", context.srr[0]);
			DBReport("srr[1] = 0x%08X\n", context.srr[1]);

			DBReport("mode = 0x%02X\n", (uint8_t)context.mode);
			DBReport("state = 0x%02X\n", (uint8_t)context.state);
		}

		// Serialize

		Json::Value* output = new Json::Value();
		output->type = Json::ValueType::Array;

		for (int i = 0; i < 32; i++)
		{
			output->AddUInt32(nullptr, context.gpr[i]);
		}

		for (int i = 0; i < 32; i++)
		{
			output->AddFloat(nullptr, (float)context.fpr[i]);
		}

		for (int i = 0; i < 32; i++)
		{
			output->AddFloat(nullptr, (float)context.psr[i]);
		}

		for (int i = 0; i < 8; i++)
		{
			output->AddUInt32(nullptr, context.gqr[i]);
		}

		output->AddUInt32(nullptr, context.cr);
		output->AddUInt32(nullptr, context.lr);
		output->AddUInt32(nullptr, context.ctr);
		output->AddUInt32(nullptr, context.xer);

		output->AddUInt32(nullptr, context.fpscr);

		output->AddUInt32(nullptr, context.srr[0]);
		output->AddUInt32(nullptr, context.srr[1]);

		output->AddUInt16(nullptr, context.mode);
		output->AddUInt16(nullptr, context.state);

		return output;
	}

}
