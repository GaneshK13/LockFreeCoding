#include <atomic>
#include <optional>
#include <cstdint>

template <typename T>
class ABAVulnerableStack
{
	struct Node
	{
		T data;
		Node* next{};
		explicit Node(T val)  : data( std::move(val) ) {} 
	};
	
	std::atomic<Node*> top_{nullptr};

public:
	void push(T value)
	{
		auto* node = new Node(std::move(value));
		node->next = top_.load(std::memory_order_relaxed);
		while (! top_.compare_exchange_weak(node->next, node, std::memory_order_release, std::memory_order_relaxed) )
		{
		}
	}
	
	std::optional<T> pop()
	{
		Node* old_top = top_.load(std::memory_order_acquire);
		while (old_top)
		{
			Node* new_top = old_top->next;				// ISSUE #1
			if ( top_.compare_exchange_weak(old_top, new_top, std::memory_order_release, std::memory_order_acquire) )
			{
				T value = std::move(old_top->data);
				delete old_top;							// ISSUE #2 (Freed here, another thread may reuse the address)
				return value;
			}
		}
		
		return std::nullopt;
	}
};

// -----------------------
// USE-AFTER-FREE SCENARIO
// -----------------------

/*

| A	|
| B	|
| C	|
-----

// t1 calls pop and reads A as old_top and B as new_top [at #1]
// It is about to swap top from A to B, but before it executes
// t2 pops A and frees it.
// t2 pops B and frees it.
// t2 pushes new node [allocator happens to place at the same address as A becoz freed memory is reused]


| A*|
| C	|
-----

// A* is new node at the same address as previous A
// A* pointing to C
// t1 resumes and executes its CAS
// CAS comparison succeeds as top still holds address A
// t1 sets top to B [which has already been freed and deallocated]
// stack is now corrupted and subsequent access to B is undefined behaviour

*/