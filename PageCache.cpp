#define _CRT_SECURE_NO_WARNINGS 1

#include"PageCache.h"

PageCache PageCache::_inst;

Span* PageCache::NewSpan(size_t npage)
{
	unique_lock<mutex> lock(_mtx);
	if (npage >= NPAGES)
	{
		void* ptr = SystemAlloc(npage);
		Span* span = new Span();
		span->_pageid = (PageID)ptr >> PAGE_SHIFT;
		span->_pagesize = npage;
		span->_objsize = npage << PAGE_SHIFT;
		_id_span_map[span->_pageid] = span;

		return span;
	}

	//从系统申请大于64K小于128页的内存的时候，需要将span的objsize进行一个设置为了释放的时候进行合并
	Span* span = _NewSpan(npage);
	//这个就是对于一个从PageCache申请span的时候，来记录申请这个span的所要分割的时候
	span->_objsize = span->_pagesize << PAGE_SHIFT;

	return span;
}
Span* PageCache::_NewSpan(size_t npage)
{
	//unique_lock<mutex> lock(_mtx);//死锁

	if (!_pagelist[npage].Empty())
	{
		return _pagelist[npage].PopFront();
	}

	//_pagelist[npage].Empty(),往数组后面找，直到最后都没有，就向系统申请一个128page大小的空间，然后划分为小空间
	for (size_t i = npage + 1; i < NPAGES; i++)
	{
		SpanList* pagelist = &_pagelist[i];
		if (!pagelist->Empty())
		{
			Span* span = pagelist->PopFront();//先将有span的pagelist拿出来，然后进行分割处理，切下来的就拿来用，剩下的插入对应页数的数组下标的位置
			Span* wantspan = new Span;
			wantspan->_pageid = span->_pageid + span->_pagesize - npage;
			wantspan->_pagesize = npage;
			span->_pagesize -= npage;
			

			for (size_t i = 0; i < wantspan->_pagesize; ++i)
			{
				_id_span_map[wantspan->_pageid + i] = wantspan;
			}
		    
			_pagelist[span->_pagesize].PushFront(span);

			return wantspan;
		}
	}
	//向系统申请空间
	//void* ptr = VirtualAlloc(NULL, (NPAGES - 1) << PAGE_SHIFT, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	//if (ptr == nullptr)
	//{
	//	throw bad_alloc();
	//}
	void* ptr = SystemAlloc((NPAGES - 1));

	//以申请空间,大小为(NPAGES-1)
	Span* maxspan = new Span;
	maxspan->_pageid = (PageID)ptr >> PAGE_SHIFT;
	maxspan->_pagesize = (NPAGES - 1);
	_pagelist[NPAGES - 1].PushFront(maxspan);

	for (size_t i = 0; i < maxspan->_pagesize; ++i)
	{
		_id_span_map[maxspan->_pageid + i] = maxspan;
	}
	return _NewSpan(npage);
}

// 获取从对象到span的映射
Span* PageCache::MapObjectToSpan(void* obj)
{
	PageID pageid = (PageID)obj >> PAGE_SHIFT;//通过地址强转成整形，因为一页=4096字节，页按照地址画分，除以4k就能得到页号
	auto it = _id_span_map.find(pageid);
	//unordered_map<PageID, Span*>::iterator it = _id_span_map.find(pageid);
	
	assert(it != _id_span_map.end());
	return it->second;//Span*
}

// 释放空闲span回到PageCache，合并相邻的span
void PageCache::ReleaseSpanToPageCache(Span* span)
{
	unique_lock<mutex> lock(_mtx);

	if (span->_pagesize >= NPAGES)
	{
		void* ptr = (void*)(span->_pageid << PAGE_SHIFT);
		_id_span_map.erase(span->_pageid);
		SystemFree(ptr);
		delete span;
		return;
	}

	//根据PageID来合并span,先找前面的span，再找后面的页，最后再挂到对应大小的pagelist上
	auto previt = _id_span_map.find(span->_pageid - 1);
	while (previt != _id_span_map.end())
	{
		if (previt->second->_usecount != 0)
			break;
		
		// 如果合并出超过NPAGES页的span，则不合并，否则没办法管理
		if (previt->second->_pagesize + span->_pagesize >= NPAGES)
			break;

		else
		{
			_pagelist[previt->second->_pagesize].Erase(previt->second);//想将其拿出来处理
			previt->second->_pagesize += span->_pagesize;
			//for (size_t i = 0; i < span->_pagesize; ++i)//映射
			//{
			//	_id_span_map[span->_pageid + i] = previt->second;
			//}
			delete span;//将span挂在prevspan后面
			span = previt->second;
			
			previt = _id_span_map.find(span->_pageid - 1);
		}
	}
	
	auto nextit = _id_span_map.find(span->_pageid +span->_pagesize);
	while (nextit != _id_span_map.end())
	{
		if (nextit->second->_usecount != 0)
			break;

		// 如果合并出超过NPAGES页的span，则不合并，否则没办法管理
		if (span->_pagesize + nextit->second->_pagesize >= NPAGES)
			break;

		else
		{
			_pagelist[nextit->second->_pagesize].Erase(nextit->second);
			span->_pagesize += nextit->second->_pagesize;
			size_t n = nextit->second->_pagesize;
			delete nextit->second;

			//for (size_t i = 0; i < n ; ++i)//映射
			//{
			//	_id_span_map[span->_pageid + span->_pagesize + i] = span;//operator[]通过key找到与key对应的value然后返回其引用,所以相当于第二个参数指向span
			//}
			nextit = _id_span_map.find(span->_pageid + span->_pagesize);
		}
	}

	//将合并好的页都映射到新的span上
	for (size_t i = 0; i < span->_pagesize ; i++)
	{
		_id_span_map[span->_pageid + i] = span;
	}
	//将相应的span挂在对应的位置
	_pagelist[span->_pagesize].PushFront(span);
}

//// 释放空闲span回到Pagecache，并合并相邻的span
//void PageCache::ReleaseSpanToPageCache(Span* span)
//{
//	auto previt = _id_span_map.find(span->_pageid - 1);
//	while (previt != _id_span_map.end())
//	{
//		Span* prevspan = previt->second;
//		// 不是空闲，则直接跳出
//		if (prevspan->_usecount != 0)
//			break;
//
//		// 如果合并出超过NPAGES页的span，则不合并，否则没办法管理
//		if (prevspan->_pagesize + span->_pagesize >= NPAGES)
//			break;
//
//		_pagelist[prevspan->_pagesize].Erase(prevspan);
//		prevspan->_pagesize += span->_pagesize;
//		delete span;
//		span = prevspan;
//
//		previt = _id_span_map.find(span->_pageid - 1);
//	}
//
//	auto nextit = _id_span_map.find(span->_pageid + span->_pagesize);
//	while (nextit != _id_span_map.end())
//	{
//		Span* nextspan = nextit->second;
//		if (nextspan->_usecount != 0)
//			break;
//
//		if (span->_pagesize + nextspan->_pagesize >= NPAGES)
//			break;
//
//		_pagelist[nextspan->_pagesize].Erase(nextspan);
//		span->_pagesize += nextspan->_pagesize;
//		delete nextspan;
//
//		nextit = _id_span_map.find(span->_pageid + span->_pagesize);
//	}
//
//	for (size_t i = 0; i < span->_pagesize; ++i)
//	{
//		_id_span_map[span->_pageid + i] = span;
//	}
//	_pagelist[span->_pagesize].PushFront(span);
//}