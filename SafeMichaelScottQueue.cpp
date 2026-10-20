#include <atomic>
#include <optional>
#include <cstdint>
#include <memory>
#include <hazard_pointer>

template <typename T>
class SafeMichaelScottQueue
{
	struct Node : public std::hazard_pointer_obj_base<Node> 
	{
		T data{};
		std::atomic<Node*> next{nullptr};
		
		Node() = default;
		explicit Node(T val) : data(std::move(val)) {}
	};
	
	alignas (std::hardware_destructive_interference_size)
		std::atomic<Node*> head_;
	alignas (std::hardware_destructive_interference_size)
		std::atomic<Node*> tail_;
		
public:
	MichaelScottQueue()
	{
		Node *sentinel = new Node();
		head_.store(sentinel, std::memory_order_relaxed);
		tail_.store(sentinel, std::memory_order_relaxed);
	}
	
	~MichaelScottQueue()
	{
		Node* current = head_.load(std::memory_order_relaxed);
		while (current)
		{
			Node* next = current->next.load(std::memory_order_relaxed);
			delete current;
			current = next;
		}
	}
	
	void enqueue(T value)
	{
		Node* node = new Node(std::move(value));
		
		while (true)
		{
			Node* tail = tail_.load(std::memory_order_acquire);
			Node* next = tail->next.load(std::memory_order_acquire);
			
			if (tail != tail_.load(std::memory_order_acquire))	// Snapshot validation
				continue;
			
			if (next == nullptr)
			{
				if ( tail->next.compare_exchange_weak(next, node, std::memory_order_release, std::memory_order_relaxed)) )
				{
					tail_.compare_exchange_strong(tail, node, std::memory_order_release, std::memory_order_relaxed);	// This would fail when other thread has updated tail so even if it fails, we return
					return;
				}
			}
			else	// Tail was lagging behind the actual end, because another producer succeeded in linking its node but stalled before advancing the tail, so current producer advances it as a helper before restarting it's own enqueue
			{
				tail_.compare_exchange_strong(tail, next, std::memory_order_release, std::memory_order_relaxed);
			}
		}
	}
	
	std::optional<T> dequeue()
	{
		std::hazard_pointer hp = std::make_hazard_pointer();
		while (true)
		{
			Node* head = hp.protect(head_, std::memory_order_acquire);
			Node* next = head->next.load(std::memory_order_acquire);
			Node* tail = tail_.load(std::memory_order_acquire);
			
			if (head != head_.load(std::memory_order_acquire))
				continue;
			
			if (head == tail)
			{
				if (next == nullptr)
					return std::nullopt;
				
				tail_.compare_exchange_strong(tail, next, std::memory_order_release, std::memory_order_relaxed);	// When other thread updated the tail, so update tail to the actual end. Another helper to advance the tail.
				continue;
			}
			
			T value = next->data;	// not head->data because head points to sentinel node
			if ( head_.compare_exchange_weak(head, next, std::memory_order_release, std::memory_order_acquire) )
			{
				hp.reset_protection();
				head->retire();	// memory reclamation step
				return value;
			}
		}
	}
};