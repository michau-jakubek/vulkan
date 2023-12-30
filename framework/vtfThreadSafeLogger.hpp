#ifndef __VTF_THREADSAFELOGGER_HPP_INCLUDED__
#define __VTF_THREADSAFELOGGER_HPP_INCLUDED__

#include <iostream>
#include <mutex>
#include <sstream>

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

} // namespace vtf

#endif // __VTF_THREADSAFELOGGER_HPP_INCLUDED__
