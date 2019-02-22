#pragma once

#include"ConcurrentMemoryPool.h"

class CentralCache//需设计为单例模式，让其ThreadCache开空间时是线程安全的
{
public:
	static CentralCache* GetInstance(){
		return &_inst;
	}

	Span* GetOneSpan(SpanList* spanlist, size_t bytes);

	// 从中心缓存获取一定数量的对象给thread cache
	size_t FetchRangeObj(void*& start, void*& end, size_t n, size_t size);

	// 将一定数量的对象释放到span跨度
	void ReleaseListToSpans(void* start, size_t byte_size);

private:
	CentralCache() = default;//调默认成员函数
	CentralCache(const CentralCache&) = delete;//无法实现，也可以只声明不实现+声明成私有
	CentralCache& operator=(const CentralCache&) = delete;

	static CentralCache _inst;

private:
	// 中心缓存自由链表
	SpanList _spanlist[NLISTS];//形似ThreadCache
};


