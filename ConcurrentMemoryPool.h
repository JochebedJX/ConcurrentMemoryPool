#pragma once

#include<iostream>
#include<assert.h>
#include<vector>
#include<thread>
#include<mutex>
#include<unordered_map>
#include<map>

using namespace std;

#ifdef _WIN32
#include <windows.h>
#endif // _WIN32

const size_t NLISTS = 240;//管理对象自由链表的长度
const size_t MAXBYTES = 64 * 1024;//单个自由链表最大字节大小
const size_t NPAGES = 129;//最大页的数量
const size_t PAGE_SHIFT = 12;//一页的大小是2^12字节

//向系统申请空间
static void * SystemAlloc(size_t npage)
{
	void* ptr = VirtualAlloc(NULL, npage << PAGE_SHIFT, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	if (ptr == nullptr)
	{
		throw bad_alloc();
	}
	return ptr;
}

static void SystemFree(void* ptr)
{
	VirtualFree(ptr, 0, MEM_RELEASE);
}

static inline void*& NEXT_OBJ(void* obj)
{
	return *((void**)obj);
}

class FreeList
{
public:
	bool Empty()
	{
		return _list == nullptr;
	}
	void PushRange(void* start, void* end, size_t num)//从中心缓存中获得一段空间头插进入自由链表
	{
		NEXT_OBJ(end) = _list;//*((void**)end)=_list;
		_list = start;
		_size += num;
	}
	void* Pop()
	{
		void* obj = _list;
		_list = NEXT_OBJ(obj);
		--_size;

		return obj;
	}
	void Push(void* obj)
	{
		NEXT_OBJ(obj) = _list;
		_list = obj;
		++_size;
	}
	void* Clear()
	{
		void* list = _list;
		_list = nullptr;
		_size = 0;
		return list;
	}
	size_t Size()
	{
		return _size;
	}
	void SetMaxSize(size_t maxsize)
	{
		_maxsize = maxsize;
	}
	size_t MaxSize()
	{
		return _maxsize;
	}
private:
	void *_list=nullptr;
	size_t _size = 0;
	size_t _maxsize = 1;
};

class ClassSize//管理内存对齐映射
{
	// 控制在12%左右的内碎片浪费
	// [1,128]				8 byte对齐		freelist[0,16)
	// [129,1024]			16byte对齐		freelist[16,72)
	// [1025,8*1024]		128byte对齐		freelist[72,128)
	// [8*1024+1,64*1024]	512byte对齐		freelist[128,240)
public:
	static inline size_t _RoundUp(size_t size, size_t align)//size为已知大小，align为对齐字节，需判断
	{  
		return (size + (align - 1))&~(align - 1);//返回一条FreeList中size位于链表的哪个位置,对应的所开的最大空间size
	}

	static inline size_t RoundUp(size_t size)//返回一条FreeList中size位于链表的哪个位置
	{
		assert(size <= MAXBYTES);
		if (size <= 128)//比较对齐
		{
			return _RoundUp(size, 8);
		}
		else if (size <= 1024)//比较对齐
		{
			return _RoundUp(size, 16);
		}
		else if (size <= 8192)//8*1024
		{
			return _RoundUp(size, 128);
		}
		else if (size <= 64 * 1024)
		{
			return _RoundUp(size, 512);
		}
		return -1;
	}
	static inline size_t _Index(size_t size, size_t align)
	{
		return ((size + (1 << align) - 1) >> align) - 1;//return ((size + align_shift - 1) /align_shift) - 1;
	}
	static inline size_t Index(size_t size)
	{
		assert(size <= MAXBYTES);
		static int group_array[4] = { 16, 56, 56, 112 };
		if (size <= 128)//比较对齐
		{
			return _Index(size, 3);
		}
		else if (size <= 1024)//比较对齐
		{
			return _Index(size - 128, 4) + group_array[0];
		}
		else if (size <= 8192)//8*1024
		{
			return _Index(size - 1024, 7) + group_array[0] + group_array[1];
		}
		else if (size <= 64 * 1024)
		{
			return _Index(size - 8192, 9) + group_array[0] + group_array[1] + group_array[2];
		}
		return -1;
	}

	static size_t NumMoveSize(size_t size)
	{
		if (size == 0)
			return 0;

		int num = static_cast<int>(MAXBYTES / size);
		if (num < 2)//按64k来算，为12页，需要两份
			num = 2;

		if (num > 512)//按8来算，最少需要1页，4096/8
			num = 512;

		return num;
	}
	// 计算一次向系统获取几个页
	static size_t NumMovePage(size_t size)//size是对齐数
	{
		size_t num = NumMoveSize(size);
		size_t npage = (num*size)>>PAGE_SHIFT;//除以4k，为页数

		if (npage == 0)
			npage = 1;

		return npage;
	}
};

typedef size_t PageID;

struct Span
{
	//以一页为单位来看
	PageID _pageid = 0;			//页号
	size_t _pagesize = 0;		//页的大小

	Span* _prev = nullptr;
	Span* _next = nullptr;

	//以每一页里的对象为单位来看
	void*  _objlist = nullptr;	// 对象自由链表，像ThreadCache里的自由链表一样，会将取得的span分割成一小块一小块儿的，即方便ThreadCache取空间
	size_t _objsize = 0;		// 对象大小，无上限，因为一页span是固定的，都是每一条_objlist里分割的小块儿空间是不确定的，所以总的size并不知道，当span切割时确定
	size_t _usecount = 0;		// 使用计数，非常重要的一个参数，用于判断一页span是否完全还回来了，即可以回收进入PageCache，实现内存碎片的回收
};

class SpanList//双向带头循环链表
{
public:
	SpanList()
	{
		_head = new Span;
		_head->_next = _head;
		_head->_prev = _head;
	}

	Span* begin()
	{
		return _head->_next;
	}

	Span* end()
	{
		return _head;
	}

	bool Empty()
	{
		return _head->_next == _head;
	}

	void Insert(Span* cur, Span* newspan)
	{
		assert(cur);

		Span* prev = cur->_prev;

		newspan->_next = cur;
		newspan->_prev = prev;
		cur->_prev = newspan;
		prev->_next = newspan;
	}

	void Erase(Span* cur)
	{
		Span* prev = cur->_prev;
		Span* next = cur->_next;

		prev->_next = next;
		next->_prev = prev;
	}

	void PushBack(Span* span)
	{
		Insert(end(), span);
	}

	Span* PopBack()
	{
		Span* tail = _head->_prev;
		Erase(tail);
		return tail;
	}

	void PushFront(Span* span)
	{
		Insert(begin(), span);
	}

	Span* PopFront()
	{
		Span* span = begin();
		Erase(span);
		return span;
	}
	std::mutex _mtx;//桶锁，方便多线程取数据相互不干扰

private:
	Span* _head = nullptr;
};

