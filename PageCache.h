#pragma once

#include"ConcurrentMemoryPool.h"
//#include"CentralCache.h"

class PageCache//同样为单例模式
{
public:
	static PageCache* GetInstance()
	{
		return &_inst;
	}

	Span* NewSpan(size_t npage);
	Span* _NewSpan(size_t npage);

	
	// 获取从对象到span的映射
	Span* MapObjectToSpan(void* obj);
	// 释放空闲span回到Pagecache，并合并相邻的span
	void ReleaseSpanToPageCache(Span* span);

	

private:
	SpanList _pagelist[NPAGES];//SpanList链表数组
	//map<PageID, Span*> _id_span_map;
	unordered_map<PageID, Span*> _id_span_map;
	
	mutex _mtx;

private:
	PageCache() = default;
	PageCache(const PageCache&) = delete;
	static PageCache _inst;
};
