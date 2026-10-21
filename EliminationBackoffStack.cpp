#include <atomic>
#include <optional>
#include <cstdint>
#include <memory>
#include <new>
#include <array>
#include <random>

template <typename T>
class EliminationStack
{
	struct Node
	{
		T data{};
		Node* next{nullptr};
		
		explicit Node(T value) : data(std::move(value)) {}
	};
	
	static constexpr std::size_t kSlots = 16;
	static constexpr std::size_t kSpinCount = 100;
	
	enum class SlotState : std::uintptr_t
	{
		Empty = 0,
		Waiting = 1,
		Done = 2,
	};
	
	struct alignas(std::hardware_destructive_interference_size) CollisionSlot
	{
		std::atomic<Node*> node{nullptr};
		std::atomic<SlotState> state{SlotState::Empty};
	}

	alignas(std::hardware_destructive_interference_size)
		std::atomic<Node*> top_{nullptr};
		
	std::array<CollisionSlot, kSlots> arena_{};	// independently aligned slots, each on it's own cache line
	
	std::size_t random_slot() const noexcept
	{
		thread_local std::minstd_rang rng{std::random_device{}()};
		return rng() % kSlots;
	}
	
	// Empty -> Waiting
	bool try_eliminate_push(Node* node) noexcept
	{
		const std::size_t idx = random_slot();
		CollisionSlot& slot = arena_[idx];
		
		SlotState expected = SlotState::Empty;
		if ( !slot.state.compare_exchange_strong(expected, SlotState::Waiting, std::memory_order_acq_rel, std::memory_order_relaxed) )	// already used slot?
			return false;
		
		slot.node.store(node, std::memory_order_release);
		
		for (std::size_t i = 0; i < kSpinCount; ++i)	// Waiting period within which pop must happen to eliminate stack top CAS
		{
			if ( slot.state.load(std::memory_order_acquire) == SlotState::Done )	 // Pop marks Waiting -> Done
			{
				slot.state.store(SlotState::Empty, std::memory_order_release);		// Release the slot
				return true;
			}
		}
		
		expected = SlotState::Waiting;
		if ( slot.state.compare_exchange_strong(expected, SlotState::Empty, std::memory_order_acq_rel, std::memory_order_relaxed) )	// Enough waiting, reset the slot and return
			return false;
		
		slot.state.store(SlotState::Empty, std::memory_order_release);
		return true;
	}
	
	// Waiting -> Done
	bool try_eliminate_pop(T& result) noexcept
	{
		const std::size_t idx = random_slot();
		CollisionSlot& slot = arena_[idx];
		
		SlotState expected = SlotState::Waiting;
		if ( slot.state.compare_exchange_strong(expected, SlotState::Done, std::memory_order_acq_rel, std::memory_order_relaxed) )
		{
			Node* node = slot.node.load(std::memory_order_acquire);
			result = std::move(node->data);
			delete node;
			return true;
		}
		
		return false;
	}
	
public:
	void push(T value)
	{
		Node* node = new Node(std::move(value));
		
		if (try_eliminate_push(node))
			return;
		
		node->next = top_.load(std::memory_order_relaxed);
		while ( !top_.compare_exchange_weak(node->next, node, std::memory_order_release, std::memory_order_relaxed) )
		{}
	}
	
	std::optional<T> pop()
	{
		T eliminated{};
		if (try_eliminate_pop(eliminated))
			return eliminated;
		
		Node* old_top = top_.load(std::memory_order_acquire);
		while (old_top)
		{
			if ( top_.compare_exchange_weak(old_top, old_top->next, std::memory_order_acquire, std::memory_order_acquire) )	// acquire in success & failure case both
			{
				T value = std::move(old_top->data);
				delete old_top;
				return value;
			}
		}
		
		return std::nullopt;
	}
};

/*
			Push()					Pop()
Empty 	------------->	Waiting	------------->	Done

*/