////////////////
// SEQ LOCKS  //
////////////////

//-------------
For sharing state, instead of events
Best used for mkt data single/few producer/s and multiple consumers.
Wait Free Producer & Lock Free Consumer.
When shared data is small - which can fit within one cache line.
//-------------


// Below is simplified version, do not use it directly in the code

template <class T>
class SeqLock
{
	std::atomic<uint32_t> m_version;
	T m_data;
};

template <class T>
void SeqLock<T>::Store(const T& value)
{
	m_version += 1;	// m_version.fetch_add(1);

	std::memcpy(m_data, &value, sizeof(T));

	m_version += 1;	// m_version.fetch_add(1);
}

template <class T>
void SeqLock<T>::Load(T &value)
{
	const uint32_t curr_version = m_version.load();
	if (curr_version & 1 != 0)	// Write by Store() in progress
		return false;
		
	std::memcpy(value, &m_data, sizeof(T));
	
	return curr_version == m_version;
}