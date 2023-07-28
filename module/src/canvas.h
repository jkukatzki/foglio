#pragma once

#include <nap/resource.h>
#include <nap/resourceptr.h>
#include <videoplayer.h>
#include <imagefromfile.h>
#include <planemesh.h>
#include <material.h>
#include <materialinstance.h>
#include <vertexattribute.h>




namespace nap
{	
	
	class Core;
	class Canvas : public Resource
	{
		RTTI_ENABLE(Resource)
	public:
		Canvas(Core& core);

		virtual bool init(utility::ErrorState& errorState) override;

		bool setup(utility::ErrorState& error);
		/**
		* @return MeshInstance (mPlane) as created during init().
		*/
		virtual MeshInstance& getMeshInstance();

		void constructCanvasMaterialInstance();

		ResourcePtr<PlaneMesh> getMesh();

		ResourcePtr<RenderTexture2D> getOutputTexture();

		MaterialInstance* getOutputMaterialInstance();

		ResourcePtr<VideoPlayer>		mVideoPlayer;
		ResourcePtr<ImageFromFile>		mMaskImage;
		ResourcePtr<Material>			mMaterialWithBindings;
		MaterialInstance				mVideoMaterialInstance;
		std::vector<glm::vec3>			mCornerOffsets = std::vector<glm::vec3>(4);

	private:
		ResourcePtr<RenderTexture2D>		mOutputTexture;
		ResourcePtr<PlaneMesh>				mPlane;
		RenderService*						mRenderService;
		Material*							mVideoMaterial;
		
		MaterialInstanceResource			mVideoMaterialInstResource;
		Material*							mCanvasOutputMaterial;
		MaterialInstance					mOutputMaterialInstance;
		MaterialInstanceResource			mOutputMaterialInstResource;
		nap::VertexAttribute<glm::vec3>*	mCornerOffsetAttribute;

		Sampler2DInstance* mYSampler = nullptr;
		Sampler2DInstance* mUSampler = nullptr;
		Sampler2DInstance* mVSampler = nullptr;

		UniformMat4Instance* mModelMatrixUniform = nullptr;
		UniformMat4Instance* mProjectMatrixUniform = nullptr;
		UniformMat4Instance* mViewMatrixUniform = nullptr;
		UniformStructInstance* mMVPStruct = nullptr;

		Sampler2DInstance* mMaskSampler = nullptr;


		UniformMat4Instance* ensureUniform(const std::string& uniformName, utility::ErrorState& error);

		Sampler2DInstance* ensureSampler(const std::string& samplerName, utility::ErrorState& error);

		void videoChanged(VideoPlayer& player);

	};
}