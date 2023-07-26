#pragma once
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
		namespace canvas
		{
			namespace sampler
			{
				inline constexpr const char* YSampler = "yTexture";	///< canvas shader Y sampler name
				inline constexpr const char* USampler = "uTexture";	///< canvas shader U sampler name
				inline constexpr const char* VSampler = "vTexture";	///< canvas shader V sampler name

				inline constexpr const char* MaskSampler = "maskTexture";	///< canvas shader mask sampler name
			}

		

		}
	}
	namespace vertexid
	{	
		namespace canvas {
			inline constexpr const char* CornerOffset = "CornerOffset";
		}
		
		namespace shader
		{
			namespace canvas {
				inline constexpr const char* CornerOffset = "in_CornerOffset";
			}
			
		}
	}

	/**
	 * Shader that converts YUV video textures, output by the nap::VideoPlayer, into an RGB image.
	 * Used by the nap::RenderCanvasComponent.
	 *
	 * The canvas shader exposes the following shader variables:
	 *
	 * ~~~~~{.frag}
	 *		uniform sampler2D yTexture;
	 *		uniform sampler2D uTexture;
	 *		uniform sampler2D vTexture;
	 * ~~~~
	 */
	class NAPAPI CanvasShader : public Shader
	{
		RTTI_ENABLE(Shader)
	public:
		CanvasShader(Core& core);

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