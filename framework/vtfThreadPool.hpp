#ifndef __VTF_THREAD_POOL_HPP_INCLUDED__
#define __VTF_THREAD_POOL_HPP_INCLUDED__

#include <thread>
#include <mutex>
#include <vector>
#include <tuple>
#include <memory>
#include <utility>
#include <chrono>
#include <algorithm>
#include <atomic>
#include <condition_variable>

#include "vtfThreadPoolHelpers.hpp"

namespace vtf
{

struct ThreadPool
{
	// If any parameter of callback callable is of ThreadIndex type,
	// reference to ThreadIndex or a pointer to ThreadIndex (including const)
	// then it will be automatically substituted with real thread index.
	// value_type is defined as a pair <current_thread_index, thread_count_being_run>
	struct ThreadIndex
	{
		typedef std::pair<uint32_t, uint32_t> value_type;

		value_type index;

		operator add_cref<value_type> () const {
			return index;
		}
		add_cref<value_type> operator ()() const {
			return index;
		}
		add_ref<ThreadIndex> operator= (add_cref<value_type> other) {
			index = other;
			return *this;
		}
		ThreadIndex () : index() {}
		ThreadIndex (add_cref<value_type> other) : index(other) {}
	};

	enum class States : uint32_t
	{
		WAIT, RUN, BREAK, INACTIVE
	};

	// Forward declaration of InvokerImpl type.
	template<class, class>	struct InvokerImpl;

	// Base Invoker type.
	struct Invoker
	{
		virtual void call(uint32_t threadIndex) = 0;
	};

	// Invoker class implementation for callback methods.
	template<class Fn, class C, class R, class... Args>
	struct InvokerImpl<Fn, std::tuple<C, R, std::tuple<Args...>>> : Invoker
	{
		typedef std::tuple<add_ptr<C>, add_rref<Args>...> FormalParams;
		typedef typename tph::wrap_tuple<ThreadIndex, FormalParams>::type Params;
		void waitContinue (Args... args)
		{
			for (uint32_t threadIndex = 0u; threadIndex < m_threadCount; ++threadIndex)
			{
				m_indices.at(threadIndex) = { threadIndex, m_threadCount };
				auto formalParams = std::tuple_cat(std::make_tuple(m_behalf), std::forward_as_tuple(std::forward<Args>(args)...));
				auto localParams = tph::make_wrapped_tuple<ThreadIndex>(std::move(formalParams));
				tph::update_wrapped_tuple(localParams, m_indices.at(threadIndex));
				m_params.at(threadIndex).assign(std::move(localParams));
			}

#if defined(DEBUG) || defined(_DEBUG)
			auto debug = std::tuple_cat(std::make_tuple(m_behalf), std::forward_as_tuple(std::forward<Args>(args)...));
			static_assert(std::is_same_v<FormalParams, std::remove_reference_t<decltype(debug)>>, "???");
#endif
			m_pool->setCurrent(this);
			m_pool->nextContinue();
			m_pool->broadcast(States::RUN);

			bool once = ! m_callInMain;
			do
			{
				if (once)
					std::this_thread::yield();
				else
				{
					once = true;
					std::apply(m_callable, *m_params.at(m_threadCount - 1u).get());
				}
			} while (m_pool->waitContinue());
		}
		InvokerImpl (add_ptr<ThreadPool> pool, Fn&& fn, add_ptr<C> c, bool callInMain)
			: m_pool		(pool)
			, m_threadCount	(callInMain ? (pool->m_threadCount + 1u) : pool->m_threadCount)
			, m_callable	(std::forward<Fn>(fn))
			, m_params		(m_threadCount)
			, m_indices		(m_threadCount)
			, m_behalf		(c)
			, m_callInMain	(callInMain) {}
		virtual ~InvokerImpl () = default;
	protected:
		virtual void call (uint32_t threadIndex) override
		{
			std::apply(m_callable, *m_params.at(threadIndex).get());
		}
	private:
		add_ptr<ThreadPool> const				m_pool;
		const uint32_t							m_threadCount;
		std::decay_t<Fn>						m_callable;
		std::vector<tph::FakeMoveable<Params>>	m_params;
		std::vector<ThreadIndex>				m_indices;
		add_ptr<C>								m_behalf;
		bool									m_callInMain;
	};

