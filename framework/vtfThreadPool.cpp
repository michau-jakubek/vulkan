#include <iomanip>
#include <iostream>
#include <map>
#include <numeric>
#include <sstream>
#include <tuple>
#include <utility>
#include "vtfThreadPool.hpp"

namespace
{
using namespace vtf;

struct MM
{
	uint32_t value;
	MM (uint32_t m) : value(m) {}
	MM (add_cref<MM> other) : value(other.value) {}
	operator uint32_t () const { return value; }
};

struct Watcher
{
	std::mutex mutex;
	std::map<uint32_t, uint32_t> map;
	void insert(uint32_t threadIndex, uint32_t value)
	{
		std::lock_guard<decltype(mutex)> lock(mutex);
		map[threadIndex] = value;
	}
	void clear() { map.clear(); }
};

void routine_worker(ThreadPool::ThreadIndex threadIndex,
					add_ptr<std::atomic<uint32_t>> value,
					const int N, const MM m, add_ref<Watcher> watcher)
{
	watcher.insert(threadIndex().first, m);

	if (threadIndex().first == threadIndex().second - 1u)
	{
		return; // skip calculation in main thread
	}

	for (int i = 0; i < N; ++i)
	{
		value->fetch_add(threadIndex().first + 1u + m);
	}
}

struct Test
{
	const uint32_t value;
	Test(uint32_t other) : value(other) {}
	void worker (const int N, add_ptr<std::atomic<uint32_t>> a, add_cref<ThreadPool::ThreadIndex> threadIndex,
				add_cref<MM> m, add_cptr<Test> p, add_ref<Watcher> watcher)
	{
		watcher.insert(threadIndex().first, m);

		for (int i = 0; i < N; ++i)
		{
			a->fetch_add(p->value + threadIndex().first + 1u + m);
		}
	}
};

struct TestParameterlessThis
{
	const int N;
	add_ref<Watcher> watcher;
	add_ref<std::atomic<uint32_t>> value;
	TestParameterlessThis(int n, add_ref<std::atomic<uint32_t>> a, add_ref<Watcher> w)
		: N(n), watcher(w), value(a) {}
	void worker()
	{
		for (int i = 0; i < N; ++i)
		{
			value.fetch_add(1u);
			watcher.insert(make_unsigned(i), make_unsigned(i));
		}
	}
};

template<class Callback, class... Args>
void test_callback_impl (const uint32_t threadCount, const uint32_t startIndex, Callback&& cb, Args&&... args)
{
	typedef std::tuple<add_rref<Args>...> FormalParams;
	typedef typename tph::wrap_tuple<ThreadPool::ThreadIndex, FormalParams>::type Params;

	std::vector<ThreadPool::ThreadIndex> indices(threadCount);
	std::vector<tph::FakeMoveable<Params>> params(threadCount);

	for (uint32_t threadIndex = 0u; threadIndex < threadCount; ++threadIndex)
	{
		indices.at(threadIndex) = { (threadIndex + startIndex), threadCount };
		auto formalParams = std::forward_as_tuple(std::forward<Args>(args)...);
		static_assert(std::is_same_v<FormalParams, decltype(formalParams)>, "???");
		auto localParams = tph::make_wrapped_tuple<ThreadPool::ThreadIndex>(std::move(formalParams));
		tph::update_wrapped_tuple(localParams, indices.at(threadIndex));
		params.at(threadIndex).assign(std::move(localParams));
	}

	auto call = [cb](Args... other) -> void
	{
		cb(std::forward<Args>(other)...);
	};

	for (uint32_t threadIndex = 0u; threadIndex < threadCount; ++threadIndex)
	{
		std::apply(call, *params.at(threadIndex).get());
	}
}
bool test_callback ()
{
	std::vector<uint32_t> indices;

	auto reference = [&](add_ref<ThreadPool::ThreadIndex> index, add_cref<int> u)
	{
		std::cout << "ThreadIndex(" << index().first << ", " << index().second << ") ";
		std::cout << "u:" << u;
		std::cout << std::endl;
		indices.push_back(index().first);
	};
	auto value = [&](ThreadPool::ThreadIndex index, add_cref<int> u)
	{
		reference(index, u);
	};

	const int x = 123;
	const uint32_t threadCount = 4u;
	ThreadPool::ThreadIndex index; UNREF(index);
	std::cout << "By R-Value Reference\n";
	test_callback_impl(threadCount, 0u, value, ThreadPool::ThreadIndex(), x);
	std::cout << "By Reference\n";
	test_callback_impl(threadCount, threadCount, reference, index, x);

	std::vector<uint32_t> ref(threadCount * 2u);
	std::iota(ref.begin(), ref.end(), 0u);
	const bool result = ref == indices;
	return result;
}

void test_fake_moveable ()
{
	struct X
	{
		std::string s;
		X (std::string& t) : s(std::move(t)) {}
		X (X&& other) : s(std::move(other.s)) {}
		~X () { std::cout << __func__ << ' ' << std::quoted(s) << std::endl; }
	};

	{
		std::string s("Hello World!");
		std::cout << "String befere X " << std::quoted(s) << std::endl;
		X x(s);
		std::cout << "String after X " << std::quoted(s) << std::endl;
		tph::FakeMoveable<X> y;
		y.assign(std::move(x));
		tph::FakeMoveable<X> z(std::move(y));
	}
}

typedef typename ThreadPool::ThreadIndex KK;
std::ostream& operator<<(std::ostream& str, const KK& mm) { return str << "MMM:" << mm.index.first; }

bool test_all ()
{
	auto call1 = [](const std::string& s, float f, KK mv, int& i, KK& mr, double* d, KK* mp,
		KK cmv, const KK& cmr, const KK* cmp) -> std::string
	{
		UNREF(d);
		std::ostringstream cout;
		cout << "call1(" << std::quoted(s) << ", f:" << f << ", mv:" << mv << ", i:" << i
			<< ", mr:" << mr << ", mp:" << *mp;
		cout << ", cmv:" << cmv;
		cout << ", cmr:" << cmr;
		cout << ", cmp:" << *cmp;
		cout << ')';
		return cout.str();
	};
	KK m1({ 1,1 }); KK m2({ 2,2 }); KK m3({ 3,3 });
	int i = 123; double d = 2.71828;
	std::tuple<float, KK, int&, KK&, double*, KK*, KK, const KK&, const KK*> tx(3.14f, m1, i, m2, &d, &m3, m1, m2, &m3);
	auto wx1 = tph::make_wrapped_tuple<MM>(std::move(tx));
	auto wx2 = tph::make_wrapped_tuple<MM>(std::move(tx));
	auto wx3 = tph::make_wrapped_tuple<MM>(std::move(tx));
	tph::update_wrapped_tuple(wx1, m1);
	tph::update_wrapped_tuple(wx2, m2);
	tph::update_wrapped_tuple(wx3, m3);
	const std::string s1 = std::apply(call1, std::tuple_cat(std::make_tuple(std::string("WX1")), wx1));
	const std::string s2 = std::apply(call1, std::tuple_cat(std::make_tuple(std::string("WX2")), wx2));
	const std::string s3 = std::apply(call1, std::tuple_cat(std::make_tuple(std::string("WX3")), wx3));
	std::cout << s1 << std::endl;
	std::cout << s2 << std::endl;
	std::cout << s3 << std::endl;

	auto call2 = [](const std::string& s, KK mv, KK& mr, const KK& cmr, KK* mp, const KK* cmp) -> std::string
	{
		UNREF(s);
		std::ostringstream cout;
		cout << "call2(";
		cout << "mv:" << mv;
		cout << ", mr:" << mr;
		cout << ", cmr:" << cmr;
		cout << ", mp:" << *mp;
		cout << ", cmp:" << *cmp;
		cout << ')';
		return cout.str();
	};
	KK* pm1 = &m3;
	KK* pm2 = &m3;
	KK* pm3 = &m3;
	const KK* cpm = &m1;
	const KK& cmr = m2;
	auto ty1 = std::forward_as_tuple(m3, m1, cmr, pm1, cpm);
	auto ty2 = std::forward_as_tuple(m3, m1, cmr, pm2, cpm);
	auto ty3 = std::forward_as_tuple(m3, m1, cmr, pm3, cpm);
	static_assert(std::is_same_v<
		std::remove_reference_t<std::tuple_element_t<3, decltype(ty1)>>, add_ptr<KK>>, "???");
	static_assert(tph::is_pointer_reference<std::tuple_element_t<3, decltype(ty1)>>::value, "rrr");
	static_assert(tph::is_pointer_reference<std::tuple_element_t<4, decltype(ty1)>>::value, "rrr");
	static_assert(std::is_same_v<std::remove_reference_t<std::tuple_element_t<3, decltype(ty1)>>, KK*>, "999");
	static_assert(std::is_same_v<tph::pointer_reference_t<std::tuple_element_t<3, decltype(ty1)>>, KK*>, "999");
	static_assert(std::is_same_v<tph::pointer_reference_t<std::tuple_element_t<4, decltype(ty1)>>, const KK*>, "999");
	auto wy1 = tph::make_wrapped_tuple<MM>(std::move(ty1));
	auto wy2 = tph::make_wrapped_tuple<MM>(std::move(ty2));
	auto wy3 = tph::make_wrapped_tuple<MM>(std::move(ty3));
	tph::update_wrapped_tuple(wy1, m1);
	tph::update_wrapped_tuple(wy2, m2);
	tph::update_wrapped_tuple(wy3, m3);
	const std::string s4 = std::apply(call2, std::tuple_cat(std::make_tuple(std::string("WY1")), wy1));
	const std::string s5 = std::apply(call2, std::tuple_cat(std::make_tuple(std::string("WY2")), wy2));
	const std::string s6 = std::apply(call2, std::tuple_cat(std::make_tuple(std::string("WY3")), wy3));
	std::cout << s4 << std::endl;
	std::cout << s5 << std::endl;
	std::cout << s6 << std::endl;
	const bool c1 = s1 == R"(call1("WX1", f:3.14, mv:MMM:1, i:123, mr:MMM:1, mp:MMM:1, cmv:MMM:1, cmr:MMM:1, cmp:MMM:1))";
	const bool c2 = s2 == R"(call1("WX2", f:3.14, mv:MMM:2, i:123, mr:MMM:2, mp:MMM:2, cmv:MMM:2, cmr:MMM:2, cmp:MMM:2))";
	const bool c3 = s3 == R"(call1("WX3", f:3.14, mv:MMM:3, i:123, mr:MMM:3, mp:MMM:3, cmv:MMM:3, cmr:MMM:3, cmp:MMM:3))";
	const bool c4 = s4 == R"(call2(mv:MMM:1, mr:MMM:1, cmr:MMM:1, mp:MMM:1, cmp:MMM:1))";
	const bool c5 = s5 == R"(call2(mv:MMM:2, mr:MMM:2, cmr:MMM:2, mp:MMM:2, cmp:MMM:2))";
	const bool c6 = s6 == R"(call2(mv:MMM:3, mr:MMM:3, cmr:MMM:3, mp:MMM:3, cmp:MMM:3))";
	const bool res = c1 && c2 && c3 && c4 && c5 && c6;
	return res;
}

} // unnamed namespace

