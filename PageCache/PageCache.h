#pragma once
#include "SpanList/SpanList.h"
#include <mutex>
#include <unordered_map>
#include <unistd.h>
#include <sys/mman.h>

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
    //通过地址查询
    Span* FindSpanByAddr(void* start);
private:
    //向系统申请n页内存（跨平台）
    void* SystemAllocate(size_t n);
    //向系统释放n页内存（跨平台）
    void SystemDeallocate(void* ptr, size_t n);
    //合并相邻的span
    Span* mergeSpan(Span* span);
};

