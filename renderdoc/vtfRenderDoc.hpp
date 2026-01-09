#ifndef __VTF_RENDERDOC_HPP_INCUDED__
#define __VTF_RENDERDOC_HPP_INCUDED__

#include "vtfZUtils.hpp"

namespace vtf
{

class RenderDoc
{
	[[maybe_unused]] void_ptr m_handle;
	bool cinit(bool throwException) const;
public:
	static bool isEnabled();
	RenderDoc();
	bool init(bool throwException = true);
	Version GetAPIVersion() const;
	void StartFrameCapture(ZInstance instance);
	void EndFrameCapture(ZInstance instance);
};

};

#endif // __VTF_RENDERDOC_HPP_INCUDED__

