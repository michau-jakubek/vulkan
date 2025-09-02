#include "vtfZDeletable.hpp"
#include "vtfVkUtils.hpp"
#include "vtfThreadSafeLogger.hpp"
#include <iterator>
#include <functional>
#include <numeric>

namespace vtf
{

std::mutex Logger_mMutex;
thread_local std::stringstream Logger_mInput;

template<class Y> extern std::ostream& operator<< (std::ostream&, const Y&);
template<class Y> using zzz = std::function<std::ostream&(std::ostream&, const Y&)>;
template<class Y> using uuu = routine_arg_t<std::ostream&(std::ostream&, const Y&), 1u>;

template<class X>
Logger& operator<< (Logger& log, uuu<X>)
{
	//UNREF(x);
	//mInput << std::noskipws << x;
	return log;
}
//template<class X> friend Logger& operator<< (Logger& log, const X& x) { return log.operator<<(x); }
Logger& Logger::operator<< (std::ostream& (&maybeNewLine)(std::ostream&))
{
	mInput << std::noskipws << maybeNewLine;
	std::istream_iterator<std::ostream::char_type> begin(mInput);
	std::istream_iterator<std::ostream::char_type> end;
	{
		std::lock_guard<std::mutex> lk(mMutex);
		std::copy(begin, end, std::ostream_iterator<std::ostream::char_type>(mOutput));
	}
	mInput.clear();
	return *this;
}
Logger::Logger () : mOutput(std::cout), mInput(Logger_mInput), mMutex(Logger_mMutex) {}
Logger::Logger (const Logger& other) : mOutput(other.mOutput), mInput(other.mInput), mMutex(other.mMutex) {}


void AddressWatcher::add (void_cptr address, size_t size)
{
	m_map.push_back(std::make_pair(address, size));
}

void AddressWatcher::add (std::pair<void const*, size_t> const& info)
{
	m_map.push_back(info);
}

AddressWatcher::AddressWatcher()
	: m_continueThread(State::NotStartedCallWatch)
	, m_lastResult()
	, m_mutex()
	, m_cv()
	, m_map()
{
}

void AddressWatcher::thread::join ()
{
	const State before = *m_continueThread;
	if (before == State::Continue)
	{
		*m_continueThread = State::FinishedByCaller;
	}
	static_cast<std::thread*>(this)->join();
	const State after = *m_continueThread;
	MULTI_UNREF(before, after);
}

bool AddressWatcher::selfTest (uint32_t sleepBeforeJoinInMicroseconds)
{
	uint32_t arr[10]{};
	AddressWatcher aw;
	for (uint32_t i = 0u; i < ARRAY_LENGTH_CAST(arr, uint32_t); ++i)
	{
		arr[i] = i + 123u;
		aw.add(&arr[i], sizeof(uint32_t));
	}
	auto trigger = [&](Trigger) -> void {	};
	AddressWatcher::thread worker = aw.watch(trigger);
	((uint8_t*)&arr[3u])[1u] = 255u;
	aw.sleepCallingThread(sleepBeforeJoinInMicroseconds);
	worker.join();
	return aw.getLastResult().first == 3u && aw.getLastResult().second == 1u;
}


} // namespace vtf

