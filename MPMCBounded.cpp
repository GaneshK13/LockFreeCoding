#include <atomic>
#include <optional>
#include <cstdint>
#include <memory>
#include <new>
#include <array>
#include <type_traits>


template <typename T, std::size_t Capacity>
	requires (std::is_trivially_copyable_v<T> && 
		Capacity & (Capacity - 1) == 0) && 
		Capacity >= 2)
class MPMCQueue
{
	static constexpr std::size_t kMask = Capacity - 1;
	
	struct alignas(std::hardware_destructive_interference_size) Slot
	{
		std::atomic<std::size_t> sequence{0};
		T data{};
	};
	
	alignas(std::hardware_destructive_interference_size)
		std::array<Slot, Capacity> slots_{};
	
	alignas(std::hardware_destructive_interference_size)
		std::atomic<std::size_t> head_{0};
	
	alignas(std::hardware_destructive_interference_size)
		std::atomic<std::size_t> tail_{0};
	
public:
	MPMCQueue() noexcept
	{
		for (std::size_t i = 0; i < Capacity; i++)
			slots_[i].sequence.store(i, std::memory_order_relaxed);	// note each sequence number
	}
	
	bool push(const T& item) noexcept
	{
		std::size_t tail = tail_.load(std::memory_order_relaxed);
		
		while (true)
		{
			Slot& slot = slots_[tail & kMask];
			std::size_t seq = slot.sequence.load(std::memory_order_acquire);
			std::intptr_t diff = static_cast<std::intptr_t>(seq) - static_cast<std::intptr_t>(tail);
			
			if (diff == 0)
			{
				if ( tail_.compare_exchange_weak(tail, tail + 1, std::memory_order_relaxed) )
					break;
			}
			else if (diff < 0)
				return false;
			else
				tail = tail_.load(std::memory_order_relaxed);
		}
		
		slots_[tail & kMask] = item;
		slots_[tail & kMask].sequence.store(tail + 1, std::memory_order_release);
		
		return true;
	}
	
	bool pop(T& item) noexcept
	{
		std::size_t head = head_.load(std::memory_order_relaxed);
		
		while (true)
		{
			Slot& slot = slots_[head & kMask];
			std::size_t seq = slot.sequence.load(std::memory_order_acquire);
			std::intptr_t diff = static_cast<std::intptr_t>(seq) - static_cast<std::intptr_t>(head + 1);
			
			if (diff == 0)
			{
				if ( head_.compare_exchange_weak(head, head + 1, std::memory_order_relaxed) )
					break;
			}
			else if (diff < 0)
				return false;
			else
				head = head_.load(std::memory_order_relaxed);
		}
		
		item = slots_[head & kMask].data;
		slots_[head & kMask].sequence.store(head + Capacity, std::memory_order_release);	// note: one cycle of capacity added to head
		
		return true;
	}
};