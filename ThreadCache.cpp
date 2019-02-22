#define _CRT_SECURE_NO_WARNINGS 1

#include"ThreadCache.h"
#include"ConcurrentMemoryPool.h"
#include"CentralCache.h"

void *ThreadCache::Allocate(size_t size)
{
	assert(size <= MAXBYTES);
	size = ClassSize::RoundUp(size);//注意此size的变化
	size_t index = ClassSize::Index(size);
	FreeList* freelist = &_freelist[index];

	if (!freelist->Empty())
	{
		return freelist->Pop();
	}
	else//向CentralCache取一段一小块一小块的空间
	{
		return FetchFromCentralCache(index, size);//此size是已经RaundUp了的size
	}
}
void ThreadCache::ListTooLong(FreeList* freelist, size_t byte)//向CentralCache里插入一个一个对象，需考虑页和对象的映射关系
{
	void* start = freelist->Clear();
	CentralCache::GetInstance()->ReleaseListToSpans(start, byte);
}
void *ThreadCache::Deallocate(void* ptr, size_t size)
{
	assert(size <= MAXBYTES);
	
	size_t index = ClassSize::Index(size);
	FreeList* freelist = &_freelist[index];

	freelist->Push(ptr);

	//当只有链表合成超过一定长度，则需要向CentralCache回收
	if (freelist->Size() >= freelist->MaxSize())
	{
		ListTooLong(freelist, size);
	}
	//ThreadCache总的字节数超过2M，则开始释放

	return freelist;
}
void* ThreadCache::FetchFromCentralCache(size_t index, size_t size)
{
	FreeList* freelist = &_freelist[index];
	
	size_t num_to_move = min(ClassSize::NumMoveSize(size),freelist->MaxSize()); 
	void* start; void* end;

	size_t fetchnum = CentralCache::GetInstance()->FetchRangeObj(start, end, num_to_move, size);//看实际返回空间数目,start指向开头，end是最后一个节点

	if (fetchnum == 1)
		return start;
	else
	{
		freelist->FreeList::PushRange(NEXT_OBJ(start), end, fetchnum - 1);//将除去start（被用）剩下的将挂上ThreadCache里的freelist
		//NEXT_OBJ(start) = nullptr;
	}
	if (num_to_move == freelist->MaxSize())
	{
		freelist->SetMaxSize(num_to_move + 1);
	}
	return start;//被切割出来的一块儿空间
}