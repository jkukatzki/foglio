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

		PlaneMesh* getMesh();

		RenderTexture2D* getOutputTexture();

		ResourcePtr<VideoPlayer>		mVideoPlayer;
		ResourcePtr<ImageFromFile>		mMaskImage;
		ResourcePtr<Material>			mMaterialWithBindings;
		std::vector<glm::vec3>			mCornerOffsets = std::vector<glm::vec3>(4);

	private:
		RenderTexture2D*					mOutputTexture;
		RenderService*						mRenderService;
		PlaneMesh*							mPlane;
		Material*							mCanvasOutputMaterial;
		MaterialInstance					mOutputMaterialInstance;
		MaterialInstanceResource			mOutputMaterialInstResource;
		nap::VertexAttribute<glm::vec3>*	mCornerOffsetAttribute;
	};
}