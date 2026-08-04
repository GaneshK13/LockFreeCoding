/*
/////////////////////
// POOL ALLOCATORS //
/////////////////////

// Any sized objects of different types: so not limited to one object type
// Not allocating on a stack: it's just a pattern which is anyway allocating on heap
// For deallocating memory allocated for one type, which was allocated before the other types got allocated, all the other types have to be deallocated first [Disadvantages]
	// Cannot dellocate individual objects out of order [All at once]
	// Deallocate back to checkpoint marker
// Fastest possible allocation
// Zero fragmentation
// What's it best for?
	// 1. Scratchpad memory
	// 2. Best tool for short-lived data [Temporary data]
*/

class StackAllocator
{
public:
	explicit StackAllocator(size_t size)
		: total_size_(size), current_offset_(0) 
	{
		memory_ = new char[total_size_];
		std::cout << "Stack with " << total_size_ << " bytes" << std::endl;
	}

private:
	// Pointer to allocated memory block
	char* memory_;
	// Total size of memory block
	size_t total_size_;
	// Current Top (next allocation position)
	size_t current_offset_;
};

// Create a 35 byte stack allocator
StackAllocator allocator(35);


/*
///////////
// Usage //
///////////

// Allocate some integers
int* integers = allocator.allocate<int>(7);
for (int i = 0; i < 7; ++i)
{
	integers[i] = i * i;
}

// Show current usage
std::cout << "Usage: " << allocator.get_used_size() << " / " << allocator.get_total_size() << " bytes\n";

// Save a marker before allocating temporary data
size_t checkpoint = allocatory.get_marker();

--------------------------------------------------------

// Allocate some floats
int* floats = allocator.allocate<float>(7);
for (int i = 0; i < 7; ++i)
{
	floats[i] = i * i;
}

// Show current usage
std::cout << "Usage: " << allocator.get_used_size() << " / " << allocator.get_total_size() << " bytes\n";

// Save a marker before allocating temporary data
size_t checkpoint = allocatory.get_marker();

*/