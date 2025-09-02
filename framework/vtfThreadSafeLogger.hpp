#ifndef __VTF_THREADSAFELOGGER_HPP_INCLUDED__
#define __VTF_THREADSAFELOGGER_HPP_INCLUDED__

#include <condition_variable>
#include <iostream>
#include <mutex>
#include <sstream>
#include <thread>

namespace vtf
{
//template<class Y> extern std::ostream& operator<< (std::ostream&, const Y&);

struct Logger;
//template<class X> Logger& operator<< (Logger&, const X&);

struct Logger
{
	//template<class X> friend Logger& operator<< (Logger&, const X&);
	template<class X> Logger& operator<< (const X& x)
	{
		mInput << std::noskipws << x;
		return *this;
	}
	//template<class X> friend Logger& operator<< (Logger& log, const X& x) { return log.operator<<(x); }
	Logger& operator<< (std::ostream& (&maybeNewLine)(std::ostream&));
	Logger ();
	Logger (const Logger& other);
protected:
	std::ostream& mOutput;
	std::stringstream& mInput;
	std::mutex& mMutex;
};

struct AddressWatcher
{
	enum State { NotStartedCallWatch, WaitForStart, Continue, FinishedByCallee, FinishedByCaller, ExhaustedLoop };

	AddressWatcher();

	void add(void const* address, size_t size);
	void add(std::pair<void const*, size_t> const& info);

	void sleepCallingThread(uint32_t microseconds = 100u) const
	{
		std::this_thread::sleep_for(std::chrono::microseconds(microseconds));
	}

	static bool selfTest(uint32_t sleepBeforeJoinInMicroseconds = 100u);

	typedef std::pair<size_t, size_t> Trigger;
	class thread : public std::thread
	{
		friend struct AddressWatcher;
		State* m_continueThread = nullptr;
	public:
		using std::thread::thread;
		void join();
		void sleepCallingThread(uint32_t microseconds = 100u) const
		{
			std::this_thread::sleep_for(std::chrono::microseconds(microseconds));
		}
	};

	template<class TriggerFunctor> // void(Trigger)
	thread watch(TriggerFunctor&& trigger)
	{
		size_t dataSize = 0u;
		for (uint32_t i = 0u; i < m_map.size(); ++i)
			dataSize += m_map[i].second;
		std::vector<uint8_t> data(dataSize);
		for (size_t i = 0u, offset = 0u; i < m_map.size(); ++i)
		{
			const size_t size = m_map[i].second;
			std::memcpy(&data.data()[offset], m_map[i].first, size);
			offset += size;
		}
		auto watcher = [&, data, this](State& continueThread, Trigger& lastResult, std::mutex& mtx, std::condition_variable& cv)
			{
				std::unique_lock lock(mtx);
				cv.wait(lock, [&] { return State::Continue == continueThread; });

				while (State::Continue == continueThread) {
					for (size_t i = 0u, offset = 0u; i < m_map.size(); ++i) {
						const size_t size = m_map[i].second;
						uint8_t const* cmp = &data.data()[offset];
						auto mem = reinterpret_cast<uint8_t const*>(m_map[i].first);
						for (size_t j = 0u; j < size; ++j) {
							if (cmp[j] != mem[j]) {
								continueThread = State::FinishedByCallee;
								lastResult = Trigger(i, j);
								trigger(lastResult);
								return;
							}
						}
						offset += size;
					}
				}

				if (State::FinishedByCaller == continueThread)
					continueThread = State::ExhaustedLoop;
			};
		thread th(watcher, std::ref(m_continueThread), std::ref(m_lastResult), std::ref(m_mutex), std::ref(m_cv));
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			th.m_continueThread = &m_continueThread;
			m_continueThread = State::Continue;
		}
		m_cv.notify_one();
		return th;
	}

	Trigger getLastResult() const { return m_lastResult; }
	State getLastState() const { return m_continueThread; }

private:
	State m_continueThread;
	Trigger m_lastResult;
	std::mutex m_mutex;
	std::condition_variable m_cv;
	std::vector<std::pair<void const*, size_t>> m_map;
};

} // namespace vtf

#endif // __VTF_THREADSAFELOGGER_HPP_INCLUDED__
