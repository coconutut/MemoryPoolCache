#include "ThreadCache/ThreadCache.h"

void* ThreadCache::Allocate(size_t size){
    if(size == 0) return nullptr;
    //大对象直接向Page Cache申请
    if(size > kMaxSmallObjectSize){
        size_t PageCount = (size + PageCache::GetInstance().GetPageSize() - 1) / PageCache::GetInstance().GetPageSize();
        Span* span = PageCache::GetInstance().AllocatePages(PageCount);
        return reinterpret_cast<void*>(span->page_id * PageCache::GetInstance().GetPageSize());
    }
    size_t align_size = SpanList::AlignUp(size, kAlign);
    size_t index = (align_size / kAlign) - 1;
    //空闲链表有块，直接分配
    if(free_lists_[index]){
        //头部node取出
        FreeListNode* node = free_lists_[index];
        free_lists_[index] = node->next;
        return node;
    }
    //向Central Cache申请批量块
    FetchFromCentralCache(index, align_size);
    //再次分配
    FreeListNode* node = free_lists_[index];
    free_lists_[index] = node->next;
    return node;
}

void ThreadCache::Deallocate(void* start, size_t size){
    if(!start || size == 0) return;
    //大对象归还给 Page Cache
    if(size > kMaxSmallObjectSize){
        Span* span = PageCache::GetInstance().FindSpanByAddr(start);
        if(span) PageCache::GetInstance().DeallocatePages(span);
        return;
    }
    size_t align_size = SpanList::AlignUp(size, kAlign);
    size_t index = (align_size / kAlign) - 1;
    //插入空闲链表
    FreeListNode* node = static_cast<FreeListNode*>(start);
    node->next = free_lists_[index];
    free_lists_[index] = node;
    //空闲链表过长，归还给Central Cache
    size_t batch_num = CalcBatchNum(align_size);
    if(GetListLength(free_lists_[index]) > batch_num * 2){
        ReleaseToCentralCache(index, align_size, batch_num);
    }
}

void ThreadCache::FetchFromCentralCache(size_t index, size_t size){
    size_t batch_num = CalcBatchNum(size);
    void* start = CentralCache::GetInstance().AllocateBatch(size, batch_num);
    if(!start) throw std::bad_alloc();
    //将批量块插入空闲链表
    FreeListNode* cur = static_cast<FreeListNode*>(start);
    FreeListNode* end = cur;
    for(size_t i = 0; i < batch_num - 1; i++){
        end = end->next;
    }
    end->next = free_lists_[index];
    free_lists_[index] = cur;
}

size_t ThreadCache::CalcBatchNum(size_t size) const{
    if(size <= 64) return 32;
    if(size <= 256) return 16;
    if(size <= 1024) return 8;
    return 4;
}

size_t ThreadCache::GetListLength(FreeListNode* node){
    size_t len = 0;
    while(node){
        len++;
        node = node->next;
    }
    return len;
}

void ThreadCache::ReleaseToCentralCache(size_t index, size_t size, size_t batch_num){
    FreeListNode* head = free_lists_[index];
    if(!head) return;

    // 从线程本地链表切下最多 batch_num 个节点
    FreeListNode* cur = head;
    FreeListNode* prev = nullptr;
    size_t n = 0;
    while(cur && n < batch_num){
        prev = cur;
        cur = cur->next;
        ++n;
    }

    if(n == 0) return;

    // 剩余节点留在线程本地
    free_lists_[index] = cur;
    // 截断要归还的链
    prev->next = nullptr;

    // 逐节点归还，避免“连续内存”假设
    FreeListNode* node = head;
    while(node){
        FreeListNode* next = node->next;
        node->next = nullptr;
        CentralCache::GetInstance().DeallocateBatch(size, node, 1);
        node = next;
    }
}