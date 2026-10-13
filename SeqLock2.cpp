#include <atomic>
#include <new>
#include <cstdint>

struct alignas(std::hardware_destructive_interference_size)
	SeqLock
{
	std::atomic<std::uint32_t> sequence{0};
	std::uint64_t data_a{0};
	std::uint64_t data_b{0};
	
	void write(std::uint64_t a, std::uint64_t b) noexcept
	{
		std::uint32_t seq = sequence.load(std::memory_order_relaxed);
		sequence.store(seq + 1, std::memory_order_relaxed);
		
		std::atomic_thread_fence(std::memory_order_release);
		data_a = a;
		data_b = b;
		std::atomic_thread_fence(std::memory_order_release);
		
		sequence.store(seq + 2, std::memory_order_relaxed);
	}
	
	bool read(std::uint64_t& a, std::uint64_t& b) noexcept
	{
		std::uint32_t seq1 = sequence.load(std::memory_order_acquire);
		
		if (seq1 & 1) return false;
		
		a = data_a;
		b = data_b;
		
		std::atomic_thread_fence(std::memory_order_acquire);
		
		std::uint32_t seq2 = sequence.load(std::memory_order_relaxed);
		
		return seq1 == seq2;
	}
};