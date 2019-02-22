#pragma once
#include "ConcurrentMemoryPool.h"
#include "ThreadCache.h"
#include "PageCache.h"

static void* ConcurrentAlloc(size_t size)
{
	if (size > MAXBYTES)//��ʽһ��ÿ�����������4���ֽڣ������Ĵ�С�������ͷţ���ʽ��������mapӳ��
	{
		//return malloc(size);
		size_t roundsize = ClassSize::_RoundUp(size, 1<<PAGE_SHIFT);//
		size_t npage = roundsize >> PAGE_SHIFT;     
		Span* span=PageCache::GetInstance()->NewSpan(npage);//����PageCacheʱ�����
		void* ptr = (void*)(span->_pageid << PAGE_SHIFT);
		return ptr;
	}
	else
	{
		// ͨ��tls����ȡ�߳��Լ���threadcache
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

	if (size > MAXBYTES)//����64k�Ķ���ReleaseSpanToPageCache()���д���
	{
		//return free(ptr);
		PageCache::GetInstance()->ReleaseSpanToPageCache(span);
	}
	else
	{
		tls_threadcache->Deallocate(ptr, size);
	}
}