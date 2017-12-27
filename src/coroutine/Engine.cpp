#include <afina/coroutine/Engine.h>

#include <setjmp.h>
#include <stdio.h>
#include <string.h>

namespace Afina {
namespace Coroutine {


void Engine::Store(context &ctx) {
    char test;
    char* where=&test;
    uint32_t len=StackBottom-where;
    if (std::get<0>(ctx.Stack)!=nullptr)
	 delete [](std::get<0>(ctx.Stack));
    std::get<0>(ctx.Stack)=new char[len];
    std::get<1>(ctx.Stack)=len;
    memcpy(std::get<0>(ctx.Stack),where,len);
}


void Engine::Restore(context &ctx) {
    char test;
    char* where=&test;
    if (where+std::get<1>(ctx.Stack)>StackBottom){
        Restore(ctx);
    }
    memcpy(StackBottom - std::get<1>(ctx.Stack),std::get<0>(ctx.Stack), std::get<1>(ctx.Stack));
    longjmp(ctx.Environment,1);
}

void Engine::yield() {
    context* routine=alive;
    if (routine!=nullptr){
        this->alive=routine->next;
        sched(routine);
  } 
}

void Engine::sched(void *routine_) {
    if (routine_==nullptr || routine_==cur_routine)
	return;
    if (cur_routine==nullptr){
        cur_routine=static_cast<context *>(routine_);
        Restore(*cur_routine);
    }
    if (setjmp(cur_routine->Environment)>0){
        return;
    }
    else{
       Store(*cur_routine);
       cur_routine=static_cast<context *>(routine_);
       Restore(*cur_routine);
    }
}

} // namespace Coroutine
} // namespace Afina