

//-------------
// For events, instead of state
// Aiming for > 100 ns & with low jitter
// Mkt data fanout
// Cannot use TCP [Point to Point]
// 1. 'User spaced' UDP Multicast [Cannot go to kernel] [Used for single server, cannot send across multiple servers]
// 2. SPMC [Single Producer Multiple Consumer] Multicast queue
// 		Shared Memory
// 		Ring buffer
//-------------




//-------------
//-------------
struct SPSCQueue
{
	alignas(64) std::atomic<uint64_t> m_WriteIdx;
	alignas(64) std::atomic<uint64_t> m_ReadIdx;
	alignas(64) uint8_t m_data[0];
};

// Issue:
//		1. Cannot maintain a read index per reader
//		2. Overflow is possible: no push back mechanism

//-------------
//-------------

// SPMC Queue V2 [uses seq lock]

using BlockVersion = uint32_t;
using MessageSize = uint32_t;

class Block
{
	std::atomic<BlockVersion> m_version;
	std::atomic<MessageSize> m_size;
	
	uint8_t m_data[0];
};

struct Header
{
	alignas(64) std::atomic<uint64_t> m_BlockCounter{0};
	alignas(64) Block m_Blocks[0];
};

// Less contention, m_BlockCounter is only read when readers join the queue!

// Simplified code

template <class WriteCallback>
void Q::Write(MessageSize size, WriteCallback writeCallback)
{
	m_version += 1;	// seq lock
	
	m_size += size;
	writeCallback(&m_CurrentBlock->m_data[0]);
	
	m_version += 1;	// seq lock
	m_BlockCounter += 1;
}