// Dump DolphinOS threads.

// Details on the DolphinOS threads can be found in \Docs\RE\thread.txt. Fairly simple and clean design.
// We catch on to the list of active threads (__OSLinkActive) and display them in turn. You can use the DumpContext command to display context.

#include "pch.h"

using namespace Debug;

namespace HLE
{

	static bool LoadOsThread(uint32_t ea, OSThread* thread)
	{
		int WIMG;

		// Translate effective address

		uint32_t threadPa = Gekko::Gekko->EffectiveToPhysical(ea, Gekko::MmuAccess::Read, WIMG);
		if (threadPa == Gekko::BadAddress)
		{
			Report(Channel::Norm, "Invalid thread effective address: 0x%08X\n", ea);
			return false;
		}

		uint8_t* ptr = MITranslatePhysicalAddress(threadPa, sizeof(OSThread));

		if (ptr == nullptr)
		{
			Report(Channel::Norm, "Invalid thread physical address: 0x%08X\n", threadPa);
			return false;
		}

		// Load thread struct and swap values

		*thread = *(OSThread*)ptr;

		thread->state = _BYTESWAP_UINT16(thread->state);
		thread->attr = _BYTESWAP_UINT16(thread->attr);
		thread->suspend = _BYTESWAP_UINT32(thread->suspend);
		thread->priority = _BYTESWAP_UINT32(thread->priority);
		thread->base = _BYTESWAP_UINT32(thread->base);
		thread->val = _BYTESWAP_UINT32(thread->val);

		thread->linkActive.next = _BYTESWAP_UINT32(thread->linkActive.next);
		thread->linkActive.prev = _BYTESWAP_UINT32(thread->linkActive.prev);

		thread->stackBase = _BYTESWAP_UINT32(thread->stackBase);
		thread->stackEnd = _BYTESWAP_UINT32(thread->stackEnd);

		// No need for other properties

		return true;
	}

	static void DumpOsThread(size_t count, OSThread* thread, uint32_t threadEa)
	{
		Report(Channel::Norm, "Thread %zi, context: 0x%08X:\n", count, threadEa);
		Report(Channel::Norm, "state: 0x%04X, attr: 0x%04X\n", thread->state, thread->attr);
		Report(Channel::Norm, "suspend: %i, priority: 0x%08X, base: 0x%08X, val: 0x%08X\n", (int)thread->suspend, thread->priority, thread->base, thread->val);
	}

	Json::Value* DumpDolphinOsThreads(bool displayOnScreen)
	{
		int WIMG;

		// Get pointer to __OSLinkActive

		uint32_t linkActiveEffectiveAddr = OS_LINK_ACTIVE;

		uint32_t linkActivePa = Gekko::Gekko->EffectiveToPhysical(linkActiveEffectiveAddr, Gekko::MmuAccess::Read, WIMG);
		if (linkActivePa == Gekko::BadAddress)
		{
			if (displayOnScreen)
			{
				Report(Channel::Norm, "Invalid active threads link effective address: 0x%08X\n", linkActiveEffectiveAddr);
			}
			return nullptr;
		}

		uint8_t* ptr = MITranslatePhysicalAddress(linkActivePa, sizeof(OSThreadLink));

		if (ptr == nullptr)
		{
			if (displayOnScreen)
			{
				Report(Channel::Norm, "Invalid active threads link physical address: 0x%08X\n", linkActivePa);
			}
			return nullptr;
		}

		OSThreadLink linkActive = *(OSThreadLink*)ptr;

		linkActive.next = _BYTESWAP_UINT32(linkActive.next);
		linkActive.prev = _BYTESWAP_UINT32(linkActive.prev);

		// Walk active threads

		Json::Value* output = new Json::Value();
		output->type = Json::ValueType::Array;

		if (displayOnScreen)
		{
			Report(Channel::Norm, "Dumping active DolphinOS threads:\n\n");
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
				Report(Channel::Norm, "\n");
			}
		}

