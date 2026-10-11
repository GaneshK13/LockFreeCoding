#include <atomic>
#include <cstdint>
#include <new>
#include <array>

template <typename T, std::size_t Capacity>
class CacheAlignedRingBuffer
{
	static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");
	
	alignas(std::hardware_destructive_interference_size) std::atomic<std::size_t> head_{0};
	alignas(std::hardware_destructive_interference_size) std::atomic<std::size_t> tail_{0};
	
	std::array<T, Capacity> buffer_{};
	
	bool push(const T& item) noexcept
	{
		const auto tail = tail_.load(std::memory_order_relaxed);
		const auto next = (tail + 1) & (Capacity - 1);
		
		if (next == head_.load(std::memory_order_acquire))
			return false;
		
		buffer_[tail] = item;
		tail_.store(next, std::memory_order_release);
		return true;
	}
	
	bool pop(T& item) noexcept
	{
		const auto head = head_.load(std::memory_order_relaxed);
		
		if (head == tail_.load(std::memory_order_acquire))
			return false;
		
		item = buffer_[head];
		head_.store((head + 1) & (Capacity - 1), std::memory_order_release);
		return true;
	}
};