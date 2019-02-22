#pragma once
#include "ConcurrentMemoryPool.h"
#include "ThreadCache.h"
#include "PageCache.h"

static void* ConcurrentAlloc(size_t size)
{
	if (size > MAXBYTES)//方式一：每次申请多申请4个字节，存对象的大小，方便释放；方式二：借用map映射
	{
		//return malloc(size);
		size_t roundsize = ClassSize::_RoundUp(size, 1<<PAGE_SHIFT);//
		size_t npage = roundsize >> PAGE_SHIFT;     
		Span* span=PageCache::GetInstance()->NewSpan(npage);//进入PageCache时须加锁
		void* ptr = (void*)(span->_pageid << PAGE_SHIFT);
		return ptr;
	}
	else
	{
		// 通过tls，获取线程自己的threadcache
		if (tls_threadcache == nullptr)
		{
			tls_threadcache = new ThreadCache;
			//cout << std::this_thread::get_id() << "->" << tls_threadcache << endl;
		}

		return tls_threadcache->Allocate(size);
	}
}

static void ConcurrentFree(void* ptr)
{
	Span* span = PageCache::GetInstance()->MapObjectToSpan(ptr);
	size_t size = span->_objsize;

	if (size > MAXBYTES)//大于64k的都在ReleaseSpanToPageCache()进行处理
	{
		//return free(ptr);
		PageCache::GetInstance()->ReleaseSpanToPageCache(span);
	}
	else
	{
		tls_threadcache->Deallocate(ptr, size);
	}
}