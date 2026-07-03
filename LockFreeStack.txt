#include <atomic>
#include <optional>

template <typename T>
struct LockFreeStack
{
private:
    struct Node
    {
        T val{};
        Node *next{nullptr};

        Node(T const& v) : val(v), next(nullptr) {}
    };
    
    struct alignas(16) TaggedPointer
    {
        Node* ptr{nullptr};
        uint64_t tag{};
    };
    
	// To ensure the hardware supports lock-free 16-byte atomic operations
    static_assert(std::atomic<TaggedPointer>::is_always_lock_free, "Target Architecture does not support Double Width CAS!");
    
    std::atomic<TaggedPointer> head;
    
public:

    LockFreeStack()
    {
        head.store(TaggedPointer{}, std::memory_order_relaxed);
    }
	
	~LockFreeStack()
	{
		TaggedPointer current = head.load(std::memory_order_relaxed);
		while (current.ptr)
		{
			Node* next = current.ptr->next;
			delete current.ptr;
			current.ptr = next;
		}
	}

    void push(T val)
    {
        Node* new_node = new Node(val);
		TaggedPointer old_head = head.load(std::memory_order_relaxed);
		
		while(true)
		{
			new_node->next = old_head.ptr;
			
			TaggedPointer new_head{new_node, old_head.tag + 1};
			
			if (head.compare_exchange_weak(old_head, new_head, std::memory_order_release, std::memory_order_relaxed))
				return;
			
			// If CAS fails, old_head is updated automatically, loop retries
		}
    }

	// ABA catastrophe prevented using Double Width CAS
    std::optional<T> pop()
    {
        TaggedPointer old_head = head.load(std::memory_order_acquire);
		while (true)
		{
			if (old_head.ptr == nullptr)	// empty stack
				return std::nullopt;
			
			Node* next_node = old_head.ptr->next;
			TaggedPointer new_head{next_node, old_head.tag + 1};
			
			if (head.compare_exchange_weak(old_head, new_head, std::memory_order_acq_rel, std::memory_order_relaxed))
			{
				T result = old_head.ptr->val;
				delete old_head.ptr;	// Safely reclaim memory
				return result;
			}
			// If CAS fails, old_head is updated automatically, loop retries
		}
    }
};