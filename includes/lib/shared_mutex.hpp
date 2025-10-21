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
#include <memory>
#include <thread>

namespace archive{

class shared_mutex{
	// WaitNode: 条件変数の通知取りこぼしを防ぐための構造体
	struct WaitNode {
		std::mutex mtx;
		std::condition_variable cv;
		std::atomic<bool> ready{false};
		size_t id;
		
		WaitNode(size_t id) : id(id) {}
	};

	class queue{
		std::list<std::shared_ptr<WaitNode>> q;
		std::mutex mtx;
		std::atomic<size_t> rm;
		shared_mutex* parent; // プリエンプトスレッドを起床させるため
		
		public:
		queue(shared_mutex* parent) : parent(parent) {}
		
		void push(std::shared_ptr<WaitNode> node){
			std::lock_guard<std::mutex> lock(mtx);
			q.emplace_back(node);
		}
		
		void done(){
			// ここでlockを取るのでpreempt*中はdoneできない
			// preempt*はqueueのすべてのmutexをlockする．
			std::lock_guard<std::mutex> lock(mtx);
			rm++;
			// プリエンプトスレッドを起床させる
			if (parent) parent->wakeup_preempt();
		}
		
		bool empty(){
			std::lock_guard<std::mutex> lock(mtx);
			return q.empty();
		}
		
		bool has_work(){
			std::lock_guard<std::mutex> lock(mtx);
			return !q.empty() || rm.load() > 0;
		}
		
		void lock(){ mtx.lock(); }
		
		// these functions under here are used under locked state
		std::shared_ptr<WaitNode> pop(){
			// popはpreempt*が管理するのですでにlockされている
			if(q.empty()) return nullptr;
			auto node = q.front();
			q.pop_front();
			// 通知取りこぼしを防ぐため、readyフラグを設定してから通知
			node->ready.store(true);
			node->cv.notify_one();
			return node;
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
			return q.front()->id;
		}
	};
  queue task_r, task_iw;
	std::thread preempt_thread;
	std::condition_variable preempt_cv;
	std::mutex preempt_mtx;
	std::atomic<bool> stop_flag{false};
	
	// 中断通知メカニズム
	struct InterruptNotification {
		std::mutex mtx;
		std::condition_variable* cv; // 外部の条件変数を共有
		std::atomic<bool> interrupt_requested{false};
		std::atomic<bool> task_interrupted{false};
		std::atomic<bool> task_completed{false}; // タスクが中断前に完了したか
	};
	std::shared_ptr<InterruptNotification> current_interrupt;
	
	std::atomic<size_t> topId, curId;
	size_t reading; // これはpreempt*からしか更新されないのでシングルスレッド
	enum class type{
		idle=0, read=1, iw=2, iw_read=3
	} current_type;
	
	// プリエンプトスレッドを起床させる
	void wakeup_preempt() {
		std::lock_guard<std::mutex> lock(preempt_mtx);
		preempt_cv.notify_one();
	}
	
	// preempt* functions run in only one thread
	void preempt(){
		while(!stop_flag.load()){
			std::unique_lock<std::mutex> lock(preempt_mtx);
			preempt_cv.wait(lock, [this]{
				return stop_flag.load() || !task_r.empty() || !task_iw.empty() ||
				       task_r.size_rm() > 0 || task_iw.size_rm() > 0;
			});
			
			if (stop_flag.load()) break;
			
			// この2つのlockはそれぞれ独立に1個ずつ取られることはあるが，2つ以上とられることがないのでデッドロックは発生しない．
			task_r.lock(), task_iw.lock();
			while(task_r.size() || task_iw.size()){
				switch(current_type){
					case type::idle: preempt_idle_under_locked(); break;
					case type::read: preempt_read_under_locked();break;
					case type::iw: preempt_iw_under_locked();break;
					case type::iw_read: preempt_iw_read_under_locked();break;
				}
			}
			task_iw.unlock(), task_r.unlock();
		}
	}

