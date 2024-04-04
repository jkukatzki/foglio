#pragma once

#include <component.h>
#include <rendercomponent.h>
#include <nap/resourceptr.h>
#include <nap/resourcemanager.h>
#include <rendertarget.h>
#include <rendertexture2d.h>
#include <materialinstance.h>
#include <renderablemesh.h>
#include <cameracomponent.h>
#include <planemesh.h>
#include <material.h>
#include <videoplayer.h>
#include <imagefromfile.h>




namespace nap
{
	class NAPAPI ImageSamplerOverride {
	public: 
		std::string uniformName = "";
		ResourcePtr<ImageFromFile> image;
	};
	// Forward declares
	class CanvasPassComponentInstance;

	class NAPAPI CanvasPassComponent : public RenderableComponent
	{
		RTTI_ENABLE(RenderableComponent)
		DECLARE_COMPONENT(CanvasPassComponent, CanvasPassComponentInstance)

	public:
		ResourcePtr<ShaderFromFile>				mPassShader = nullptr;
		std::vector<ImageSamplerOverride>		mImageOverrides = {};
	};

	class NAPAPI CanvasPassComponentInstance : public RenderableComponentInstance
	{
		RTTI_ENABLE(RenderableComponentInstance)
	public:
		CanvasPassComponentInstance(EntityInstance& entity, Component& resource);

		virtual bool init(utility::ErrorState& errorState) override;

		bool initPassTargetAndTexture(ResourcePtr<RenderTarget> canvasRenderTarget, ResourcePtr<RenderTexture2D> canvasTexture, utility::ErrorState& errorState);

		ResourcePtr<RenderTexture2D> getOutputTexture();

		bool constructMaterial(utility::ErrorState& errorState);

		void draw();

		void setInTextureSampler(ResourcePtr<RenderTexture2D> inTexture);

		void computeModelMatrixFullscreen(glm::mat4& outMatrix, float sizeX, float sizeY);

		bool setupPlaneMesh(ResourcePtr<PlaneMesh> planeMesh, nap::utility::ErrorState errorState);

		UniformMat4Instance* ensureUniformMat4(const std::string& uniformName, UniformStructInstance* structInstance, utility::ErrorState& error);
		UniformVec3Instance* ensureUniformVec3(const std::string& uniformName, UniformStructInstance* structInstance, utility::ErrorState& error);
		UniformFloatInstance* ensureUniformFloat(const std::string& uniformName, UniformStructInstance* structInstance, utility::ErrorState& error);
		Sampler2DInstance* ensureSampler(const std::string& samplerName, MaterialInstance* materialInstance, utility::ErrorState& error);
		bool constructTextureAndRenderTarget(ResourcePtr<RenderTarget>& renderTarget, ResourcePtr<RenderTexture2D>& texture, bool transparent, utility::ErrorState& error);

		bool createUniforms(utility::ErrorState& errorState);

		std::vector<ResourcePtr<VideoPlayer>>	mVideoPlayers;
		std::vector<ResourcePtr<ImageFromFile>>	mImages;

	protected:

		virtual void onDraw(IRenderTarget& renderTarget, VkCommandBuffer commandBuffer, const glm::mat4& viewmatrix, const glm::mat4& projectionMatrix) override;


	private:
		using DoubleBufferedRenderTarget = std::array<rtti::ObjectPtr<RenderTarget>, 2>;
		ResourcePtr<Material>						mMaterial = nullptr;
		std::unique_ptr<MaterialInstanceResource>	mMaterialInstResource = nullptr;

		MaterialInstance* mMaterialInstance = nullptr;
		ResourcePtr<ShaderFromFile> mPassShader = nullptr;
		UniformStructInstance* mMVPStruct = nullptr;
		UniformMat4Instance* mModelMatrixUniform = nullptr;
		UniformMat4Instance* mProjectMatrixUniform = nullptr;
		UniformMat4Instance* mViewMatrixUniform = nullptr;
		std::map<std::string, Sampler2DInstance*>	mSamplers;

		UniformStructInstance* mUBO = nullptr;

		RenderableMesh mRenderableMesh;

		DoubleBufferedRenderTarget		mDoubleBufferTarget;
		ResourcePtr<RenderTarget>		mCurrentInternalRT;
		
		ResourcePtr<RenderTarget>		mFinalRenderTarget;
		ResourcePtr<RenderTexture2D>	mFinalTexture;

		ResourceManager* mResourceManager;
		std::vector<ResourcePtr<VideoPlayer>> mVideoResources;
		std::vector<ResourcePtr<ImageFromFile>> mImageResources;

		float* mAspectRatio = nullptr;
		int* mResolution = nullptr;

		ResourcePtr<PlaneMesh>			mHeadlessPlaneMesh; //1x1 plane mesh
		ResourcePtr<PlaneMesh>			mFinalPlaneMesh;	//10x10 plane mesh because warp vertex shader needs more geometry for uv data so it doesn't get distorted

		RenderService* mRenderService = nullptr;

		Vec3VertexAttribute* mOffsetVec3Uniform = nullptr;

		glm::mat4x4					mModelMatrix;

		void videoChanged(VideoPlayer& player, CanvasPassComponentInstance* pass);

		
	};
}