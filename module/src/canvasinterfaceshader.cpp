/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

 // Local includes
#include "canvasinterfaceshader.h"
#include "foglioservice.h"
#include "renderservice.h"

// External includes
#include <nap/core.h>

// nap::VideoShader run time class definition 
RTTI_BEGIN_CLASS_NO_DEFAULT_CONSTRUCTOR(nap::CanvasInterfaceShader)
RTTI_CONSTRUCTOR(nap::Core&)
RTTI_END_CLASS


//////////////////////////////////////////////////////////////////////////
// CanvasShader
//////////////////////////////////////////////////////////////////////////

namespace nap
{
	namespace shader
	{
		inline constexpr const char* canvasinterface = "canvasinterface";
	}

	CanvasInterfaceShader::CanvasInterfaceShader(Core& core) : Shader(core),
		mRenderService(core.getService<RenderService>()),
		mFoglioService(core.getService<FoglioService>()) { }


	bool CanvasInterfaceShader::init(utility::ErrorState& errorState)
	{	
		std::string relative_path = utility::joinPath({ "shaders", utility::appendFileExtension(shader::canvasinterface, "vert") });
		const std::string vertex_shader_path = mFoglioService->getModule().findAsset(relative_path);
		if (!errorState.check(!vertex_shader_path.empty(), "%s: Unable to find %s vertex shader %s", mFoglioService->getModule().getName().c_str(), shader::canvasinterface, vertex_shader_path.c_str()))
			return false;

		relative_path = utility::joinPath({ "shaders", utility::appendFileExtension(shader::canvasinterface, "frag") });
		const std::string fragment_shader_path = mFoglioService->getModule().findAsset(relative_path);
		if (!errorState.check(!fragment_shader_path.empty(), "%s: Unable to find %s frag shader %s", mFoglioService->getModule().getName().c_str(), shader::canvasinterface, fragment_shader_path.c_str()))
			return false;

		// Read vert shader file
		std::string vert_source;
		if (!errorState.check(utility::readFileToString(vertex_shader_path, vert_source, errorState), "Unable to read %s vertex shader file", shader::canvasinterface))
			return false;

		// Read frag shader file
		std::string frag_source;
		if (!errorState.check(utility::readFileToString(fragment_shader_path, frag_source, errorState), "Unable to read %s fragment shader file", shader::canvasinterface))
			return false;

		//Compile shader
		return this->load(shader::canvasinterface, vert_source.data(), vert_source.size(), frag_source.data(), frag_source.size(), errorState);
	}
}