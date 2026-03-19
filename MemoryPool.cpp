#include "MemoryPool.h"

inline size_t AlignUp(size_t size, size_t align){
    return (size + align - 1) & ~(align - 1);
}

SpanList::SpanList(){
    head_ = new Span();
    tail_ = new Span();
    head_->next = tail_;
    tail_->prev = head_;
}

void SpanList::PushBack(Span* span){
    span->prev = tail_->prev;
    span->next = tail_;
    tail_->prev->next = span;
    tail_->prev = span;
}

void SpanList::RemoveSpan(Span* span){
    span->prev->next = span->next;
    span->next->prev = span->prev;
    span->prev = nullptr;
    span->next = nullptr;
}

Span* SpanList::PopFront(){
    //SpanList为空
    if(head_->next = tail_) return nullptr;
    Span* span = head_->next;
    RemoveSpan(span);
    return span;
}

bool SpanList::Empty() const{
    return head_->next == tail_;
}

PageCache::PageCache(){
#ifdef _WIN32
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    page_size_ = si.dwPageSize;
#else
    page_size_ = sysconf(_SC_PAGESIZE);
#endif
}

PageCache& PageCache::GetInstance(){
    static PageCache instance;
    return instance;
}

Span* PageCache::AllocatePages(size_t n){
    if(n == 0 || n > kMaxPageCount){
        throw std::bad_alloc();
    }
    std::lock_guard<std::mutex> lock(mutex_);
    //1.查找对应页数的span
    if(!span_lists_[n].Empty()){
        Span* span = span_lists_[n].PopFront();
        for(size_t i = 0; i < span->page_count; i++){
            page_id_to_span_[span->page_id + i] = span;
        }
        span->is_used = true;
        return span;
    }
    //2.查找更大的span，分割使用
    for(size_t i = n + 1; i <= kMaxPageCount; i++){
        if(!span_lists_[i].Empty()){
            Span* bigSpan = span_lists_[i].PopFront();
            Span* smallSpan = new Span();
            smallSpan->page_id = bigSpan->page_id;
            smallSpan->page_count = n;
            smallSpan->is_used = true;
            //剩余Span
            bigSpan->page_id += n;
            bigSpan->page_count -= n;
            span_lists_[bigSpan->page_count].PushBack(bigSpan);
            //记录页号映射
            for(size_t i = 0; i < smallSpan->page_count; i++){
                page_id_to_span_[smallSpan->page_id + i] = smallSpan;
            }
            return smallSpan;
        }
    }
    //3.请求分配n页span
    // size_t total_size = n * page_size_;
    void* ptr = SystemAllocate(n);
    if(!ptr) throw std::bad_alloc();
    Span* span = new Span();
    span->page_count = n;
    //把指针地址转换成整数 reinterpret_cast
    span->page_id = reinterpret_cast<size_t>(ptr) / page_size_;
    span->is_used = true;
    for(size_t i = 0; i < n; i++){
        page_id_to_span_[span->page_id + i] = span;
    }
    return span;
}

void PageCache::DeallocatePages(Span* span){
    if(!span || !span->is_used) return;
    std::lock_guard<std::mutex> lock(mutex_);
    span->is_used = false;
    span->free_count = 0;
    span->free_list = nullptr;
    //合并相邻的span
    mergeSpan(span);
    //插入对应的span_list
    span_lists_[span->page_count].PushBack(span);
    //移除页号映射
    for(size_t i = 0; i < span->page_count; i++){
        page_id_to_span_.erase(span->page_id + i);
    }
}

void* PageCache::SystemAllocate(size_t n){
    size_t total_size = n * page_size_;
#ifdef _WIN32
    return VirtualAlloc(nullptr, total_size，MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
    return mmap(nullptr, total_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif
}

void PageCache::SystemDeallocate(void* ptr, size_t n){
    size_t total_size = n * page_size_;
#ifdef _WIN32
    VirtualFree(ptr, 0, MEM_RELEASE);
#else
    munmap(ptr, total_size);
#endif
}

void PageCache::mergeSpan(Span* span){
    //合并前一个span
    size_t prev_page_tail = span->page_id - 1;
    auto prev_page_it = page_id_to_span_.find(prev_page_tail);
    if(prev_page_it != page_id_to_span_.end()){
        Span* prev_span = prev_page_it->second;
        if(!prev_span->is_used && prev_span->page_id + prev_span->page_count == span->page_id){
            span_lists_[prev_span->page_count].RemoveSpan(prev_span);
            prev_span->page_count += span->page_count;
            delete span;
            span = prev_span;
            for(size_t i = 0; i < span->page_count; i++){
                page_id_to_span_[i + span->page_id] = span;
            }
        }
    }
    //合并后一个span
    size_t next_page_tail = span->page_id + span->page_count;
    auto next_page_it = page_id_to_span_.find(next_page_tail);
    if(next_page_it != page_id_to_span_.end()){
        Span* next_span = next_page_it->second;
        if(!next_span->is_used && span->page_id + span->page_count == next_span->page_id){
            span_lists_->RemoveSpan(next_span);
            span->page_count += next_span->page_count;
            delete next_span;
            for(size_t i = 0; i < span->page_count; i++){
                page_id_to_span_[i + span->page_id] = span;
            }
        }
    }
}

size_t PageCache::GetPageSize(){
    return page_size_;
}

CentralCache& CentralCache::GetInstance(){
    static CentralCache instance;
    return instance;
}

void* CentralCache::AllocateBatch(size_t size, size_t batch_nums){
    if(size == 0 || batch_nums == 0 || size > kMaxSmallObjSize){
        return nullptr;
    }
    size_t align_size = AlignUp(size, kAlign);
    size_t index = (align_size / kAlign) - 1;
    std::lock_guard<std::mutex> lock(mutexes_[index]);
    Span* span = nullptr;
    for(Span* cur = span_lists_[index].head_->next; cur != span_lists_[index].tail_; cur = cur->next){
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
        char* start = reinterpret_cast<char*>(newSpan->page_id * PageCache::GetInstance().GetPageSize());
        char* end = start + newSpan->page_count * PageCache::GetInstance().GetPageSize();
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
    for(size_t i = 0; i < actual_num; i++){
        end = *reinterpret_cast<void**>(end);
    }
    span->free_list = *reinterpret_cast<void**>(end);
    *reinterpret_cast<void**>(end) = nullptr;
    span->free_count -= actual_num;
    return start;
}