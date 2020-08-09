// ThreadDemo
//

#include "pch.h"

SpinLock Girl;

void ThreadBoy1(void* param)
{
	while (true)
	{
		Girl.Lock();

		printf("Boy1 owning Girl.\n");

		Thread::Sleep(100);

		Girl.Unlock();
	}
}

void ThreadBoy2(void* param)
{
	while (true)
	{
		Girl.Lock();

		printf("Boy2 owning Girl.\n");

		Thread::Sleep(100);

		Girl.Unlock();
	}
}

int main()
{
	Thread* boy1 = new Thread(ThreadBoy1, false, nullptr, "Boy1");
	Thread* boy2 = new Thread(ThreadBoy2, false, nullptr, "Boy2");

	Thread::Sleep(2000);

	delete boy1;
	delete boy2;

	return 0;
}
