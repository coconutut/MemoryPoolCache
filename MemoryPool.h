#pragma once
#include <iostream>
#include <cstdlib>
#include <cstddef>
#include <mutex>
#include <thread>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#else 
#include <sys/mman.h>
#include <unistd.h>
#endif

struct Span{
    size_t page_id; 
    size_t page_count;
    size_t block_size;
    size_t free_count;
    void* free_list; //当前可分配空闲块链表的头
    Span* prev;
    Span* next;
    bool is_used;
    Span() : page_id(0), page_count(0), block_size(0), free_count(0), 
             free_list(nullptr), prev(nullptr), next(nullptr), is_used(false){};
};

class SpanList{
public:
    SpanList();
    ~SpanList();
    //添加一个span
    void PushBack(Span* span);
    //移除并返回头部span
    Span* PopFront();
    //移除一个span
    void RemoveSpan(Span* span);
    //判空
    bool Empty() const;

    Span* head_;
    Span* tail_;
};

class PageCache{
private:
    static const size_t kMaxPageCount = 128;
    SpanList span_lists_[kMaxPageCount + 1]; //SpanList的数组
    std::mutex mutex_; //全局锁
    size_t page_size_; //系统页大小
    std::unordered_map<size_t, Span*> page_id_to_span_; //页号到Span映射
    PageCache(); //跨平台获取页大小

    //显性禁止赋值、拷贝构造
    PageCache& operator=(const PageCache&) = delete;
    PageCache(const PageCache&) = delete;

public:
    //单例模式
    static PageCache& GetInstance();
    //申请n页内存
    Span* AllocatePages(size_t n);
    //释放n页内存
    void DeallocatePages(Span* span);
    //获取page_size_
    size_t GetPageSize();
private:
    //向系统申请n页内存（跨平台）
    void* SystemAllocate(size_t n);
    //向系统释放n页内存（跨平台）
    void SystemDeallocate(void* ptr, size_t n);
    //合并相邻的span
    void mergeSpan(Span* span);
};

class CentralCache{
private:
    static const size_t kAlign = 8;
    static const size_t kMaxSmallObjSize = 4096;
    static const size_t kFreeListCount = kMaxSmallObjSize / kAlign;
    SpanList span_lists_[kFreeListCount];
    std::mutex mutexes_[kFreeListCount];
    CentralCache() = default;
    CentralCache& operator=(const CentralCache&) = delete;
    CentralCache(const CentralCache&) = delete;
public:
    //单例模式
    static CentralCache& GetInstance();
    //批量申请内存块
    void* AllocateBatch(size_t size, size_t batch_num);
};