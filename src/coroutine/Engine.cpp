#include <afina/coroutine/Engine.h>

#include <setjmp.h>
#include <stdio.h>
#include <string.h>

namespace Afina {
namespace Coroutine {


void Engine::Store(context &ctx) {
    char test;
    char* StackTop=&test;
    ctx.Low = std::min(StackTop,StackBottom);
    ctx.Hight = std::max(StackTop,StackBottom);
    uint32_t len=ctx.Hight-ctx.Low;
    if (std::get<0>(ctx.Stack)!=nullptr)
       delete[] std::get<0>(ctx.Stack);
    std::get<0>(ctx.Stack)=new char[len];
    std::get<1>(ctx.Stack)=len;
    memcpy(std::get<0>(ctx.Stack),ctx.Low,len);
}


void Engine::Restore(context &ctx) {
    char test;
    volatile char* StackTop=&test;
    if ((ctx.Low<StackTop) && (StackTop<ctx.Hight))
        Restore(ctx);
    memcpy(ctx.Low,std::get<0>(ctx.Stack),std::get<1>(ctx.Stack));
    longjmp(ctx.Environment,1);
}

void Engine::yield() {
    context* candidate=alive;
    if(candidate==cur_routine && candidate!=nullptr){ 
        candidate=candidate->next;
    }
    if (candidate!=nullptr){
        if(cur_routine!=nullptr){
            Store(*cur_routine);
            if(setjmp(cur_routine->Environment)!=0){
                return;
            }
        }
        cur_routine=candidate;
        Restore(*cur_routine);
    }
}

void Engine::sched(void *routine_) {
    if(routine_==nullptr || routine_==cur_routine) return;
    if(cur_routine!=nullptr){
        if (setjmp(cur_routine->Environment)>0){
            return;
        }
        else
            Store(*cur_routine);
    }
    cur_routine=static_cast<context *>(routine_);
    Restore(*cur_routine);
}

} // namespace Coroutine
} // namespace Afina
