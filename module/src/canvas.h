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
	
	class CanvasMaterialItem {
		public:
			CanvasMaterialItem();
			Material*									mMaterial = nullptr;
			MaterialInstanceResource*					mMaterialInstResource = nullptr;
			MaterialInstance*							mMaterialInstance;
			UniformStructInstance*						mMVPStruct = nullptr;
			UniformMat4Instance*						mModelMatrixUniform = nullptr;
			UniformMat4Instance*						mProjectMatrixUniform = nullptr;
			UniformMat4Instance*						mViewMatrixUniform = nullptr;
			std::map<std::string, Sampler2DInstance*>	mSamplers;
	};

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

		ResourcePtr<PlaneMesh> getMesh();

		ResourcePtr<RenderTexture2D> getOutputTexture();

		MaterialInstance* getOutputMaterialInstance();

		ResourcePtr<VideoPlayer>		mVideoPlayer;
		ResourcePtr<ImageFromFile>		mMaskImage;
		ResourcePtr<Material>			mMaterialWithBindings;

		std::map<std::string, CanvasMaterialItem> mCanvasMaterialItems;

		std::vector<glm::vec3>			mCornerOffsets = std::vector<glm::vec3>(4);

	private:
		ResourcePtr<RenderTexture2D>		mOutputTexture;
		ResourcePtr<PlaneMesh>				mPlane;
		RenderService*						mRenderService;

		Material*							mVideoMaterial;
		MaterialInstanceResource			mVideoMaterialInstResource;
		Sampler2DInstance* mYSampler = nullptr;
		Sampler2DInstance* mUSampler = nullptr;
		Sampler2DInstance* mVSampler = nullptr;


		Material*							mCanvasOutputMaterial;
		MaterialInstance					mOutputMaterialInstance;
		MaterialInstanceResource			mOutputMaterialInstResource;
		nap::VertexAttribute<glm::vec3>*	mCornerOffsetAttribute;

		

		UniformMat4Instance* mModelMatrixUniform = nullptr;
		UniformMat4Instance* mProjectMatrixUniform = nullptr;
		UniformMat4Instance* mViewMatrixUniform = nullptr;
		UniformStructInstance* mMVPStruct = nullptr;

		Sampler2DInstance* mMaskSampler = nullptr;

		//VIDEO: handles video texture and converts yuv color to rgb
		//CANVASMASK: sets mMaskImage as sample texture and sets textures transparency corresponding to image
		//CANVASOUTPUT: this renders the canvas for the main output window, holds vertex shader that offsets corners
		//CANVASUI: draws outline of plane onto texture //TODO: maybe add a new mOutputTexture for this?
		enum class CanvasMaterialTypes
		{
			VIDEO = 0, CANVASMASK = 1, CANVASOUTPUT = 2, CANVASUI = 3
		};


		UniformMat4Instance* ensureUniform(const std::string& uniformName, CanvasMaterialItem& materialItem, utility::ErrorState& error);

		Sampler2DInstance* ensureSampler(const std::string& samplerName, CanvasMaterialItem& materialItem, utility::ErrorState& error);

		void videoChanged(VideoPlayer& player);

		bool constructMaterialInstance(CanvasMaterialTypes type, utility::ErrorState& error);

	};
}