#pragma once
#include <cstdlib>

struct Span{
    size_t page_id; 
    size_t page_count;
    size_t block_size;
    size_t free_count;
    void* free_list; //当前可分配空闲块链表的头
    void* startAddr;
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
    //工具函数AlignUp
    inline static size_t AlignUp(size_t size, size_t Align){
        return (size + Align - 1) & ~(Align - 1);
    }
    //接口
    Span* GetHead() const;
    Span* GetTail() const;
private:
    Span* head_;
    Span* tail_;
};