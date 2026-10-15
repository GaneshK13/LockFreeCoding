#include <atomic>
#include <optional>
#include <cstdint>
#include <new>

template <typename T>
class TaggedStack
{
	struct Node
	{
		T data;
		Node* next{nullptr};
		explicit Node(T val)  : data( std::move(val) ) {}
	};
	
	struct alignas(16) TaggedPtr
	{
		Node* ptr{nullptr};
		std::unint64_t tag{0};
		
		bool operator== (const TaggedPtr& other) const noexcept
		{
			return ptr == other.ptr && tag == other.tag;
		}
	};
	
	static_assert(sizeof(TaggedPtr) == 16, "TaggedPtr must be exactly must be exactly 16 bytes for CMPXCHG16B");
	static_assert(std::atomic<TaggedPtr>::is_always_lock_free(), "TaggedPtr atomic must be lock free on this platform");
	
	std::atomic<TaggedPtr> top_{TaggedPtr{}};
	
public:
	
	void push(T value)
	{
		Node* node = new Node(std::move(value));
		TaggedPtr expected = top_.load(std::memory_order_relaxed);
		TaggedPtr desired;
		
		do
		{
			node->next = expected.ptr;
			desired = TaggedPtr{node, expected.tag + 1};
		} while (! top_.compare_exchange_weak(expected, desired, std::memory_order_release, std::memory_order_relaxed))
	}

	std::optional<T> pop()
	{
		TaggedPtr expected = top_.load(std::memory_order_acquire);
		TaggedPtr desired;
		
		while (expected.ptr)
		{
			desired = TaggedPtr{expected.ptr->next, expected.tag + 1};
			if ( top_.compare_exchange_weak(expected, desired, std::memory_order_release, std::memory_order_acquire) )
			{
				T value = std::move(expected.ptr->data);
				delete expected.ptr;
				return value;
			}
		}
		
		return std::nullopt;
	};
};