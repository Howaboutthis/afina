#include "MapBasedGlobalLockImpl.h"
using namespace std;

#include <mutex>

namespace Afina {
namespace Backend {

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::Put(const std::string &key, const std::string &value) {
    std::unique_lock<std::mutex> guard(_lock);
	if(data.find(key)!=data.end()){
		if(Delete(key))
			return PutIfAbsent(key,value);
		else
			return false;
	}
	else
		return PutIfAbsent(key,value);
}

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::PutIfAbsent(const std::string &key, const std::string &value){
    std::unique_lock<std::mutex> guard(_lock);
	if(data.find(key)!=data.end())
		return false;
	if (curr_size>_max_size)
		Delete(oldest);
	val newval;
	newval.item=value;
	if (0!=curr_size)
		newval.prev=youngest;
	else
		newval.prev.clear();
	if (data.insert( pair<string,val>(key,newval)).second){
		if(data.find(youngest)!=data.end())
			data.find(youngest)->second.next=key;
		if (0==curr_size)
			oldest=key;
		youngest=key;
		curr_size++;
		return true;
	}
	else
		return false;
		
}


// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::Set(const std::string &key, const std::string &value) {
	map<string,val>::iterator pos=data.find(key);
	if (pos==data.end())
		return false;
	else
		pos->second.item=value;
}

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::Delete(const std::string &key){
    std::unique_lock<std::mutex> guard(_lock);
	map<string,val>::iterator pos=data.find(key);
	if (pos==data.end())
		return false;
	if (oldest==key){
		if (data.find(pos->second.next)!=data.end())
			oldest=data.find(pos->second.next)->first;
		else
			oldest.clear();
	}
	else{
		if (youngest==key){
			if (data.find(pos->second.prev)!=data.end())
				youngest=data.find(pos->second.prev)->first;
			else
				youngest.clear();
		}
		else{
			data.find(pos->second.prev)->second.next=pos->second.next;
			data.find(pos->second.next)->second.prev=pos->second.prev;
		}
	}
	curr_size--;
	data.erase(key);
	return true;
}


// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::Get(const std::string &key, std::string &value) const {
    std::unique_lock<std::mutex> guard(_lock);
	if (data.find(key)==data.end())
		return false;
	else{
		value=(data.find(key)->second.item);
		return true;
	}
}

} // namespace Backend
} // namespace Afina