		if (displayOnScreen)
		{
			Report(Channel::Norm, "Active threads: %zi. Use DumpContext command to dump thread context.\n", activeThreadsCount);
		}

		return output;
	}

	Json::Value* DumpDolphinOsContext(uint32_t effectiveAddr, bool displayOnScreen)
	{
		int WIMG;

		// Get context pointer

		uint32_t physAddr = Gekko::Gekko->EffectiveToPhysical(effectiveAddr, Gekko::MmuAccess::Read, WIMG);

		if (physAddr == Gekko::BadAddress)
		{
			if (displayOnScreen)
			{
				Report(Channel::Norm, "Invalid context effective address: 0x%08X\n", effectiveAddr);
			}
			return nullptr;
		}

		uint8_t* ptr = MITranslatePhysicalAddress(physAddr, sizeof(OSContext));

		if (ptr == nullptr)
		{
			if (displayOnScreen)
			{
				Report(Channel::Norm, "Invalid context physical address: 0x%08X\n", physAddr);
			}
			return nullptr;
		}

		// Copyout context and swap values

		OSContext context = *(OSContext*)ptr;

		for (int i = 0; i < 32; i++)
		{
			context.gpr[i] = _BYTESWAP_UINT32(context.gpr[i]);
			context.fprAsUint[i] = _BYTESWAP_UINT64(context.fprAsUint[i]);
			context.psrAsUint[i] = _BYTESWAP_UINT64(context.psrAsUint[i]);
		}

		for (int i = 0; i < 8; i++)
		{
			context.gqr[i] = _BYTESWAP_UINT32(context.gqr[i]);
		}

		context.cr = _BYTESWAP_UINT32(context.cr);
		context.lr = _BYTESWAP_UINT32(context.lr);
		context.ctr = _BYTESWAP_UINT32(context.ctr);
		context.xer = _BYTESWAP_UINT32(context.xer);

		context.fpscr = _BYTESWAP_UINT32(context.fpscr);

		context.srr[0] = _BYTESWAP_UINT32(context.srr[0]);
		context.srr[1] = _BYTESWAP_UINT32(context.srr[1]);

		context.mode = _BYTESWAP_UINT16(context.mode);
		context.state = _BYTESWAP_UINT16(context.state);

		// Dump contents

		if (displayOnScreen)
		{
			for (int i = 0; i < 32; i++)
			{
				Report(Channel::Norm, "gpr[%i] = 0x%08X\n", i, context.gpr[i]);
			}

			for (int i = 0; i < 32; i++)
			{
				Report(Channel::Norm, "fpr[%i] = %f (0x%llx)\n", i, context.fpr[i], context.fprAsUint[i]);
			}

			for (int i = 0; i < 32; i++)
			{
				Report(Channel::Norm, "psr[%i] = %f (0x%llx)\n", i, context.psr[i], context.psrAsUint[i]);
			}

			for (int i = 0; i < 8; i++)
			{
				Report(Channel::Norm, "gqr[%i] = 0x%08X\n", i, context.gqr[i]);
			}

			Report(Channel::Norm, "cr = 0x%08X\n", context.cr);
			Report(Channel::Norm, "lr = 0x%08X\n", context.lr);
			Report(Channel::Norm, "ctr = 0x%08X\n", context.ctr);
			Report(Channel::Norm, "xer = 0x%08X\n", context.xer);

			Report(Channel::Norm, "fpscr = 0x%08X\n", context.fpscr);

			Report(Channel::Norm, "srr[0] = 0x%08X\n", context.srr[0]);
			Report(Channel::Norm, "srr[1] = 0x%08X\n", context.srr[1]);

			Report(Channel::Norm, "mode = 0x%02X\n", (uint8_t)context.mode);
			Report(Channel::Norm, "state = 0x%02X\n", (uint8_t)context.state);
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
