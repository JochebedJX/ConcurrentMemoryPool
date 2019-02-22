#define _CRT_SECURE_NO_WARNINGS 1
#include"ConcurrentMemoryPool.h"
#include"CentralCache.h"
#include"PageCache.h"

CentralCache CentralCache::_inst;// 在程序入口之前就完成单例对象的初始化

Span* CentralCache::GetOneSpan(SpanList* spanlist, size_t bytes)
{
	Span* span = spanlist->begin();
	while (span != spanlist->end())
	{
		if (span->_objlist != nullptr)
			return span;

		span = span->_next;
	}

	//span全为空页,则向PageCache申请一定长度的newspan
	size_t npage = ClassSize::NumMovePage(bytes);
	Span* newspan = PageCache::GetInstance()->NewSpan(npage);

	//切割newspan，然后将其挂在spanlist上
	char* start = (char*)(newspan->_pageid << PAGE_SHIFT);//转换成首地址
	char* end = start + (newspan->_pagesize << PAGE_SHIFT);//首地址+页的大小
	char* cur = start;
	char* next = cur + bytes;
	while (next < end)//切割连接
	{
		NEXT_OBJ(cur) = next;
		cur = next;
		next = cur + bytes;
	}

	NEXT_OBJ(cur) = nullptr;

	newspan->_objlist = start;
	newspan->_objsize = bytes;
	newspan->_usecount = 0;

	//while (start != nullptr)
	//{
	//	PageID pageid = (PageID)start >> PAGE_SHIFT;
	//	_id_span_map[pageid] = newspan;
	//	start += bytes;
	//}

	// 将newspan插入到spanlist
	spanlist->PushFront(newspan);
	return newspan;
}

// 从中心缓存获取一定数量的对象给thread cache，需加锁
size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t n, size_t size)
{                                                                                          
	size_t index = ClassSize::Index(size);
	SpanList* spanlist = &_spanlist[index];
	unique_lock<mutex> lock(spanlist->_mtx);

	Span* span = GetOneSpan(spanlist, size);//取得一页不为空的span，该span里是一条自由链表，所含有的小块儿空间数目不确定

	void* cur = span->_objlist;//取自由链表
	void* prev = cur;
	size_t fetchnum = 0;
	while (cur != nullptr && fetchnum < n)
	{
		prev = cur;
		cur = NEXT_OBJ(cur);
		++fetchnum;
	}
	//要么cur==nullptr（即不够n个）要么fetchnum=n
	start = span->_objlist;
	end = prev;
	NEXT_OBJ(end) = nullptr;//start为第一个节点，end是最后一个节点

	span->_objlist = cur;//注意修改span里的参数
	span->_usecount += fetchnum;

	if (span->_objlist == nullptr)
	{
		spanlist->Erase(span);
		spanlist->PushBack(span);
	}

	return fetchnum;
}

// 将一定数量的对象释放到span跨度
void CentralCache::ReleaseListToSpans(void* start, size_t byte_size)
{
	size_t index = ClassSize::Index(byte_size);
	SpanList* spanlist = &_spanlist[index];//找到对应的SpanList
	unique_lock<mutex> lock(spanlist->_mtx);

	//根据对象地址找对应的pageip
	while (start)
	{
		void* next = NEXT_OBJ(start);
		Span* span = PageCache::GetInstance()->MapObjectToSpan(start);//找到start的映射位置span的位置
		//将不为空的span挂在spanlist的前面。
		if (span->_objlist != nullptr)
		{
			spanlist->Erase(span);
			spanlist->PushFront(span);
		}
		//将start对象挂在span的_objlist的自由链表之后
		NEXT_OBJ(start) = span->_objlist;
		span->_objlist = start;
		span->_usecount--;

		//在整理span，观察当前span的_usecount是否等于0，若为0，则将span释放回到PageCache进行合并
		if (span->_usecount == 0)
		{
			spanlist->Erase(span);

			//span上所挂对象回到整页

			span->_objlist = nullptr;
			span->_objsize = 0;
			span->_prev = nullptr;
			span->_next = nullptr;

			PageCache::GetInstance()->ReleaseSpanToPageCache(span);
		}
		

		start = next;
	}
}
