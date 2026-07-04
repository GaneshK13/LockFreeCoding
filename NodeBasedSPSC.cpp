#include <atomic>
#include <iostream>
#include <new>

// UNBOUNDED QUEUE
template <typename T>
class NodeBasedSPSC
{
	public:
		NodeBasedSPSC()
		{
			// If head and tail initially pointed to the exact same data node in an empty queue,
			// a race condition would occur when the producer updates next
			// while the consumer tries to read/remove the node.
			Node* dummy = new Node();
			head.store(dummy, std::memory_order_relaxed);
			tail.store(dummy, std::memory_order_relaxed);
		}
		
		~NodeBasedSPSC()
		{
			Node* current_node = head.load(std::memory_order_relaxed);
			while (current_node != nullptr)
			{
				Node* next_node = current_node->next.load(std::memory_order_relaxed);
				delete current;
				current = next_node;
			}
		}
		
		// for thread safety guarantees
		NodeBasedSPSC(const NodeBasedSPSC&) = delete;
		NodeBasedSPSC& operator=(const NodeBasedSPSC&) = delete;
		
		template <typename... Args>
		void enqueue(Args&&... args)
		{
			Node* new_node = new Node(std::forward<Args>(args)...);
			Node* prev_head = head.load(std::memory_order_relaxed);
			
			// Inform the consumer thread that a new node is available
			// Crucial: release order guarantees 'new_node' construction is visible
			prev_head->next.store(new_node, std::memory_order_release);
			
			head.store(new_node, std::memory_order_relaxed);
		}
		
		// The node pointed to by tail_ is always a dummy node.
		// Its data has already been read or it was the initial placeholder.
		bool dequeue(T& res)
		{
			Node* current_tail = tail.load(std::memory_order_relaxed);
			Node* next_node = current_tail->next.load(std::memory_order_acquire);
			
			if (next_node == nullptr) // Empty stack
				return false;
			
			// In this architecture, the 'next_node' contains the value, 
			// and 'current_tail' becomes the old dummy to be deleted.
			res = std::move(next_node->data);	// move the data
			tail.store(next_node, std::memory_order_relaxed);	// pointing to new dummy whose data is already been read
			delete current_tail;
			
			return true;
		}
		
	private:
		struct Node
		{
			T data;
			std::atomic<Node*> next;	// atomic next
			
			Node(): next(nullptr) {}
			
			template <typename... Args>
			Node(Args&&... args) : data(std::forward<Args>(args)...), next(nullptr) {}
		};
	
		// Align atomic pointers to separate cache lines to completely prevent False Sharing
		alignas(64) std::atomic<Node*> head;
		alignas(64) std::atomic<Node*> tail;
};

/*


A. The Sentinal/Dummy Node Mechanism

If head and tail initially pointed to the exact same data node in an empty queue, a race condition would occur when the producer updates next while the consumer tries to read/remove the node.
How it works here: The node pointed to by tail_ is always a dummy node. Its data has already been read or it was the initial placeholder.
When a producer calls enqueue, it links a new node to prev_head->next.
The consumer evaluates emptiness by checking if current_tail->next is null. If it isn't null, it means next_node contains the legitimate fresh data. The consumer extracts that data and promotes next_node to be the new dummy node, deleting the old one.



B. Memory Fence Analysis (Acquire/Release Edge)

Why std::memory_order_seq_cst isn't needed and how acquire/release coordinates memory visibility?

Producer's Release: prev_head->next.store(new_node, std::memory_order_release);
This guarantees that the CPU and the compiler cannot reorder the constructor execution of Node(args...) after the pointer is linked. The actual data payload must be written to memory before the consumer can see the updated pointer.

Consumer's Acquire: Node* next_node = current_tail->next.load(std::memory_order_acquire);
This creates a synchronized dependency. It forces the consumer's CPU core to invalidate its local cache line for that node and pull the fresh payload data committed by the producer's release step.



C. Why head_ and tail_ Loads/Stores are relaxed?

Look closely at head_.store(new_node, std::memory_order_relaxed);
Why can it be relaxed? Because head_ is EXCLUSIVELY read and written by the producer thread. No other thread ever looks at head_.
The same goes for tail_; it is exclusively accessed by the consumer thread.
The only bridge of communication between the two cores is the next pointer inside the Node structure. Therefore, only next requires explicit acquire-release synchronization semantics.



Q1: "Lock-free queue, but is it actually deterministic?"
No, it is lock-free but not wait-free, and it is structurally unsuitable for ultra-low latency critical paths.
Reason: It relies on new and delete. The standard allocator calls down to the OS kernel memory management subsystems, which use internal global locks (ptmalloc, jemalloc, etc.).
If another thread in the system triggers a page fault or memory compaction, our enqueue or dequeue threads could experience an unpredictably large latency spike (tail latency).



Q3: "Does this node-based design suffer from the ABA problem?"
No. The ABA problem typically manifests in Multi-Producer or Multi-Consumer scenarios where a thread suspends, a node is freed,
and then reallocated at the exact same virtual memory address before the suspended thread resumes its CAS operation.
Because this queue is strictly SPSC and uses direct pointer assignment instead of Compare-And-Swap (compare_exchange) loops, the ABA vulnerability is mathematically impossible.


*/