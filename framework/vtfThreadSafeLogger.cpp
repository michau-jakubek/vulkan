#include "vtfZDeletable.hpp"
#include "vtfVkUtils.hpp"
#include "vtfThreadSafeLogger.hpp"
#include <iterator>
#include <functional>

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


} // namespace vtf

