#ifndef __THREAD_CACHE_H__
#define __THREAD_CACHE_H__

#include"ConcurrentMemoryPool.h"

class ThreadCache{
public:
	//�������󿪿ռ�
	void *Allocate(size_t size);
	
	//�ͷſռ��CentralCache
	void *Deallocate(void* ptr, size_t size);
	void ListTooLong(FreeList* freelist, size_t byte);
	
	// �����Ļ����ȡ����
	void* FetchFromCentralCache(size_t index, size_t size);
private:
	FreeList _freelist[NLISTS];
};

//ƽ̨���ܲ����ݣ���ʹ�ö�̬�ģ�ʹ��ÿ���̵߳��ӿڵķ�ʽ��
static _declspec(thread) ThreadCache* tls_threadcache = nullptr;//����ÿ���̱߳��ص�ThreadCache��ָ��,ÿ���̶߳��е�ȫ�ֱ���,ÿ���̵߳�tls�����໥��Ӱ��

#endif //__THREAD_CACHE_H__
