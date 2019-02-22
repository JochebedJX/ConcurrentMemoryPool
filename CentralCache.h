#pragma once

#include"ConcurrentMemoryPool.h"

class CentralCache//�����Ϊ����ģʽ������ThreadCache���ռ�ʱ���̰߳�ȫ��
{
public:
	static CentralCache* GetInstance(){
		return &_inst;
	}

	Span* GetOneSpan(SpanList* spanlist, size_t bytes);

	// �����Ļ����ȡһ�������Ķ����thread cache
	size_t FetchRangeObj(void*& start, void*& end, size_t n, size_t size);

	// ��һ�������Ķ����ͷŵ�span���
	void ReleaseListToSpans(void* start, size_t byte_size);

private:
	CentralCache() = default;//��Ĭ�ϳ�Ա����
	CentralCache(const CentralCache&) = delete;//�޷�ʵ�֣�Ҳ����ֻ������ʵ��+������˽��
	CentralCache& operator=(const CentralCache&) = delete;

	static CentralCache _inst;

private:
	// ���Ļ�����������
	SpanList _spanlist[NLISTS];//����ThreadCache
};


