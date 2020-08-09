// ThreadDemo
//

#include "pch.h"

SpinLock Girl;

void ThreadBoy1(void* param)
{
	Girl.Lock();

	printf("Boy1 owning Girl.\n");

	Thread::Sleep(100);

	Girl.Unlock();
}

void ThreadBoy2(void* param)
{
	Girl.Lock();

	printf("Boy2 owning Girl.\n");

	Thread::Sleep(100);

	Girl.Unlock();
}

void SpamThread(void* param)
{
	printf("Spam! ");
	Thread::Sleep(5);
}

int main()
{
	// Mutual-exclusive syncronization

	Thread* boy1 = new Thread(ThreadBoy1, false, nullptr, "Boy1");
	Thread* boy2 = new Thread(ThreadBoy2, false, nullptr, "Boy2");

	Thread::Sleep(2000);

	delete boy1;
	delete boy2;

	// Suspend/Resume

	Thread *spam = new Thread(SpamThread, true, nullptr, "Spam");

	for (int i = 0; i < 10; i++)
	{
		Thread::Sleep(500);
		spam->Resume();
		Thread::Sleep(500);
		spam->Suspend();
	}

	delete spam;

	printf("\n");

	return 0;
}
