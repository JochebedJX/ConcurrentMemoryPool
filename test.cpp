#define _CRT_SECURE_NO_WARNINGS 1
#include"CentralCache.h"
#include"ConcurrentMemoryPool.h"
#include"PageCache.h"
#include"ThreadCache.h"
#include "ConcurrentAlloc.h"

void TestThreadCache()
{
	/*std::thread t1(ConcurrentAlloc, 10);
	std::thread t2(ConcurrentAlloc, 10);

	t1.join();
	t2.join();*/

	std::vector<void*> v;
	for (size_t i = 0; i < 10; ++i)
	{
		v.push_back(ConcurrentAlloc(10));
		cout << v.back() << endl;
	}
	cout << endl << endl;

	for (size_t i = 0; i < 10; ++i)
	{
		ConcurrentFree(v[i]);
	}
	v.clear();

	for (size_t i = 0; i < 10; ++i)
	{
		v.push_back(ConcurrentAlloc(10));
		cout << v.back() << endl;
	}

	for (size_t i = 0; i < 10; ++i)
	{
		ConcurrentFree(v[i]);
	}
	v.clear();
}

void TestPageCache()
{
	//void* ptr = VirtualAlloc(NULL, (NPAGES - 1) << PAGE_SHIFT, MEM_RESERVE, PAGE_READWRITE);
	void* ptr = malloc((NPAGES - 1) << PAGE_SHIFT);
	cout << ptr << endl;
	if (ptr == nullptr)
	{
		throw std::bad_alloc();
	}

	PageID pageid = (PageID)ptr >> PAGE_SHIFT;
	cout << pageid << endl;

	void* shiftptr = (void*)(pageid << PAGE_SHIFT);
	cout << shiftptr << endl;
}

void TestConcurrentAlloc()
{
	size_t n = 514;
	std::vector<void*> v;
	for (size_t i = 0; i < n; ++i)
	{
		v.push_back(ConcurrentAlloc(1025));
		cout << v.back() << endl;
	}
	for (size_t i = 0; i < n; ++i)
	{
		ConcurrentFree(v[i]);
		cout << i << endl;
	}
	v.clear();
	
	cout << endl << endl;
	cout << endl << endl;

	for (size_t i = 0; i < n; ++i)
	{
		v.push_back(ConcurrentAlloc(10));
		cout << v.back() << endl;
	}


	for (size_t i =
		0; i < n; ++i)
	{
		ConcurrentFree(v[i]);
	}
	v.clear();
	cout << endl << endl;
}

//int main()
//{
//	//TestThreadCache();
//	TestConcurrentAlloc();
//	//TestPageCache();
//
//	system("pause");
//	return 0;
//}


