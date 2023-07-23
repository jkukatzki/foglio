#pragma once

#include <component.h>
#include <rendercomponent.h>
#include <nap/resourceptr.h>
#include <rendertexture2d.h>
#include <planemesh.h>
#include <rendertarget.h>
#include <materialinstance.h>
#include <renderablemesh.h>
#include <videoplayer.h>
#include <imagefromfile.h>
#include <foglioservice.h>
#include <transformcomponent.h>

namespace nap
{
	// Forward declares
	class RenderCanvasComponentInstance;

	class NAPAPI RenderCanvasComponent : public RenderableComponent
	{
		RTTI_ENABLE(RenderableComponent)
		DECLARE_COMPONENT(RenderCanvasComponent, RenderCanvasComponentInstance)

	public:
		ResourcePtr<VideoPlayer>		mVideoPlayer = nullptr;
		ResourcePtr<ImageFromFile>		mMaskImage = nullptr;
		int								mVideoIndex = 0;
	};

	class NAPAPI RenderCanvasComponentInstance : public RenderableComponentInstance
	{
		RTTI_ENABLE(RenderableComponentInstance)
	public:
		RenderCanvasComponentInstance(EntityInstance& entity, Component& resource);

		virtual bool init(utility::ErrorState& errorState) override;

		virtual bool isSupported(nap::CameraComponentInstance& camera) const override;

		Texture2D& getOutputTexture();

		VideoPlayer* getVideoPlayer();

		void draw();

	protected:

		virtual void onDraw(IRenderTarget& renderTarget, VkCommandBuffer commandBuffer, const glm::mat4& viewmatrix, const glm::mat4& projectionMatrix) override;

	private:
		VideoPlayer*					mPlayer = nullptr;
		RenderTarget					mTarget;
		ResourcePtr<RenderTexture2D>	mOutputTexture = nullptr;

		PlaneMesh						mPlane;
		Material*						mCanvasOutputMaterial;
		MaterialInstance				mOutputMaterialInstance;
		MaterialInstanceResource		mOutputMaterialInstResource;
		RenderableMesh					mRenderableOutputMesh;
		
		TransformComponentInstance*	mTransformComponent = nullptr;

		RenderService*				mRenderService = nullptr;

		UniformMat4Instance*		mModelMatrixUniform = nullptr;
		UniformMat4Instance*		mProjectMatrixUniform = nullptr;
		UniformMat4Instance*		mViewMatrixUniform = nullptr;
		UniformStructInstance*		mMVPStruct = nullptr;
		Sampler2DInstance*			mYSampler = nullptr;
		Sampler2DInstance*			mUSampler = nullptr;
		Sampler2DInstance*			mVSampler = nullptr;
		glm::mat4x4					mModelMatrix;


		UniformMat4Instance* ensureUniform(const std::string& uniformName, utility::ErrorState& error);

		Sampler2DInstance* ensureSampler(const std::string& samplerName, utility::ErrorState& error);

		void videoChanged(VideoPlayer& player);

		nap::Slot<VideoPlayer&> mVideoChangedSlot = { this, &RenderCanvasComponentInstance::videoChanged };

	};
}