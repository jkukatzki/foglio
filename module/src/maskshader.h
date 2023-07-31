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
		namespace mask
		{
			namespace sampler
			{
				inline constexpr const char* MaskSampler = "maskTexture";	///< canvas shader mask sampler name
			}



		}
	}

	/**
		shader that turns pixels transparent according to image representing a mask
	 */
	class NAPAPI MaskShader : public Shader
	{
		RTTI_ENABLE(Shader)
	public:
		MaskShader(Core& core);

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