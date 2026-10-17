#include <hazard_pointer> // C++26
#include <atomic>
#include <optional>
#include <cstdint>
#include <memory>
#include <new>

template <typename T>
class HazardProtectedStack
{
	struct Node
	{
		T data;
		std::atomic<Node*> next{nullptr};
		
		explicit Node(T val) : data(std::move(val)) {}
	};
	
	std::atomic<Node*> top_{nullptr};
	
public:
	void push(T value)
	{
		auto* node = new Node(std::move(value));
		node->next.store(top_.load(std::memory_order_relaxed));
		while ( !top.compare_exchange_weak (*reinterpret_cast<Node**>(&node->next), node, std::memory_order_release, std::memory_order_relaxed ) )
		{
		}
	}
	
	std::optional<T> pop()
	{
		std::hazard_pointer hp = std::make_hazard_pointer();
		Node* old_top = nullptr;
		
		do
		{
			old_top = top_.load(std::memory_order_acquire);
			if (!old_top)
				return std::nullopt;
			
			// Publish the hazard before derefercing
			hp.protect(top_, std::memory_order_acquire);
			old_top = top_.load(std::memory_order_acquire);	// validate after publish pattern
			if (!old_top)
				return std::nullopt;
			
		} while (! top_.compare_exchange_weak ( old_top, old_top->next.load(std::memory_order_relaxed), std::memory_order_release, std::memory_order_acquire ) );
		
		T value = std::move(old_top->data);
		
		// Defer reclamation until no hazard slot references this node
		hp.reset_protection();
		std::hazard_pointer_obj_base<Node>::retire(old_top);	// registers the removed node for deferred reclamation
		
		return value;
	}
};

// hp.protect();
// hp.protect_ptr();
// node->retire();