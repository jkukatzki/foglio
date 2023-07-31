/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

 // External Includes
#include <shader.h>

namespace nap
{
	// Forward declares
	class Core;
	class RenderService;
	class FoglioService;

	// canvas shader sampler names 
	namespace uniform
	{
		namespace canvaswarp
		{
			inline constexpr const char* uboStructWarp = "UBO";
			inline constexpr const char* topLeft = "topLeft";
			inline constexpr const char* topRight = "topRight";
			inline constexpr const char* bottomLeft = "bottomLeft";
			inline constexpr const char* bottomRight = "bottomRight";

			namespace sampler
			{
				inline constexpr const char* inTexture = "inTexture";
			}

		}
	}


	class NAPAPI CanvasWarpShader : public Shader
	{
		RTTI_ENABLE(Shader)
	public:
		CanvasWarpShader(Core& core);

		/**
		 * Cross compiles the canvas GLSL shader code to SPIR-V, creates the shader module and parses all the uniforms and samplers.
		 * @param errorState contains the error if initialization fails.
		 * @return if initialization succeeded.
		 */
		virtual bool init(utility::ErrorState& errorState) override;

	private:
		RenderService* mRenderService = nullptr;
		FoglioService* mFoglioService = nullptr;
	};
}