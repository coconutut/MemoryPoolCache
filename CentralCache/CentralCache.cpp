#include "CentralCache/CentralCache.h"

CentralCache& CentralCache::GetInstance(){
    static CentralCache instance;
    return instance;
}

void* CentralCache::AllocateBatch(size_t size, size_t batch_nums){
    if(size == 0 || batch_nums == 0 || size > kMaxSmallObjSize){
        return nullptr;
    }
    size_t align_size = SpanList::AlignUp(size, kAlign);
    size_t index = (align_size / kAlign) - 1;
    std::lock_guard<std::mutex> lock(mutexes_[index]);
    Span* span = nullptr;
    //轮询查找可用Span
    for(Span* cur = span_lists_[index].GetHead()->next; cur != span_lists_[index].GetTail(); cur = cur->next){
        if(cur->free_count > 0){
            span = cur;
            break;
        }
    }
    //没有可用的span,向Page Cache申请
    if(!span){
        size_t page_count = (align_size * batch_nums + PageCache::GetInstance().GetPageSize() - 1) / (PageCache::GetInstance().GetPageSize());
        page_count = std::max(page_count, static_cast<size_t>(1));
        Span* newSpan = PageCache::GetInstance().AllocatePages(page_count);
        newSpan->block_size = align_size;
        newSpan->free_count = (page_count * PageCache::GetInstance().GetPageSize()) / align_size;
        //分割Span为小块，构建空闲链表
        char* start = reinterpret_cast<char*>(newSpan->startAddr);
        char* end = start + newSpan->page_count * PageCache::GetInstance().GetPageSize();
        newSpan->free_list = start;
        char* cur = start;
        while(cur + align_size < end){
            *reinterpret_cast<char**>(cur) = cur + align_size;
            cur += align_size;
        }
        *reinterpret_cast<char**>(cur) = nullptr;
        span_lists_[index].PushBack(newSpan);
        span = newSpan;
    }
    //从span中取出batch_num个块
    void* start = span->free_list;
    void* end = start;
    size_t actual_num = std::min(batch_nums, span->free_count);
    for(size_t i = 0; i < actual_num - 1; i++){
        end = *reinterpret_cast<void**>(end);
    }
    span->free_list = *reinterpret_cast<void**>(end);
    *reinterpret_cast<void**>(end) = nullptr;
    span->free_count -= actual_num;
    return start;
}

void CentralCache::DeallocateBatch(size_t size, void* start, size_t count){
    if(!start || size == 0 || count == 0 || size > kMaxSmallObjSize) return;

    size_t align_size = SpanList::AlignUp(size, kAlign);
    size_t index = (align_size / kAlign) - 1;
    std::lock_guard<std::mutex> lock(mutexes_[index]);

    void* node = start;
    size_t n = 0;

    while(node && n < count){
        void* next = *reinterpret_cast<void**>(node);

        Span* span = PageCache::GetInstance().FindSpanByAddr(node);
        if(span){
            *reinterpret_cast<void**>(node) = span->free_list;
            span->free_list = node;
            ++span->free_count;

            size_t total_blocks =
                (span->page_count * PageCache::GetInstance().GetPageSize()) / span->block_size;

            if(span->free_count == total_blocks){
                span_lists_[index].RemoveSpan(span);
                PageCache::GetInstance().DeallocatePages(span);
            }
        }

        node = next;
        ++n;
    }
}