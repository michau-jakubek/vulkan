#include "vtfRenderDoc.hpp"

namespace vtf
{

void renderDocAssert()
{
	ASSERTFALSE("RENDERDOC_APP_HEADER not defined");
}

bool RenderDoc::isEnabled()
{
#ifdef RENDERDOC_APP_HEADER
	return true;
#else
	return false;
#endif
}

bool RenderDoc::cinit(bool throwException) const
{
	UNREF(throwException);
	renderDocAssert();
	return false;
}

RenderDoc::RenderDoc()
	: m_handle(nullptr)
{
}

bool RenderDoc::init(bool throwException)
{
	UNREF(throwException);
	renderDocAssert();
	return false;
}

Version RenderDoc::GetAPIVersion() const
{
	renderDocAssert();
	return Version(0, 0);
}

void RenderDoc::StartFrameCapture(ZInstance)
{
	renderDocAssert();
}

void RenderDoc::EndFrameCapture(ZInstance)
{
	renderDocAssert();
}

} // namespace vtf