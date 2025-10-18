#ifndef SHARED_MUTEX_HPP
#define SHARED_MUTEX_HPP
#include <cstddef>
#include <limits>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <chrono>
#include <list>
#include <algorithm>
#include <optional>

namespace archive{

class shared_mutex{
	class queue{
		std::list<std::pair<std::condition_variable&, size_t>> q;
		std::mutex mtx;
		std::atomic<size_t> rm;
		public:
		queue()=default;
		void push(std::condition_variable& cv, size_t id){
			std::lock_guard<std::mutex> lock(mtx);
			q.emplace_back(cv, id);
		}
		void done(){
			// ここでlockを取るのでpreempt*中はdoneできない
			// preempt*はqueueのすべてのmutexをlockする．
			std::lock_guard<std::mutex> lock(mtx);
			rm++;
		}
		bool empty(){
			std::lock_guard<std::mutex> lock(mtx);
			return q.empty();
		}
		void lock(){ mtx.lock(); }
		// these functions under here are used under locked state
		std::condition_variable* pop(){
			// popはpreempt*が管理するのですでにlockされている
			if(q.empty()) return nullptr;
			auto&[cv, id]=q.front();
			cv.notify_one();
			q.pop_front();
			return &cv;
		}
		void unlock(){ mtx.unlock(); }
		void collect(){
			rm--; // これはすでにロックされているpreempt*からしか呼ばれない
		}
		size_t size(){
			return q.size()+rm.load();
		}
		size_t size_without_rm()const{
			return q.size();
		}
		size_t size_rm()const{
			return rm.load();
		}
		size_t id()const{
			if(q.empty()) return std::numeric_limits<size_t>::max();
			return q.front().second;
		}
	};
  queue task_r, task_w, task_iw;
	std::thread preempt_thread;
	std::condition_variable preempt_cv;
	std::condition_variable* iw_cv;
	std::atomic<size_t> topId, curId;
	size_t reading; // これはpreempt*からしか更新されないのでシングルスレッド
	enum class type{
		idle=0, read=1, write=2, iw=3, iw_read=4
	} current_type;
	
	// preempt* functions run in only one thread
	void preempt(){
		std::mutex mtx;
		while(true){
			std::unique_lock<std::mutex> lock(mtx);
			preempt_cv.wait(lock, [this]{
				return !task_r.empty() || !task_w.empty() || !task_iw.empty();
			});
			// この3つのlockはそれぞれ独立に1個ずつ取られることはあるが，2つ以上とられることがないのでデッドロックは発生しない．
			task_r.lock(), task_w.lock(), task_iw.lock();
			while(task_r.size() || task_w.size()|| task_iw.size()){
				switch(current_type){
					case type::idle: preempt_idle_under_locked(); break;
					case type::read: preempt_read_under_locked();break;
					case type::write: preempt_write_under_locked();break;
					case type::iw: preempt_iw_under_locked();break;
					case type::iw_read: preempt_iw_read_under_locked();break;
				}
			}
			task_iw.unlock(), task_w.unlock(), task_r.unlock();
		}
	}

	void preempt_idle_under_locked() {
		if(task_w.size_rm()) task_w.collect();
		if(task_r.size_rm()) task_r.collect();
		if(task_iw.size_rm()) task_iw.collect();
		size_t wid=task_w.id(), rid=task_r.id(), iwid=task_iw.id();
		size_t minofid=std::min({wid,rid,iwid});
		// std::numeric_limits<>::max()なら空なのでreturn
		if(minofid==std::numeric_limits<size_t>::max()) return;
		if(wid==minofid){
			current_type = type::write;
			task_w.pop();
		}else if (rid==minofid){
			current_type = type::read;
			++reading;
			task_r.pop();
		}else{ // if (iwid==minofid)
			if(iw_cv=task_iw.pop())
				current_type = type::iw;
		}
	}
	bool preempt_read_before_write(){
		if(task_w.size_without_rm()&&task_w.id()<task_r.id()) return false;
		return true;
	}
	void preempt_read_under_locked(){
		if(task_r.size_rm()){
			task_r.collect();
			--reading;
			if(reading==0)
				current_type=type::idle;
		}
		if(task_r.size_without_rm()){
			if(!preempt_read_before_write()) return;
			++reading;
			task_r.pop();
		}
	}
	void preempt_write_under_locked(){
		if(task_w.size_rm()){
			task_w.collect();
			current_type=type::idle;
		}
	}
	void preempt_iw_under_locked(){
		if(task_iw.size_rm()){
			task_iw.collect();
			current_type=type::idle;
		}
		if(task_r.size_without_rm()){
			if(!preempt_read_before_write()) return;
			iw_cv->notify_one();
			// iw_cvはタスクが中断を知るための条件変数
			// ここで，中断要求をnotifyする
			{ // 同じ条件変数に対して中断要求の受理を送り返してくるのでwaitする
				// このとき受理を返すのはweak_lockではなくweak_lock(cv)を呼び出したスレッドである．
				std::mutex mtx;
				std::unique_lock<std::mutex> lock(mtx);
				iw_cv->wait(lock);
			}
			++reading;
			current_type=type::iw_read;
			task_r.pop();
		}
	}
	void preempt_iw_read_under_locked(){
		if(task_r.size_rm()){
			task_r.collect();
			--reading;
			if(reading==0)
				current_type=type::iw;
		}
		if(task_r.size_without_rm()){
			++reading;
			task_r.pop();
		}
	}

  public:
  void lock(){
		std::mutex mtx;
		std::unique_lock<std::mutex> lock(mtx);
		std::condition_variable cv;
		task_w.push(cv, topId.fetch_add(1, std::memory_order_relaxed));
		preempt_cv.notify_one();
		cv.wait(lock);
  }

  void lock_shared(){
		std::mutex mtx;
		std::unique_lock<std::mutex> lock(mtx);
		std::condition_variable cv;
		task_r.push(cv, topId.fetch_add(1, std::memory_order_relaxed));
		preempt_cv.notify_one();
		cv.wait(lock);
  }
  
  void weak_lock(std::condition_variable& cv){
		std::mutex mtx;
		std::unique_lock<std::mutex> lock(mtx);
		task_iw.push(cv, topId.fetch_add(1, std::memory_order_relaxed));
		preempt_cv.notify_one();
		cv.wait(lock);
  }

  void unlock(){ task_w.done(); }
  void unlock_shared(){ task_r.done(); }
  void weak_unlock(){ task_iw.done(); }
  shared_mutex()
	:task_r(), task_w(), task_iw(),
		preempt_thread(&shared_mutex::preempt, this),
		current_type(type::idle),
		topId(0), curId(0), reading(0){}
  shared_mutex(const shared_mutex&)=delete;
  shared_mutex(shared_mutex&&)=delete;
  shared_mutex& operator=(const shared_mutex&)=delete;
  shared_mutex& operator=(shared_mutex&&)=delete;

};

} // namespace archive
#endif