	// Invoker class implementation for callback routines.
	template<class Fn, class R, class... Args>
	struct InvokerImpl<Fn, std::tuple<R, std::tuple<Args...>>> : Invoker
	{
		typedef std::tuple<add_rref<Args>...> FormalParams;
		typedef typename tph::wrap_tuple<ThreadIndex, FormalParams>::type Params;
		void waitContinue (Args... args)
		{
			for (uint32_t threadIndex = 0u; threadIndex < m_threadCount; ++threadIndex)
			{
				m_indices.at(threadIndex) = { threadIndex, m_threadCount };
				auto formalParams = std::forward_as_tuple(std::forward<Args>(args)...);
				auto localParams = tph::make_wrapped_tuple<ThreadIndex>(std::move(formalParams));
				tph::update_wrapped_tuple(localParams, m_indices.at(threadIndex));
				m_params.at(threadIndex).assign(std::move(localParams));
			}

#if defined(DEBUG) || defined(_DEBUG)
			auto debug = std::tuple_cat(std::forward_as_tuple(std::forward<Args>(args)...));
			static_assert(std::is_same_v<FormalParams, std::remove_reference_t<decltype(debug)>>, "???");
#endif
			m_pool->setCurrent(this);
			m_pool->nextContinue();
			m_pool->broadcast(States::RUN);

			bool once = ! m_callInMain;
			do
			{
				if (once)
					std::this_thread::yield();
				else
				{
					once = true;
					std::apply(m_callable, *m_params.at(m_threadCount - 1u).get());
				}
			} while (m_pool->waitContinue());
		}
		InvokerImpl (add_ptr<ThreadPool> pool, Fn&& fn, bool callInMain)
			: m_pool		(pool)
			, m_threadCount	(callInMain ? (pool->m_threadCount + 1u) : pool->m_threadCount)
			, m_callable	(std::forward<Fn>(fn))
			, m_params		(m_threadCount)
			, m_indices		(m_threadCount)
			, m_callInMain	(callInMain) {}
		virtual ~InvokerImpl () = default;

	protected:
		virtual void call (uint32_t threadIndex) override
		{
			std::apply(m_callable, *m_params.at(threadIndex).get());
		}
		add_ptr<ThreadPool> const				m_pool;
		const uint32_t							m_threadCount;
		std::decay_t<Fn>						m_callable;
		std::vector<tph::FakeMoveable<Params>>	m_params;
		std::vector<ThreadIndex>				m_indices;
		bool									m_callInMain;
	};

	template<class Fn, class C>
	struct Call : InvokerImpl<Fn, std::tuple<C, typename routine_signature<Fn>::Result,
												typename routine_signature<Fn>::ArgList>>
	{
		Call(add_ptr<ThreadPool> pool, Fn&& fn, add_ptr<C> c, bool callInMain)
			: InvokerImpl<Fn, std::tuple<C, typename routine_signature<Fn>::Result,
											typename routine_signature<Fn>::ArgList>>
				(pool, std::forward<Fn>(fn), c, callInMain) {}
	};
	template<class Fn>
	struct Call<Fn, void> : InvokerImpl<Fn, std::tuple<typename routine_signature<Fn>::Result,
														typename routine_signature<Fn>::ArgList>>
	{
		Call(add_ptr<ThreadPool> pool, Fn&& fn, bool callInMain)
			: InvokerImpl<Fn, std::tuple<typename routine_signature<Fn>::Result,
										typename routine_signature<Fn>::ArgList>>
				(pool, std::forward<Fn>(fn), callInMain) {}
	};

