#include "Worker.h"
#include <iostream>
#include <pthread.h>
#include <signal.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <map>
#include <memory>
#include "Utils.h"
#include "../../protocol/Parser.h"
#include "afina/execute/Command.h"

#define MAX_EVENTS 100
#define BUFFER_LENGTH 1000


namespace Afina {
namespace Network {
namespace NonBlocking {

enum ParseState{NEW,BODY,GARBAGE};

struct ParseTool{
    size_t totallen,inputlen,processedlen,parsedlen,currparsedlen;
    uint32_t bodylen;
    ParseState state;
    std::string reserve;
    std::unique_ptr<Execute::Command> command;
    Afina::Protocol::Parser parser;
    ParseTool(){
        inputlen=0;
        totallen=0;
        processedlen=0;
        bodylen=0;
        parsedlen=0;
        currparsedlen=0;
        state=NEW;
        parser.Reset();
   }
};

struct WorkerData{
    Worker *worker;
    struct sockaddr_in sin;
    int sfd;
};

void* Worker::GettingStarted(void *args){
    WorkerData workerdata=(*reinterpret_cast<WorkerData*>(args));
    int sfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    sfd=workerdata.sfd;
    if (bind(sfd, (struct sockaddr *)&(workerdata.sin), sizeof(workerdata.sin)) == -1) {
        close(sfd);
        throw std::runtime_error("SOCKET BINDING ERROR");
    }
    if (listen(sfd, 10) == -1){
        close(sfd);
        throw std::runtime_error("SOCKET LISTENING ERROR");
    }
    workerdata.worker->OnRun(sfd);
}

std::string Worker::ParseString(char *buffer, ParseTool& parsetool){
    std::string result;
    parsetool.reserve.append(buffer,0,parsetool.inputlen);
    parsetool.totallen+=parsetool.inputlen;
    while (parsetool.totallen>parsetool.processedlen || parsetool.bodylen!=0){
        switch(parsetool.state){
            case NEW:
                bool is_parsed;
                try{
                    is_parsed=parsetool.parser.Parse(parsetool.reserve.substr(parsetool.totallen-parsetool.inputlen,parsetool.inputlen),parsetool.currparsedlen);
                    parsetool.parsedlen+=parsetool.currparsedlen;
                }
                catch(std::runtime_error& e){
                    result+=std::string("SERVER ERROR")+e.what()+"\r\n";
                    parsetool.state=GARBAGE;
                    continue;
                }
                if (is_parsed){
                    parsetool.command=parsetool.parser.Build(parsetool.bodylen);
                    parsetool.parser.Reset();
                    if(parsetool.bodylen==0){
                        try{
                            std::string temp;
                            parsetool.command->Execute(*pStorage,std::string(),temp);
                            result += temp+"\r\n";
                            
                        }
                        catch(std::exception &e){
                            result+=std::string("SERVER_ERROR")+e.what()+"\r\n";
                        }
                    }
                    else
                        parsetool.state=BODY;
                    parsetool.processedlen+=parsetool.parsedlen;
                    parsetool.reserve.erase(0,parsetool.parsedlen);
                    parsetool.parsedlen=0;
                }
                else{
                    parsetool.processedlen=parsetool.totallen;
               }
            break; 
            case BODY:
                if (parsetool.bodylen+2<=parsetool.reserve.length()){
                    try{
                        std::string temp;
                        parsetool.command->Execute(*pStorage,parsetool.reserve.substr(0,parsetool.bodylen),temp);
                        result +=temp+ "\r\n";
                    }
                    catch(std::exception &e){
                        result+=std::string("SERVER_ERROR")+e.what()+"\r\n";
                    }
                    parsetool.state=NEW;
                    parsetool.reserve.erase(0,parsetool.bodylen+2);
                    parsetool.processedlen+=parsetool.bodylen+2;
                    parsetool.bodylen=0;
                }
                break;  
            case GARBAGE:
                if (parsetool.reserve[0]=='\n')
                    parsetool.state=NEW;
                parsetool.processedlen+=1;
                parsetool.reserve.erase(0,1);
                break;
        }
   }
   return result;
}
            
                
                


// See Worker.h
Worker::Worker(std::shared_ptr<Afina::Storage> ps):pStorage(ps),stop(true){
    // TODO: implementation here
}

// See Worker.h
Worker::~Worker(){
    // TODO: implementation here
}

// See Worker.h
void Worker::Start(int sfd) {
    struct sockaddr_in sin;
    socklen_t len = sizeof(sin);
    if (getsockname(sfd, (struct sockaddr *)&sin, &len) == -1)
        throw std::runtime_error("OBTAINING SOCKET NAME ERROR");
    pthread_t worker_thread;
    WorkerData *workerdata=new WorkerData();
    workerdata->sin=sin;
    workerdata->worker=this;
    workerdata->sfd=sfd;
    if ((thread=pthread_create(&worker_thread, NULL, Worker::GettingStarted, workerdata)) < 0) {
        throw std::runtime_error("THREAD CREATION ERROR");
    }
}

// See Worker.h
void Worker::Stop(){
    stop=true;
    // TODO: implementation here
}
//
// See Worker.h
void Worker::Join(){
    pthread_join(thread, 0);
}

// See Worker.h
void Worker::OnRun(int sfd){
    stop=false;
    int s,efd,eventnum;
    struct epoll_event event;
    struct epoll_event *events;
    efd = epoll_create1(0);
    if (efd == -1) {
        throw std::runtime_error("EPOLL CREATION ERROR");
    }
    event.data.fd = sfd;
    event.events = EPOLLIN | EPOLLOUT;
    s = epoll_ctl(efd, EPOLL_CTL_ADD, sfd, &event);
    if (s == -1) {
        throw std::runtime_error("EPOLL CTL ERROR");
    }
    struct epoll_event eventlist[MAX_EVENTS];
    std::map<int,ParseTool> clientbase;
    int socknum=1;
    char buffer[BUFFER_LENGTH];
    while(!stop){
        eventnum=epoll_wait(efd, eventlist, MAX_EVENTS, -1);
        for(int i=0;i<eventnum;i++){
            if(eventlist[i].data.fd==sfd){
                struct sockaddr in_addr;
                socklen_t in_len = sizeof(in_addr);
                int infd = -1;
                if((infd = accept(sfd, &in_addr, &in_len)) == -1) {
                    printf("SOCKET ACCEPTING ERROR\n");
                    close(infd);
                    break;
                }
                if (socknum == MAX_EVENTS){
                    printf("TOO MANY SOCKETS\n");
                    close(infd);
                    break;
                }
                make_socket_non_blocking(infd);
                struct epoll_event infd_event;
                infd_event.events=EPOLLIN | EPOLLOUT | EPOLLRDHUP;
                infd_event.data.fd=infd;
                if(epoll_ctl(efd, EPOLL_CTL_ADD, infd, &infd_event) == -1) {
                    close(infd);
                    printf("ADDING EVENTS TO EPOLL ERROR\n");
                    break;
                }
                clientbase[infd]=ParseTool();
                socknum++;
            }else{
                int infd=eventlist[i].data.fd;
                ParseTool &parsetool=clientbase[infd];
                if (eventlist[i].events & EPOLLIN){
                    if((parsetool.inputlen=read(infd, buffer, BUFFER_LENGTH)) >0) {
                        std::string out=ParseString(buffer,clientbase[infd]);
                        if(out.size())
                            write(infd,out.c_str(),out.size());
                    }else{
                        epoll_ctl(efd, EPOLL_CTL_DEL, infd, 0);
                        socknum--;
                        close(infd);
                        clientbase.erase(infd);
                    }
                }
                if(eventlist[i].events & EPOLLRDHUP){
                    epoll_ctl(efd, EPOLL_CTL_DEL, infd,0);
                    socknum--;
                    close(infd);
                    clientbase.erase(infd);
                }
            }
        }
    }


    for(auto it=clientbase.begin();it!=clientbase.end();it++){
        close(it->first);
    }
    close(sfd);
}

} // namespace NonBlocking
} // namespace Network
} // namespace Afinainclude "Worker.h"

