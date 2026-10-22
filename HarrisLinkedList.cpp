#include <atomic>
#include <optional>
#include <cstdint>
#include <memory>
#include <new>

template <typename T>
class HarrisLinkedList
{
	struct Node
	{
		T data{};
		std::atomic<Node*> next{nullptr};
		
		Node() = default;
		explicit Node(T value) : data(std::move(value)) {}
	};

	static Node* marked(Node* ptr) noexcept
	{
		return reinterpret_cast<Node*>(reinterpret_cast<std::uintptr_t>(ptr) | 1u);
	}

	static Node* unmarked(Node* ptr) noexcept
	{
		return reinterpret_cast<Node*>(reinterpret_cast<std::uintptr_t>(ptr) & ~std::uintptr_t{1});
	}

	static bool is_marked(Node* ptr) noexcept
	{
		return (reinterpret_cast<std::uintptr_t>(ptr) & 1u) != 0;
	}
	
	Node* head_{new Node()};	// sentinel node; never removed
	Node* tail_{new Node()};	// sentinel node; never removed
	
public:
	HarrisLinkedList() noexcept
	{
		head_->next.store(tail_, std::memory_order_relaxed);	// head pointing to tail
	}
	
	~HarrisLinkedList()
	{
		Node* current = head_;
		while (current)
		{
			Node* next = unmarked(current->next.load(std::memory_order_relaxed));	// unmarked() needed?
			delete current;
			current = next;
		}
	}
	
	HarrisLinkedList(const HarrisLinkedList&) = delete;
	HarrisLinkedList& operator=(const HarrisLinkedList&) = delete;
	
	/*
		find() locates two adjacent nodes:
			a predecessor & a current node
			predecessor's next pointer is not marked & points to current node
			current node either matches the search key or is the first node whose key is >= search key in ordered list
		During traversal, find() physically removes any logically deleted nodes [lazy cleanup]
	*/
	struct FindResult
	{
		Node* prev;
		Node* curr;
	};
	
	FindResult find(const T& value) noexcept
	{
		Node* prev = nullptr, curr = nullptr, next = nullptr;
		
		retry:			// JUMP STATEMENT
			prev = head_;		// Restart from head
			curr = unmarked(prev->next.load(std::memory_order_acquire));
			
			while (true)
			{
				if (curr == tail_)
					return {prev, curr};
				
				next = curr->next.load(std::memory_order_acquire);
				bool c_marked = is_marked(next);
				next = unmarked(next);
				
				if (prev->next.load(std::memory_order_acquire) != curr) // Restart
					goto retry;
				
				if (c_marked)	// Logically marked node, so physically delete it
				{
					if ( !prev->next.compare_exchange_strong(curr, next, std::memory_order_release, std::memory_order_relaxed) ) // Restart
						goto retry; // Restart
					
					delete curr;
					curr = next;
				}
				else
				{
					if (curr->data >= value)
						return {prev, curr};
					
					prev = curr;
					curr = next;
				}
			}
	}
	
	bool insert(T value)
	{
		Node* node = new Node(value);
		
		while (true)
		{
			auto [prev, curr] = find(value);		// deletes logically removed nodes
			
			if ( curr != tail_ && curr->data == value)	// same value found in list, return false
			{
				delete node;
				return false;
			}
			
			node->next.store(curr, std::memory_order_relaxed);
			
			if ( prev->next.compare_exchange_strong(curr, node, std::memory_order_release, std::memory_order_relaxed) )
				return true;
		}
	}
	
	bool remove(const T& value)
	{
		while (true)
		{
			auto [prev, curr] = find(value);
			
			if (curr == tail_ || curr->data != value)
				return false;
			
			Node* next = unmarked(curr->next.load(std::memory_order_acquire));
			
			Node* unmarked_next = next;
			Node* marked_next = marked(next);
			// Logical deletion; Mark the curr node next pointer
			if ( !curr->next.compare_exchange_strong(unmarked_next, marked_next, std::memory_order_release, std::memory_order_relaxed) )
				continue;
			
			prev->next.compare_exchange_strong(curr, next, std::memory_order_release, std::memory_order_relaxed);
			
			return true;
		}
	}
	
	// skip logically deleted node
	// does not need to perform physical cleanup
	// never modifies list structure so it doesnt need to restart on consistency failures
	bool contains(const T& value) noexcept
	{
		Node* curr = unmarked(head_.load(std::memory_order_acquire));
		while (curr != tail_)
		{
			Node* next = curr->next.load(std::memory_order_acquire);
			bool c_marked = is_marked(next);
			if (!c_marked && curr->data == value)
				return true;
			curr = unmarked(next);
		}
		
		return false;
	}
};