	template<class Fn, std::enable_if_t<std::is_same_v<typename routine_signature<Fn>::Behalf, void>, int> = 13>
	std::shared_ptr<Call<Fn, void>> getInterface (Fn&& fn, bool callInMain = true)
	{
		return std::make_shared<Call<Fn, void>>(this, std::forward<Fn>(fn), callInMain);
	}
	template<class Fn, class C, std::enable_if_t<std::is_same_v<typename routine_signature<Fn>::Behalf, C>, int> = 31>
	std::shared_ptr<Call<Fn, C>> getInterface (Fn&& fn, C* c, bool callInMain = true)
	{
		return std::make_shared<Call<Fn, C>> (this, std::forward<Fn>(fn), c, callInMain);
	}
	ThreadPool (uint32_t threadCount)
		: m_continue	(0u)
		, m_threadCount	((threadCount == 0u || threadCount > std::thread::hardware_concurrency())
							? std::thread::hardware_concurrency() : threadCount)
		, m_threads		()
		, m_states		(m_threadCount)
		, m_continues	(m_threadCount)
		, m_variables	(m_threadCount)
		, m_mutexes		(m_threadCount)
		, m_current		(nullptr)
	{
		init();
	}
	virtual ~ThreadPool ()
	{
		waitFinish();
	}
	void waitFinish ()
	{
		broadcast(States::BREAK);
		do
		{
			std::this_thread::yield();
			std::this_thread::sleep_for(std::chrono::microseconds(5));
		} while (!std::all_of(m_states.begin(), m_states.end(),
			[](const States& state) { return state == States::INACTIVE; }));
	}
	static bool selfTest();
private:
	void init ()
	{
		m_threads.reserve(m_threadCount);
		for (uint32_t threadIndex = 0u; threadIndex < m_threadCount; ++threadIndex)
		{
			m_states.at(threadIndex) = States::WAIT;
			m_threads.emplace_back(&ThreadPool::worker, this, threadIndex);
			m_threads.back().detach();
		}
	}
	void broadcast (const States state)
	{
		for (uint32_t i = 0u; i < m_threadCount; ++i)
		{
			if (States::INACTIVE != m_states[i])
			{
				std::lock_guard<std::mutex> lk(m_mutexes[i]);
				m_states[i] = state;
			}
			m_variables[i].notify_one();
		}
	}
	void nextContinue ()
	{
		m_continue = m_continue ^ 7u;
	}
	bool waitContinue ()
	{
		return (!std::all_of(m_continues.begin(), m_continues.end(),
			[&](const uint32_t& value) { return value == m_continue; }));
	}
	void setCurrent (add_ptr<Invoker> current)
	{
		m_current = current;
	}
	void worker (const uint32_t threadIndex)
	{
		while (States::BREAK != m_states[threadIndex])
		{
			std::unique_lock lk(m_mutexes[threadIndex]);
			m_variables[threadIndex].wait(lk, [&] { return m_states[threadIndex] != States::WAIT; });
			if (States::BREAK == m_states[threadIndex])
			{
				lk.unlock();
				m_states[threadIndex] = States::INACTIVE;
				return;
			}
			m_current->call(threadIndex);
			lk.unlock();
			m_states[threadIndex] = States::WAIT;
			m_continues[threadIndex] = m_continue;
		}
	}
private:
	uint32_t								m_continue;
	const uint32_t							m_threadCount;
	std::vector<std::thread>				m_threads;
	std::vector<std::atomic<States>>		m_states;
	std::vector<std::atomic<uint32_t>>		m_continues;
	std::vector<std::condition_variable>	m_variables;
	std::vector<std::mutex>					m_mutexes;
	add_ptr<Invoker>						m_current;
};

} // namespace vtf

#endif // __VTF_THREAD_POOL_HPP_INCLUDED__
