#pragma once
#include <memory>
#include <mutex>

#define PAGE_SIZE 4096

template<class T, class Allocator = std::allocator<T>>
class rbqueue {
public:
	rbqueue(Allocator alloc = std::allocator<T>()) : alloc_{ alloc } {
		ring_buf_ = alloc_.allocate(PAGE_SIZE / sizeof(T));
		size_ = PAGE_SIZE / sizeof(T);
	}

	rbqueue(const rbqueue% other) {

	}

	void operator= (const rbqueue% other) {

	}

	rbqueue(rbqueue&& other) {

	}

	~rbqueue() {
		int i = 0;
		while (!user_.compare_exchange_weak(i, -2)) {}

		uint64_t i = 0;
		do {
			uint64_t id = head_.fetch_add(1) % size_;
			std::destroy_at(&ring_buf_[id]);
			i++;
		} while (!(head_ == tail_));

		alloc_.deallocate(ring_buf_, size_);
	}

	int enqueue(const T& data) {
		if (user == -2) {
			return -2;
		}

		while (user_ == -1) {}

		if ((head_.load() % size_) == (tail_.load() % size_) && tail_ != head_) {
			int i = 0;
			while (!user_.compare_exchange_weak(i, -1)) {}

			if ((head_.load() % size_) != (tail_.load() % size_)) {}
			else {
				//realloc
				uint64_t tsize = sizeof(T);
				uint64_t mod = size_ * tsize % PAGE_SIZE;
				uint64_t div = size_ * tsize / PAGE_SIZE;
				uint64_t new_size = (mod == 0) ? ((div + 1) * size_) : ((div + 2) * PAGE_SIZE / tsize);
				T* new_rb = alloc_.allocate(new_size);

				uint64_t i = 0;
				do {
					uint64_t id = head_.fetch_add(1) % size_;
					new_rb[i] = ring_buf_[id];
					std::destroy_at(&ring_buf_[id]);
					i++;
				} while (!(head_ == tail_));

				head_ = 0;
				tail_ = size_;
				alloc_.deallocate(ring_buf_, size_);
				ring_buf_ = new_rb;
				size_ = new_size;
			}
			user_++;
		}

		user_++;

		ring_buf_[tail_.fetch_add(1) % size_] = data;

		user_--;
		return 0;
	}

	T dequeue() {
		if (user == -2) {
			return -2;
		}

		while (user_ == -1) {}

		if (head_ == tail_) {
			return NULL;
		}

		user_++;

		T ret = ring_buf_[head_.fetch_add(1) % size_];

		user_--;
		return ret;
	}

	int operator<< (const T& data) {
		if (user == -2) {
			return -2;
		}

		return enqueue(data);
	}



	uint64_t size() { return (tail_ - head_ + 1); }
	uint64_t volume() { return size_; }
private:
	T* ring_buf_ = nullptr;

	Allocator alloc_;

	std::atomic<int> user_ = 0;
	std::atomic<uint64_t> tail_ = 0;
	std::atomic<uint64_t> head_ = 0;
	uint64_t size_;
};