#include <atomic>
#include <optional>
#include <cstdint>
#include <memory>
#include <new>
#include <array>
#include <type_traits>

#ifdef __x86_64__
#include <immintrin.h>
#endif


template <typename T, std::size_t Capacity>
	requires (std::is_trivially_copyable_v<T> && 
		Capacity & (Capacity - 1) == 0))
class BlockingSPSC
{
	static constexpr std::size_t kMask = Capacity - 1;
	
	alignas(std::hardware_destructive_interference_size)
		std::atomic<std::size_t> head_{0};
	alignas(std::hardware_destructive_interference_size)
		std::atomic<std::size_t> tail_{0};
	alignas(std::hardware_destructive_interference_size)
		std::array<T, Capacity> buffer_{};
	
	/*
		signals the processor that the current loop is spin wait
		, allowing it to reduce power consumption
		and avoid pipeline flushes
		and avoid saturating the memory bus with repeated cache line reads
	*/
	static void spin_hint() noexcept
	{
		#ifdef __x86_64__
			_mm_pause();
		#elif defined (__aarch64__)
			asm volatile("yield" ::: "memory");
		#endif
	};
public:
	void push_blocking(const T& item) noexcept
	{
		const std::size_t tail = tail_.load(std::memory_order_relaxed);
		const std::size_t next_tail = (tail + 1) & kMask;
		
		if (next_tail == head_.load(std::memory_order_acquire))	//HERE
			spin_hint();
		
		buffer_[tail] = item;
		tail_.store(next_tail, std::memory_order_release);
	}
	
	void pop_blocking(T& item) noexcept
	{
		const std::size_t head = head_.load(std::memory_order_relaxed);
		if (head == tail_.load(std::memory_order_acquire))	//HERE
			spin_hint();
		
		item = buffer[head];
		const std::size_t next_head = (head + 1) & kMask;
		head_.store(next_head, std::memory_order_release);
	}
	
	bool empty() const noexcept
	{
		return head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_acquire);
	}
	
	std::size_t size() const noexcept
	{
		const std::size_t head = head_.load(std::memory_order_acquire);
		const std::size_t tail = tail_.load(std::memory_order_acquire);
		return (tail - head + Capacity) & kMask;
	}
};