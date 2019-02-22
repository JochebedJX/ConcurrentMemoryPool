#define _CRT_SECURE_NO_WARNINGS 1

#include"ThreadCache.h"
#include"ConcurrentMemoryPool.h"
#include"CentralCache.h"

void *ThreadCache::Allocate(size_t size)
{
	assert(size <= MAXBYTES);
	size = ClassSize::RoundUp(size);//ע���size�ı仯
	size_t index = ClassSize::Index(size);
	FreeList* freelist = &_freelist[index];

	if (!freelist->Empty())
	{
		return freelist->Pop();
	}
	else//��CentralCacheȡһ��һС��һС��Ŀռ�
	{
		return FetchFromCentralCache(index, size);//��size���Ѿ�RaundUp�˵�size
	}
}
void ThreadCache::ListTooLong(FreeList* freelist, size_t byte)//��CentralCache�����һ��һ�������迼��ҳ�Ͷ����ӳ���ϵ
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

	//��ֻ������ϳɳ���һ�����ȣ�����Ҫ��CentralCache����
	if (freelist->Size() >= freelist->MaxSize())
	{
		ListTooLong(freelist, size);
	}
	//ThreadCache�ܵ��ֽ�������2M����ʼ�ͷ�

	return freelist;
}
void* ThreadCache::FetchFromCentralCache(size_t index, size_t size)
{
	FreeList* freelist = &_freelist[index];
	
	size_t num_to_move = min(ClassSize::NumMoveSize(size),freelist->MaxSize()); 
	void* start; void* end;

	size_t fetchnum = CentralCache::GetInstance()->FetchRangeObj(start, end, num_to_move, size);//��ʵ�ʷ��ؿռ���Ŀ,startָ��ͷ��end�����һ���ڵ�

	if (fetchnum == 1)
		return start;
	else
	{
		freelist->FreeList::PushRange(NEXT_OBJ(start), end, fetchnum - 1);//����ȥstart�����ã�ʣ�µĽ�����ThreadCache���freelist
		//NEXT_OBJ(start) = nullptr;
	}
	if (num_to_move == freelist->MaxSize())
	{
		freelist->SetMaxSize(num_to_move + 1);
	}
	return start;//���и������һ����ռ�
}