#include "SpanList/SpanList.h"

SpanList::SpanList(){
    head_ = new Span();
    tail_ = new Span();
    head_->next = tail_;
    tail_->prev = head_;
}

SpanList::~SpanList(){
    delete head_;
    delete tail_;
}

void SpanList::PushBack(Span* span){
    if(!span) return;
    span->prev = tail_->prev;
    span->next = tail_;
    tail_->prev->next = span;
    tail_->prev = span;
}

void SpanList::RemoveSpan(Span* span){
    if(!span) return;
    span->prev->next = span->next;
    span->next->prev = span->prev;
    span->prev = nullptr;
    span->next = nullptr;
}

Span* SpanList::PopFront(){
    //SpanList为空
    if(head_->next == tail_) return nullptr;
    Span* span = head_->next;
    RemoveSpan(span);
    return span;
}

bool SpanList::Empty() const{
    return head_->next == tail_;
}

Span* SpanList::GetHead() const{
    return head_;
}

Span* SpanList::GetTail() const{
    return tail_;
}