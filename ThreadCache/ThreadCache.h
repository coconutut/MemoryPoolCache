#pragma once
#include "SpanList/SpanList.h"
#include "PageCache/PageCache.h"
#include "CentralCache/CentralCache.h"

class ThreadCache{
private:
    static const size_t kAlign = 8;
    static const size_t kMaxSmallObjectSize = 4096;
    static const size_t kFreeListCount = kMaxSmallObjectSize / kAlign;
    //空闲链表节点（嵌入式）
    struct FreeListNode{
        FreeListNode* next;
    };
    //空闲链表数组
    FreeListNode* free_lists_[kFreeListCount] = {nullptr};
public:
    //分配size大小的内存
    void* Allocate(size_t size);
    //释放size大小的内存
    void Deallocate(void* start, size_t size);
private:
    //从中心缓存区申请size大小的空间
    void FetchFromCentralCache(size_t index, size_t size);
    //根据申请size的大小返回申请的数量
    size_t CalcBatchNum(size_t size) const;
    //获取free_lists_下的空闲数量
    size_t GetListLength(FreeListNode* head);
    //将batch_num数量的size空间返回给CentralCache
    void ReleaseToCentralCache(size_t index, size_t size, size_t batch_num);
};