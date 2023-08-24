#ifndef __VTF_THREADSAFELOGGER_HPP_INCLUDED__
#define __VTF_THREADSAFELOGGER_HPP_INCLUDED__

#include <iterator>
#include <iostream>
#include <mutex>
#include <sstream>

namespace vtf
{

extern	std::mutex Logger_mMutex;
thread_local extern std::stringstream Logger_mInput;

struct Logger
{
    template<class X>
	Logger& operator<< (const X& x) {
        mInput << std::noskipws << x;
        return *this;
    }
	Logger& operator<< (std::ostream& (&maybeNewLine)(std::ostream&))
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
	Logger () : mOutput(std::cout), mInput(getInput()), mMutex(getMutex()) {}
	Logger (const Logger& other) : mOutput(other.mOutput), mInput(other.mInput), mMutex(other.mMutex) {}
protected:
	std::ostream& mOutput;
	std::stringstream& mInput;
	std::mutex& mMutex;

	std::stringstream& getInput()
	{
		return Logger_mInput;
	}
	std::mutex& getMutex()
	{
		return Logger_mMutex;
	}
};

} // namespace vtf

#endif // __VTF_THREADSAFELOGGER_HPP_INCLUDED__