	void preempt_idle_under_locked() {
		if(task_r.size_rm()) task_r.collect();
		if(task_iw.size_rm()) task_iw.collect();
		
		size_t rid=task_r.id(), iwid=task_iw.id();
		
		// READ優先の仕様を実装: READが存在する限りREADを優先
		if(rid != std::numeric_limits<size_t>::max()) {
			current_type = type::read;
			++reading;
			task_r.pop();
		} else if(iwid != std::numeric_limits<size_t>::max()) {
			auto node = task_iw.pop();
			if(node) {
				current_interrupt = std::make_shared<InterruptNotification>();
				current_type = type::iw;
			}
		}
	}
	bool preempt_read_before_iw(){
		// IWが実行中で、待ちのIWタスクが存在し、そのIDがREADのIDより小さい場合はREADをブロック
		if(current_type == type::iw && task_iw.size_without_rm() > 0 && task_iw.id() < task_r.id()) {
			return false;
		}
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
			if(!preempt_read_before_iw()) return;
			++reading;
			task_r.pop();
		}
	}
	void preempt_iw_under_locked(){
		if(task_iw.size_rm()){
			task_iw.collect();
			current_type=type::idle;
		}else if(!current_interrupt){
			current_type=type::idle;
			return;
		}else if(!current_interrupt->cv){
			return;
		}
		if(task_r.size_without_rm()){
			if(!preempt_read_before_iw()) return;
			
			// READが来たのでIWに中断要求を送信
			current_interrupt->interrupt_requested.store(true);
			current_interrupt->cv->notify_one();
			
			// タスク側からの中断通知または完了通知を待つ
			std::unique_lock<std::mutex> lock(current_interrupt->mtx);
			current_interrupt->cv->wait(lock, [this]{
				return current_interrupt->task_interrupted.load() || 
							 current_interrupt->task_completed.load();
			});
			
			// タスクが完了した場合は中断待ちを終了
			if(current_interrupt->task_completed.load()) {
				current_interrupt.reset();
				++reading;
				current_type=type::read;
				task_r.pop();
				return;
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
  // WRITE機能は常に中断可能
  void lock(std::condition_variable* interrupt_cv=nullptr){
		auto node = std::make_shared<WaitNode>(topId.fetch_add(1, std::memory_order_relaxed));
		task_iw.push(node);
		wakeup_preempt();
		
		std::unique_lock<std::mutex> lock(node->mtx);
		node->cv.wait(lock, [&]{ return node->ready.load(); });
		
		// 中断可能なタスクの実行前処理：中断通知オブジェクトを設定
		current_interrupt = std::make_shared<InterruptNotification>();
		current_interrupt->cv = interrupt_cv;
  }

  void lock_shared(){
		auto node = std::make_shared<WaitNode>(topId.fetch_add(1, std::memory_order_relaxed));
		task_r.push(node);
		wakeup_preempt();
		
		std::unique_lock<std::mutex> lock(node->mtx);
		node->cv.wait(lock, [&]{ return node->ready.load(); });
  }
  
  
  // 中断可能なタスクが中断要求を検知した時に呼ぶ
  void notify_interrupt() {
		if(current_interrupt && current_interrupt->cv) {
			current_interrupt->task_interrupted.store(true);
			current_interrupt->cv->notify_one();
		}
  }
  
  // 中断可能なタスクが中断要求をチェックする
  bool check_interrupt_request() {
		return current_interrupt && current_interrupt->interrupt_requested.load();
  }
  
  // 中断可能なタスクが完了した時に呼ぶ（中断前に完了した場合）
  void notify_completion() {
		if(current_interrupt && current_interrupt->cv) {
			current_interrupt->task_completed.store(true);
			current_interrupt->cv->notify_one();
		}
  }

  void unlock(){ 
    notify_completion();
    task_iw.done(); 
  }
  void unlock_shared(){ task_r.done(); }
  shared_mutex()
	:task_r(this), task_iw(this),
		preempt_thread(&shared_mutex::preempt, this),
		current_type(type::idle),
		topId(0), curId(0), reading(0){}
		
  ~shared_mutex() {
		stop_flag.store(true);
		wakeup_preempt();
		if(preempt_thread.joinable()) {
			preempt_thread.join();
		}
	}
	
  shared_mutex(const shared_mutex&)=delete;
  shared_mutex(shared_mutex&&)=delete;
  shared_mutex& operator=(const shared_mutex&)=delete;
  shared_mutex& operator=(shared_mutex&&)=delete;

};

} // namespace archive
#endif