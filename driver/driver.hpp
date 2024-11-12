#ifndef __DRIVER_HPP_INCLUDED__
#define __DRIVER_HPP_INCLUDED__

#include "vtfCUtils.hpp"

struct DriverInitializer
{
	DriverInitializer ();
};

std::string getDriverFileName (add_ref<bool> success);

#endif // __DRIVER_HPP_INCLUDED__
