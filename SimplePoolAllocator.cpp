// POOL_SIZE = 56

template <typename T>
class SimplePoolAllocator
{
	alignas(T) char pool_[POOL_SIZE * sizeof(T)];
	
	struct FreeNode { FreeNode* next; };
	
	FreeNode* free_head_ = nullptr;
	size_t allocated_count_ = 0;
	
	void initialize_pool()
	{
		char* current = pool_;
		
		for (size_t i = 0; i < POOL_SIZE - 1; i++)
		{
			reinterpret_cast<FreeNode*>(current)->next = reinterpret_cast<FreeNode*>(current + sizeof(T));
			current += sizeof(T);
		}
		
		reinterpret_cast<FreeNode*>(current)->next = nullptr;
		free_head_ = reinterpret_cast<FreeNode*>(pool_);
	}
	
	T* allocate()
	{
		if (free_head_ == nullptr)
		{
			std::cout << "Pool exhausted!" << std::endl;
			throw std::bad_alloc();
		}
		
		T* result = reinterpret_cast<T*>(free_head_);
		free_head_ = free_head_->next;
		++allocated_count_;
		
		std::cout << "Allocated block #" << allocated_count_ << " at " << static_cast<void*>(result) << std::endl;
		
		return result;
	}
	
	void deallocate(T* p)
	{
		if (pool_ == nullptr)
			return;
		if (p < reinterpret_cast<T*>(pool_) || p >= reinterpret_cast<T*>(pool_ + sizeof(pool_)))
		{
			std::cout << "Warning: pointer not from this pool!" << std::endl;
			return;
		}
		
		// Push back to free list
		FreeNode* node = reinterpret_cast<FreeNode*>(p);
		node->next = free_head_;
		free_head_ = node;
		--allocated_count_;
		
		std::cout << "Deallocated block, " << allocated_count_ << " still allocated" << std::endl;
	}
};

/*
///////////
// Usage //
///////////



std::cout << "Manual allocation & construction\n";
for (int i = 0; i < 34; ++i)
{
	// step 1: allocate raw memory from pool_
	objects[i] = custom_pool.allocate();
	
	// step 2: construct object using placement new
	new (objects[i]) TestObject();
}



std::cout << Manual destruction & deallocation\n";
for (int i = 0; i < 4; ++i)
{
	// step 1: manually call destructor
	objects[i]->~TestObject();
	
	// step 2: return memory to pool
	custom_pool.deallocate(objects[i]);
}
*/