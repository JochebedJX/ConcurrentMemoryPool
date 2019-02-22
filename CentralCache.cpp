#define _CRT_SECURE_NO_WARNINGS 1
#include"ConcurrentMemoryPool.h"
#include"CentralCache.h"
#include"PageCache.h"

CentralCache CentralCache::_inst;// �ڳ������֮ǰ����ɵ�������ĳ�ʼ��

Span* CentralCache::GetOneSpan(SpanList* spanlist, size_t bytes)
{
	Span* span = spanlist->begin();
	while (span != spanlist->end())
	{
		if (span->_objlist != nullptr)
			return span;

		span = span->_next;
	}

	//spanȫΪ��ҳ,����PageCache����һ�����ȵ�newspan
	size_t npage = ClassSize::NumMovePage(bytes);
	Span* newspan = PageCache::GetInstance()->NewSpan(npage);

	//�и�newspan��Ȼ�������spanlist��
	char* start = (char*)(newspan->_pageid << PAGE_SHIFT);//ת�����׵�ַ
	char* end = start + (newspan->_pagesize << PAGE_SHIFT);//�׵�ַ+ҳ�Ĵ�С
	char* cur = start;
	char* next = cur + bytes;
	while (next < end)//�и�����
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

	// ��newspan���뵽spanlist
	spanlist->PushFront(newspan);
	return newspan;
}

// �����Ļ����ȡһ�������Ķ����thread cache�������
size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t n, size_t size)
{                                                                                          
	size_t index = ClassSize::Index(size);
	SpanList* spanlist = &_spanlist[index];
	unique_lock<mutex> lock(spanlist->_mtx);

	Span* span = GetOneSpan(spanlist, size);//ȡ��һҳ��Ϊ�յ�span����span����һ���������������е�С����ռ���Ŀ��ȷ��

	void* cur = span->_objlist;//ȡ��������
	void* prev = cur;
	size_t fetchnum = 0;
	while (cur != nullptr && fetchnum < n)
	{
		prev = cur;
		cur = NEXT_OBJ(cur);
		++fetchnum;
	}
	//Ҫôcur==nullptr��������n����Ҫôfetchnum=n
	start = span->_objlist;
	end = prev;
	NEXT_OBJ(end) = nullptr;//startΪ��һ���ڵ㣬end�����һ���ڵ�

	span->_objlist = cur;//ע���޸�span��Ĳ���
	span->_usecount += fetchnum;

	if (span->_objlist == nullptr)
	{
		spanlist->Erase(span);
		spanlist->PushBack(span);
	}

	return fetchnum;
}

// ��һ�������Ķ����ͷŵ�span���
void CentralCache::ReleaseListToSpans(void* start, size_t byte_size)
{
	size_t index = ClassSize::Index(byte_size);
	SpanList* spanlist = &_spanlist[index];//�ҵ���Ӧ��SpanList
	unique_lock<mutex> lock(spanlist->_mtx);

	//���ݶ����ַ�Ҷ�Ӧ��pageip
	while (start)
	{
		void* next = NEXT_OBJ(start);
		Span* span = PageCache::GetInstance()->MapObjectToSpan(start);//�ҵ�start��ӳ��λ��span��λ��
		//����Ϊ�յ�span����spanlist��ǰ�档
		if (span->_objlist != nullptr)
		{
			spanlist->Erase(span);
			spanlist->PushFront(span);
		}
		//��start�������span��_objlist����������֮��
		NEXT_OBJ(start) = span->_objlist;
		span->_objlist = start;
		span->_usecount--;

		//������span���۲쵱ǰspan��_usecount�Ƿ����0����Ϊ0����span�ͷŻص�PageCache���кϲ�
		if (span->_usecount == 0)
		{
			spanlist->Erase(span);

			//span�����Ҷ���ص���ҳ

			span->_objlist = nullptr;
			span->_objsize = 0;
			span->_prev = nullptr;
			span->_next = nullptr;

			PageCache::GetInstance()->ReleaseSpanToPageCache(span);
		}
		

		start = next;
	}
}
