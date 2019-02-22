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

const size_t NLISTS = 240;//���������������ĳ���
const size_t MAXBYTES = 64 * 1024;//����������������ֽڴ�С
const size_t NPAGES = 129;//���ҳ������
const size_t PAGE_SHIFT = 12;//һҳ�Ĵ�С��2^12�ֽ�

//��ϵͳ����ռ�
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
	void PushRange(void* start, void* end, size_t num)//�����Ļ����л��һ�οռ�ͷ�������������
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

class ClassSize//�����ڴ����ӳ��
{
	// ������12%���ҵ�����Ƭ�˷�
	// [1,128]				8 byte����		freelist[0,16)
	// [129,1024]			16byte����		freelist[16,72)
	// [1025,8*1024]		128byte����		freelist[72,128)
	// [8*1024+1,64*1024]	512byte����		freelist[128,240)
public:
	static inline size_t _RoundUp(size_t size, size_t align)//sizeΪ��֪��С��alignΪ�����ֽڣ����ж�
	{  
		return (size + (align - 1))&~(align - 1);//����һ��FreeList��sizeλ��������ĸ�λ��,��Ӧ�����������ռ�size
	}

	static inline size_t RoundUp(size_t size)//����һ��FreeList��sizeλ��������ĸ�λ��
	{
		assert(size <= MAXBYTES);
		if (size <= 128)//�Ƚ϶���
		{
			return _RoundUp(size, 8);
		}
		else if (size <= 1024)//�Ƚ϶���
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
		if (size <= 128)//�Ƚ϶���
		{
			return _Index(size, 3);
		}
		else if (size <= 1024)//�Ƚ϶���
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
		if (num < 2)//��64k���㣬Ϊ12ҳ����Ҫ����
			num = 2;

		if (num > 512)//��8���㣬������Ҫ1ҳ��4096/8
			num = 512;

		return num;
	}
	// ����һ����ϵͳ��ȡ����ҳ
	static size_t NumMovePage(size_t size)//size�Ƕ�����
	{
		size_t num = NumMoveSize(size);
		size_t npage = (num*size)>>PAGE_SHIFT;//����4k��Ϊҳ��

		if (npage == 0)
			npage = 1;

		return npage;
	}
};

typedef size_t PageID;

struct Span
{
	//��һҳΪ��λ����
	PageID _pageid = 0;			//ҳ��
	size_t _pagesize = 0;		//ҳ�Ĵ�С

	Span* _prev = nullptr;
	Span* _next = nullptr;

	//��ÿһҳ��Ķ���Ϊ��λ����
	void*  _objlist = nullptr;	// ��������������ThreadCache�����������һ�����Ὣȡ�õ�span�ָ��һС��һС����ģ�������ThreadCacheȡ�ռ�
	size_t _objsize = 0;		// �����С�������ޣ���Ϊһҳspan�ǹ̶��ģ�����ÿһ��_objlist��ָ��С����ռ��ǲ�ȷ���ģ������ܵ�size����֪������span�и�ʱȷ��
	size_t _usecount = 0;		// ʹ�ü������ǳ���Ҫ��һ�������������ж�һҳspan�Ƿ���ȫ�������ˣ������Ի��ս���PageCache��ʵ���ڴ���Ƭ�Ļ���
};

class SpanList//˫���ͷѭ������
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
	std::mutex _mtx;//Ͱ����������߳�ȡ�����໥������

private:
	Span* _head = nullptr;
};

