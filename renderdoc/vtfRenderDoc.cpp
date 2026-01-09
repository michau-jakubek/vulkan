#include "renderdoc_app.h"
#include "vtfRenderDoc.hpp"
#include "vtfFilesystem.hpp"
#include "vtfCUtils.hpp"
#include "vtfVkUtils.hpp"

#include <Windows.h>
#include <iostream>

namespace vtf
{

bool RenderDoc::isEnabled()
{
#ifdef RENDERDOC_APP_HEADER
	return true;
#else
	return false;
#endif
}

RenderDoc::RenderDoc()
	: m_handle(nullptr)
{
}

bool RenderDoc::cinit(bool throwException) const
{
	return ((RenderDoc*)this)->init(throwException);
}

Version RenderDoc::GetAPIVersion() const
{
	cinit(true);
	RENDERDOC_API_1_7_0* api = (RENDERDOC_API_1_7_0*)m_handle;
	int major, minor, patch;
	api->GetAPIVersion(&major, &minor, &patch);

	return Version(major, minor, patch, 0);
}

void RenderDoc::StartFrameCapture(ZInstance instance)
{
	init(true);
	RENDERDOC_API_1_7_0* api = (RENDERDOC_API_1_7_0*)m_handle;
	api->StartFrameCapture(RENDERDOC_DEVICEPOINTER_FROM_VKINSTANCE(*instance), nullptr);
}

void RenderDoc::EndFrameCapture(ZInstance instance)
{
	init(true);
	RENDERDOC_API_1_7_0* api = (RENDERDOC_API_1_7_0*)m_handle;
	api->EndFrameCapture(RENDERDOC_DEVICEPOINTER_FROM_VKINSTANCE(*instance), nullptr);
}

} // namespace vtf
