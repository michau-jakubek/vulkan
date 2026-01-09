#include "renderdoc_app.h"
#include "vtfRenderDoc.hpp"
#include "vtfFilesystem.hpp"
#include "vtfCUtils.hpp"
#include "vtfVkUtils.hpp"

#include <Windows.h>
#include <iostream>

namespace vtf
{

bool RenderDoc::init(bool throwException)
{
	if (m_handle)
	{
		return true;
	}

	const std::string dll = fs::path(RENDERDOC_LIB).filename().string();
	HMODULE renderdoc = GetModuleHandleA(dll.c_str());
	ASSERTMSG(false == throwException || renderdoc, "Unable to get ", dll, " handle");
	if (nullptr == renderdoc)
	{
		return false;
	}

	pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)GetProcAddress(renderdoc, "RENDERDOC_GetAPI");
	ASSERTMSG(false == throwException || RENDERDOC_GetAPI, "Unable to get RENDERDOC_getAPI");
	if (nullptr == RENDERDOC_GetAPI)
	{
		return false;
	}

	RENDERDOC_GetAPI(RENDERDOC_Version::eRENDERDOC_API_Version_1_7_0, (void**)&m_handle);
	ASSERTMSG(false == throwException || m_handle, "Unable to get RenderDoc API");
	if (nullptr == m_handle)
	{
		return false;
	}

	return true;
}

} // namespace vtf
