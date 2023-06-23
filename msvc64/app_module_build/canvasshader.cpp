/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

 // Local includes
#include "canvasshader.h"
#include "renderservice.h"

// External includes
#include <nap/core.h>

// nap::VideoShader run time class definition 
RTTI_BEGIN_CLASS_NO_DEFAULT_CONSTRUCTOR(nap::CanvasShader)
RTTI_CONSTRUCTOR(nap::Core&)
RTTI_END_CLASS


//////////////////////////////////////////////////////////////////////////
// CanvasShader
//////////////////////////////////////////////////////////////////////////

namespace nap
{
	namespace shader
	{
		inline constexpr const char* canvas = "canvas";
	}

	CanvasShader::CanvasShader(Core& core) : Shader(core),
		mRenderService(core.getService<RenderService>()) { }


	bool CanvasShader::init(utility::ErrorState& errorState)
	{
		return load(shader::canvas, "someVertShader", 10, "someFragShader", 10, errorState);
	}
}