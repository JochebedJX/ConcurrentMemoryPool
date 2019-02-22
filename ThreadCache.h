#ifndef __THREAD_CACHE_H__
#define __THREAD_CACHE_H__

#include"ConcurrentMemoryPool.h"

class ThreadCache{
public:
	//创建对象开空间
	void *Allocate(size_t size);
	
	//释放空间给CentralCache
	void *Deallocate(void* ptr, size_t size);
	void ListTooLong(FreeList* freelist, size_t byte);
	
	// 从中心缓存获取对象
	void* FetchFromCentralCache(size_t index, size_t size);
private:
	FreeList _freelist[NLISTS];
};

//平台可能不兼容，可使用动态的，使用每个线程调接口的方式。
static _declspec(thread) ThreadCache* tls_threadcache = nullptr;//保存每个线程本地的ThreadCache的指针,每个线程独有的全局变量,每个线程的tls变量相互不影响

#endif //__THREAD_CACHE_H__
