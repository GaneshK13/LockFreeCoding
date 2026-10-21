#include <atomic>
#include <optional>
#include <cstdint>
#include <memory>
#include <new>

template <typename T>
class TreiberStack
{
	struct Node
	{
		T data{};
		Node* next{nullptr};
		
		explicit Node(T val) : data(std::move(val)) {}
	};
	
	alignas (std::hardware_destructive_interference_size)
		std::atomic<Node*> top_;
	
public:
	TreiberStack() = default;
	
	TreiberStack(const TreiberStack&) = default;
	TreiberStack& TreiberStack(const TreiberStack&) = default;
	
	~TreiberStack()
	{
		Node* current = top_.load(std::memory_order_relaxed);
		while (current)
		{
			Node* next = current->next;
			delete current;
			current = next;
		}
	}
	
	void push(T value)
	{
		Node* next = new Node(std::move(value));
		node->next = top_.load(std::memory_order_relaxed);
		
		while (! top_.compare_exchange_weak(node->next, node, std::memory_order_release, std::memory_order_relaxed) )
		{}
	}
	
	std::optional<T> pop()
	{
		Node* old_top = top_.load(std::memory_order_acquire);
		
		while (old_top)
		{
			// Success & Failure CAS case are both acquire
			// On Success: old_top has to be dereferenced therefore acquire
			// On Failure: old_top has to go through next iteration therefore acquire
			if ( top_.compare_exchange_weak(old_top, old_top->next, std::memory_order_acquire, std::memory_order_acquire) )
			{
				T val = std::move(old_top->data);
				delete old_top;
				return val;
			}
		}
	}
	
	bool empty() const noexcept
	{
		return top_.load(std::memory_order_acquire) == nullptr;
	}
};