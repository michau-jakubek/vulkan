#include <iostream>
#include "vtfThreadSafeLogger.hpp"

namespace vtf
{

std::mutex Logger_mMutex;
thread_local std::stringstream Logger_mInput;

} // namespace vtf

