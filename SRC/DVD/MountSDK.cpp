/*
Code for mounting the Dolphin SDK folder as a virtual disk.

All the necessary data (BI2, Appldr, some DOL executable, we take from the SDK). If they are not there, then the disk is simply not mounted.
*/

#include "pch.h"

namespace DVD
{
	MountDolphinSdk::MountDolphinSdk(const TCHAR * DolphinSDKPath)
	{
		_tcscpy_s(directory, _countof(directory) - 1, DolphinSDKPath);

		if (!GenDiskId())
			return;
		if (!GenApploader())
			return;
		if (!GenBi2())
			return;
		if (!GenDvdData())
			return;
		if (!GenDol())
			return;
		if(!GenBb2())
			return;

		if (!GenMap())
			return;

		mounted = true;
	}

	MountDolphinSdk::~MountDolphinSdk()
	{
	}

	void MountDolphinSdk::MapVector(std::vector<uint8_t>& v, uint32_t offset)
	{
		std::tuple<uint8_t*, uint32_t, size_t> entry(v.data(), offset, v.size());
		mapping.push_back(entry);
	}

	uint8_t* MountDolphinSdk::Translate(uint32_t offset, size_t requestedSize, size_t& maxSize)
	{
		for (auto it = mapping.begin(); it != mapping.end(); ++it)
		{
			uint8_t * ptr = std::get<0>(*it);
			uint32_t startingOffset = std::get<1>(*it);
			size_t size = std::get<2>(*it);

			if (startingOffset <= offset && offset < (startingOffset + size))
			{
				maxSize = min(requestedSize, (startingOffset + size) - offset);
				return ptr + offset;
			}
		}
		return nullptr;
	}

	void MountDolphinSdk::Seek(int position)
	{
		if (!mounted)
			return;

		assert(position >= 0 && position < DVD_SIZE);

		currentSeek = (uint32_t)position;
	}

	void MountDolphinSdk::Read(void* buffer, size_t length)
	{
		assert(buffer);

		if (!mounted)
		{
			memset(buffer, 0, length);
			return;
		}

		size_t maxLength = 0;
		
		uint8_t* ptr = Translate(currentSeek, length, maxLength);
		if (ptr != nullptr)
		{
			memcpy(buffer, ptr, maxLength);
			if (maxLength < length)
			{
				memset((uint8_t *)buffer + maxLength, 0, length - maxLength);
			}
		}
		else
		{
			memset(buffer, 0, length);
		}
	}

	#pragma region "Data Generators"

	bool MountDolphinSdk::GenDiskId()
	{
		return false;
	}

	bool MountDolphinSdk::GenApploader()
	{
		return false;
	}

	bool MountDolphinSdk::GenDol()
	{
		return false;
	}

	bool MountDolphinSdk::GenBi2()
	{
		return false;
	}

	bool MountDolphinSdk::GenDvdData()
	{
		return false;
	}

	bool MountDolphinSdk::GenBb2()
	{
		return false;
	}

	bool MountDolphinSdk::GenMap()
	{
		return false;
	}

	#pragma endregion "Data Generators"

}
