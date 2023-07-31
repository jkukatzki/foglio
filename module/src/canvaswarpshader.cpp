/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

 // Local includes
#include "canvaswarpshader.h"
#include "foglioservice.h"
#include "renderservice.h"

// External includes
#include <nap/core.h>

// nap::VideoShader run time class definition 
RTTI_BEGIN_CLASS_NO_DEFAULT_CONSTRUCTOR(nap::CanvasWarpShader)
RTTI_CONSTRUCTOR(nap::Core&)
RTTI_END_CLASS


//////////////////////////////////////////////////////////////////////////
// CanvasWarpShader
//////////////////////////////////////////////////////////////////////////

namespace nap
{
	namespace shader
	{
		inline constexpr const char* canvaswarp = "warp";
	}

	CanvasWarpShader::CanvasWarpShader(Core& core) : Shader(core),
		mRenderService(core.getService<RenderService>()),
		mFoglioService(core.getService<FoglioService>()) { }


	bool CanvasWarpShader::init(utility::ErrorState& errorState)
	{
		std::string relative_path = utility::joinPath({ "shaders", utility::appendFileExtension(shader::canvaswarp, "vert") });
		const std::string vertex_shader_path = mFoglioService->getModule().findAsset(relative_path);
		if (!errorState.check(!vertex_shader_path.empty(), "%s: Unable to find %s vertex shader %s", mFoglioService->getModule().getName().c_str(), shader::canvaswarp, vertex_shader_path.c_str()))
			return false;

		relative_path = utility::joinPath({ "shaders", utility::appendFileExtension(shader::canvaswarp, "frag") });
		const std::string fragment_shader_path = mFoglioService->getModule().findAsset(relative_path);
		if (!errorState.check(!fragment_shader_path.empty(), "%s: Unable to find %s frag shader %s", mFoglioService->getModule().getName().c_str(), shader::canvaswarp, fragment_shader_path.c_str()))
			return false;

		// Read vert shader file
		std::string vert_source;
		if (!errorState.check(utility::readFileToString(vertex_shader_path, vert_source, errorState), "Unable to read %s vertex shader file", "canvas"))
			return false;

		// Read frag shader file
		std::string frag_source;
		if (!errorState.check(utility::readFileToString(fragment_shader_path, frag_source, errorState), "Unable to read %s fragment shader file", shader::canvaswarp))
			return false;

		//Compile shader
		return this->load(shader::canvaswarp, vert_source.data(), vert_source.size(), frag_source.data(), frag_source.size(), errorState);
	}
}