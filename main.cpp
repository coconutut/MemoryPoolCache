#include "MemoryPool.h"
#include <iostream>

namespace{
    thread_local ThreadCache tls_thread_cache;
}

class MemoryPool{
public:
    //申请size大小的内存
    void* PoolAllocate(size_t size){
        try{
            return tls_thread_cache.Allocate(size);
        }
        catch(const std::bad_alloc& e){
            std::cerr << "PoolAllocate failed: " << e.what() << std::endl;
            return nullptr;
        }
    }
    //释放size大小的内存
    void PoolDeallocate(void* start, size_t size){
        tls_thread_cache.Deallocate(start, size);
    }

    static MemoryPool& GetInstance(){
        static MemoryPool instance;
        return instance;
    }
};
//多线程测试
void TestConcurrentAlloc(){
    const int kThreadNum = 8;
    const int kAllocNum = 100000;
    std::vector<std::thread> threads;
    for(int i = 0; i < kThreadNum; i++){
        threads.emplace_back([=](){
            std::vector<void*> ptrs;
            ptrs.reserve(kAllocNum);
            //分配
            for(int j = 0; j < kAllocNum; j++){
                ptrs.push_back(MemoryPool::GetInstance().PoolAllocate(8));
                if(!ptrs.back()){
                    std::cerr << "Thread " << i << "allocate failed" << std::endl;
                    return;
                }
            }
            //释放
            for(void* ptr : ptrs){
                MemoryPool::GetInstance().PoolDeallocate(ptr, 8);
            }
        });
    }
    for(auto& thread : threads){
        thread.join();
    }
    std::cout << "Concurrent allocation test success!" << std::endl;
}
//单线程测试
void TestSingleThreadAlloc(){
    int* i = static_cast<int*>(MemoryPool::GetInstance().PoolAllocate(sizeof(int)));
    int* j = static_cast<int*>(MemoryPool::GetInstance().PoolAllocate(sizeof(int)));
    int* k = static_cast<int*>(MemoryPool::GetInstance().PoolAllocate(sizeof(int)));
    if(!i || !j || !k){
        std::cerr << "Single thread allocate failed" << std::endl;
    }
    *i = 1;
    *j = 2;
    *k = 3;
    std::cout << "i alloc: " << i << "i = " << *i << std::endl;
    std::cout << "j alloc: " << j << "i = " << *j << std::endl;
    std::cout << "k alloc: " << k << "i = " << *k << std::endl;
    MemoryPool::GetInstance().PoolDeallocate(i, sizeof(int));
    MemoryPool::GetInstance().PoolDeallocate(j, sizeof(int));
    MemoryPool::GetInstance().PoolDeallocate(k, sizeof(int));
    std::cout << "Single thread test success!" << std::endl;
}   

int main(){
    TestConcurrentAlloc();
    TestSingleThreadAlloc();
    return 0;
}