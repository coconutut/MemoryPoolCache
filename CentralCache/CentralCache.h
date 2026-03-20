#pragma once
#include "SpanList/SpanList.h"
#include "PageCache/PageCache.h"
#include <mutex>

class CentralCache{
private:
    static const size_t kAlign = 8;
    static const size_t kMaxSmallObjSize = 4096;
    static const size_t kFreeListCount = kMaxSmallObjSize / kAlign;
    SpanList span_lists_[kFreeListCount];
    std::mutex mutexes_[kFreeListCount];
    //显性禁止拷贝构造、赋值构造
    CentralCache() = default;
    CentralCache& operator=(const CentralCache&) = delete;
    CentralCache(const CentralCache&) = delete;
public:
    //单例模式
    static CentralCache& GetInstance();
    //批量申请内存块
    void* AllocateBatch(size_t size, size_t batch_num);
    //批量归还内存块
    void DeallocateBatch(size_t size, void* start, size_t count);
};