namespace
{

bool threadPoolSelfTest (uint32_t testIndex)
{
	const uint32_t threadCount = 2u;
	const uint32_t T0 = 0u;	UNREF(T0);
	const uint32_t T1 = 1u;	UNREF(T1);
	const uint32_t T2 = 2u;	UNREF(T2);
	ThreadPool tp(threadCount);

	const int N0 = 5;
	const int N1 = 7; 
	const int N2 = 11;

	const int K = 3;
	const uint32_t m = 1u;

	Test t1(K);
	Watcher watcher;
	std::atomic<uint32_t> a = 0;
	TestParameterlessThis t2(N2, a, watcher);

	auto routine = tp.getInterface(&routine_worker);
	auto method1 = tp.getInterface(&Test::worker, &t1, false);
	auto method2 = tp.getInterface(&TestParameterlessThis::worker, &t2);

	uint32_t expected = 0u;

	watcher.clear();
	routine->waitContinue({/* don't care */ }, &a, N0, MM(m), watcher);
	// It works on 2 thread because skip calculation in main thread.
	// Additionally the m is passed via value;
	expected += N0 * ((T0 + 1u + m) + (T1 + 1u + m));
	const bool c1 = expected == a;

	watcher.clear();
	method1->waitContinue(N1, &a, {/* don't care */ }, MM(m), &t1, watcher);
	// It is run on 2 threads as well, because we declare to do not do calculation in main thread.
	// The m is passed via const reference, so its value should be equal to m
	expected += N1 * ((K + T0 + 1u + m) + (K + T1 + 1u + m));
	const bool c2 = expected == a;

	watcher.clear();
	method2->waitContinue();
	// All 3, 2 threads and main thead should increment value N2 times.
	expected += N2 * 3;
	const bool c3 = expected == a;

	tp.waitFinish();

	if (!(c1 && c2 && c3))
	{
		std::cout << "Test " << testIndex << " failed, expected: " << expected << " got: " << a << std::endl;
		return false;
	}

	return true;
}

} // unnamed namespace

namespace vtf
{

bool ThreadPool::selfTest()
{
	test_fake_moveable();
	const bool c1 = test_all();
	const bool c2 = test_callback();

	const uint32_t testCount = 100u;
	std::vector<int> tests(testCount);
	for (uint32_t i = 0u; i < testCount; ++i)
	{
		tests.at(i) = threadPoolSelfTest(i) ? 1 : 0;
	}
	const auto fail = std::count(tests.begin(), tests.end(), 0);
	const auto pass = testCount - fail;
	std::cout << "Pass " << pass << " from " << testCount << " (" << ((double(pass) * 100.0)/double(testCount)) << "%)" << std::endl;
	return (c1 && c2 && (fail == 0u));
}

} // vtf
