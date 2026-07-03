#include <atomic>
#include <iostream>

template <typename T, size_t Capacity>
class LockFreeSPSC
{
	// For fast bitwise modulo operations in ring buffer
	static_assert(Capacity >= 2, "Capacity must be at least 2");
	static_assert(Capacity & (Capacity - 1) == 0, "Capacity must be a power of 2");
	
public:
	LockFreeSPSC() : head(0), tail(0) {}
	
	// Producer thread
	bool enqueue(const T& item)
	{
		size_t current_head = head.load(std::memory_order_relaxed);
		size_t current_tail = tail.load(std::memory_order_acquire);
		
		if (current_head - current_tail == Capacity) // Queue full
			return false;
			
		buffer[current_head & bufferMask] = item;
		// Release: ensures the item write happens before head_ is incremented
		head.store(current_head + 1, std::memory_order_release);
		
		return true;
	}
	
	// Consumer thread
	bool dequeue(T& item) // reference variable item
	{
		size_t current_head = head.load(std::memory_order_acquire);
		size_t current_tail = tail.load(std::memory_order_relaxed);
		
		if (current_head == current_tail)	// Queue Empty in the ring buffer
			return false;
			
		item = buffer[current_tail & bufferMask];
		// Release: ensures the item read happens before tail_ is incremented
		tail.store(current_tail + 1, std::memory_order_release);
		
		return true;
	}
	
private:

	static constexpr size_t bufferMask = (Capacity - 1);

	T buffer[Capacity]; // used as a ring buffer
	// Hardware cache line = 64 bytes typically
	// Prevents False Sharing
	alignas(64) std::atomic<size_t> head;
	alignas(64) std::atomic<size_t> tail;
};





/*
#include <atomic>
#include <cstddef>
#include <new>
#include <vector>
#include <optional>

template <typename T, size_t Capacity>
class LockFreeSPSC {
    // Ensure capacity is a power of two for fast bitwise modulo operations
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of 2");
    static_assert(Capacity >= 2, "Capacity must be at least 2");

public:
    LockFreeSPSC() : head_(0), tail_(0) {}

    // Producer Thread Only
    bool enqueue(const T& item) {
        const size_t current_head = head_.load(std::memory_order_relaxed);
        const size_t current_tail = tail_.load(std::memory_order_acquire); // Synchronize with dequeue

        if ((current_head - current_tail) == Capacity) {
            return false; // Queue is full
        }

        buffer_[current_head & BufferMask] = item;
        // Release: ensures the item write happens before head_ is incremented
        head_.store(current_head + 1, std::memory_order_release);
        return true;
    }

    // Consumer Thread Only
    bool dequeue(T& item) {
        const size_t current_tail = tail_.load(std::memory_order_relaxed);
        const size_t current_head = head_.load(std::memory_order_acquire); // Synchronize with enqueue

        if (current_tail == current_head) {
            return false; // Queue is empty
        }

        item = buffer_[current_tail & BufferMask];
        // Release: ensures the item read happens before tail_ is incremented
        tail_.store(current_tail + 1, std::memory_order_release);
        return true;
    }

private:
    static constexpr size_t BufferMask = Capacity - 1;
    T buffer_[Capacity];

    // Hardware cache line size is typically 64 bytes. 
    // We align atomic variables to prevent "False Sharing".
    alignas(64) std::atomic<size_t> head_;
    alignas(64) std::atomic<size_t> tail_;
};
*/