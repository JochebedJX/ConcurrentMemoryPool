#pragma once

#include"ConcurrentMemoryPool.h"
//#include"CentralCache.h"

class PageCache//ͬ��Ϊ����ģʽ
{
public:
	static PageCache* GetInstance()
	{
		return &_inst;
	}

	Span* NewSpan(size_t npage);
	Span* _NewSpan(size_t npage);

	
	// ��ȡ�Ӷ���span��ӳ��
	Span* MapObjectToSpan(void* obj);
	// �ͷſ���span�ص�Pagecache�����ϲ����ڵ�span
	void ReleaseSpanToPageCache(Span* span);

	

private:
	SpanList _pagelist[NPAGES];//SpanList��������
	//map<PageID, Span*> _id_span_map;
	unordered_map<PageID, Span*> _id_span_map;
	
	mutex _mtx;

private:
	PageCache() = default;
	PageCache(const PageCache&) = delete;
	static PageCache _inst;
};
