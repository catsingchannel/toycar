#pragma once
#include <memory>
#include <mutex>
#include <optional>
#include <cstdlib>

#define PAGE_SIZE 4096
#define STATUS_RESIZE -1
#define STATUS_DESTROY -2
#define STATUS_COPY -3

template<class T, class Allocator = std::allocator<T>>
class rbqueue {
public:
	rbqueue(Allocator alloc = std::allocator<T>()) : alloc_{ alloc } {
		ring_buf_ = alloc_.allocate(PAGE_SIZE / sizeof(T));
		size_ = PAGE_SIZE / sizeof(T);
	}

	rbqueue(const rbqueue& other) {

	}

	void operator= (const rbqueue& other) {

	}

	rbqueue(rbqueue&& other) {

	}

	~rbqueue() {
		int i = 0;
		while (!user_.compare_exchange_weak(i, STATUS_DESTROY)) {}

		uint64_t j = 0;
		do {
			uint64_t id = head_.fetch_add(1) % size_;
			
			j++;
		} while (!(head_ == tail_));

		alloc_.deallocate(ring_buf_, size_);
	}

	int enqueue(const T& data);

	std::optional<T> dequeue();

	int operator<< (const T& data);

	uint64_t size() { return (tail_ - head_); }
	uint64_t volume() { return size_; }
private:
	T* ring_buf_ = nullptr;

	Allocator alloc_;

	std::atomic<int> user_ = 0;
	std::atomic<int> flag_ = 0;
	std::atomic<uint64_t> tail_ = 0;
	std::atomic<uint64_t> head_ = 0;
	uint64_t size_;

	void resize();
};

template<class T, class Allocator>
int rbqueue<T, Allocator>::enqueue(const T& data) {
	user_++;

	if (flag_ == STATUS_DESTROY) {
		user_--;
		return STATUS_DESTROY;
	}

	if (flag_ == STATUS_RESIZE || flag_ == STATUS_COPY) {
		user_--;
		while (flag_ == STATUS_RESIZE || flag_ == STATUS_COPY) {}
		user_++;
	}

	if (volume() - size() <= (PAGE_SIZE / sizeof(T) * 0.1) && tail_ != head_) {
		resize();
	}

	ring_buf_[tail_.fetch_add(1) % size_] = data;

	user_--;
	return 0;
}

template<class T, class Allocator>
std::optional<T> rbqueue<T, Allocator>::dequeue() {
	user_++;

	if (flag_ == STATUS_DESTROY) {
		user_--;
		return std::nullopt;
	}

	if (flag_ == STATUS_RESIZE || flag_ == STATUS_COPY) {
		user_--;
		while (flag_ == STATUS_RESIZE || flag_ == STATUS_COPY) {}
		user_++;
	}

	if (size() <= 0) {
		user_--;
		//cout << this_thread::get_id() << " exiting because depletion" << endl;
		return std::nullopt;
	}

	std::optional<T> value(ring_buf_[head_.fetch_add(1) % size_]);

	user_--;
	return value;
}

template<class T, class Allocator>
int rbqueue<T, Allocator>::operator<<(const T& data) {
	return enqueue(data);
}

template<class T, class Allocator>
void rbqueue<T, Allocator>::resize() {
	cout << this_thread::get_id() << " entering resize" << endl;
	int i = 0;
	uint64_t osize = size_;
	
	if (!flag_.compare_exchange_weak(i, STATUS_RESIZE)) {
		user_--;
		while (flag_ == STATUS_RESIZE || flag_ == STATUS_COPY) {}
		user_++;
		return;
	}

	while (user_ != 1) {}

	if (osize == size_ && (head_ != tail_)) {
		//realloc
		uint64_t tsize = sizeof(T);
		uint64_t mod = size_ * tsize % PAGE_SIZE;
		uint64_t div = size_ * tsize / PAGE_SIZE;
		uint64_t new_size = (mod == 0) ? ((div + 1) * PAGE_SIZE / tsize) : ((div + 2) * PAGE_SIZE / tsize);
		cout << this_thread::get_id() << " is reallocating at" << head_ << ":" << tail_ << ":" << size_ << ":" << new_size << ":";
		cout << head_.load() % size_ << ":" << tail_.load() % size_ << endl;
		T* new_rb = alloc_.allocate(new_size);

		uint64_t i = 0;
		do {
			uint64_t id = head_.fetch_add(1) % size_;
			new_rb[i] = ring_buf_[id];
			std::destroy_at(&ring_buf_[id]);
			i++;
		} while (!(head_ == tail_));

		head_ = 0;
		tail_ = size();
		alloc_.deallocate(ring_buf_, size_);
		ring_buf_ = new_rb;
		size_ = new_size;
	}
	flag_++;

	return;
}