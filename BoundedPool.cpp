#include <atomic>
#include <cassert>
#include <type_traits>
#include <cstdint>
#include <array>
#include <new>

template <typename T, std::size_t Capacity>
class BoundedPool
{
	static_assert(std::is_trivially_destructible_v<T>, "Pool requires trivially destructible types to guarantee bounded release time");
	
	struct alignas(std::hardware_destructive_interference_size) Slot
	{
		alignas(alignof(T)) std::byte storage[sizeof(T)];	// variable stored here
		std::atomic<bool> free{true};
	};
	
	std::array<Slot, Capacity> slots_{};	// array of variables
	std::atomic<std::size_t> hint_{0};
	
public:
	T* acquire() noexcept
	{
		const std::size_t start = hint_.load(std::memory_order_relaxed);
		
		for (std::size_t i = 0; i < Capacity; ++i)
		{
			const std::size_t idx = (start + i) & (Capacity - 1);
			Slot& slot = slots_[idx];
			
			bool expected = true;
			if ( slot.free.compare_exchange_strong(expected, false, std::memory_order_release, std::memory_order_relaxed) )
			{
				hint_.store((idx + 1) & (Capacity - 1), std::memory_order_relaxed);
				return reinterpret_cast<T*>(slot.storage);
			}
		}
		
		return nullptr;
	}
	
	void release(T* ptr) noexcept
	{
		const auto raw = reinterpret_cast<std::byte*>(ptr);
		const auto base = reinterpret_cast<std::byte*>(slots_.data());
		const std::ptrdiff_t offset = raw - base;
		const std::size_t idx = static_cast<std::size_t>(offset) / sizeof(Slot);
		
		assert(idx < Capacity);
		slots_[idx].free.store(true, std::memory_order_release);
	}
};