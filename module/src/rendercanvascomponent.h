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
#include <material.h>


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
		float							mAspectRatio;
		int								mResolution;
		std::vector<glm::vec2>			mCornerOffsets = std::vector<glm::vec2>(4);
		ResourcePtr<Material>			mPostShader = nullptr;
		ResourcePtr<ImageFromFile>		mMask = nullptr;
		
	};

	class NAPAPI RenderCanvasComponentInstance : public RenderableComponentInstance
	{
		RTTI_ENABLE(RenderableComponentInstance)
	public:
		RenderCanvasComponentInstance(EntityInstance& entity, Component& resource);

		virtual bool init(utility::ErrorState& errorState) override;

		virtual bool isSupported(nap::CameraComponentInstance& camera) const override;

		ResourcePtr<RenderTexture2D> getOutputTexture();

		VideoPlayer* getVideoPlayer();

		std::vector<glm::vec2>	getCornerOffsets() { return mCornerOffsets; }

		enum class CanvasMaterialType
		{
			VIDEO = 0, MASK = 1, WARP = 2, INTERFACE = 3
		};
		struct CanvasPass {
			ResourcePtr<Material>						mMaterial = nullptr;
			std::unique_ptr<MaterialInstanceResource>	mMaterialInstResource = nullptr;
			//instances shouldnt need to be smart pointers here cause they're already unique_ptrs on creation
			MaterialInstance* mMaterialInstance;
			UniformStructInstance* mMVPStruct = nullptr;
			UniformMat4Instance* mModelMatrixUniform = nullptr;
			UniformMat4Instance* mProjectMatrixUniform = nullptr;
			UniformMat4Instance* mViewMatrixUniform = nullptr;
			std::map<std::string, Sampler2DInstance*>	mSamplers;
			UniformStructInstance* mUBO;
			RenderableMesh mRenderableMesh;
		};
		void setCornerOffsets(std::vector<glm::vec2> offsets);

		void draw(RenderableMesh mesh, const DescriptorSet* descriptor_set);

		void drawHeadlessPass(CanvasPass& pass);

		void drawAllHeadlessPasses();

		void drawInterface(rtti::ObjectPtr<RenderTarget> interfaceTarget);

		void setFinalSampler(bool isInterface);

		void computeModelMatrix(const nap::IRenderTarget& target, glm::mat4& outMatrix, ResourcePtr<RenderTexture2D> canvas_output_texture, TransformComponentInstance* transform_comp);

		void computeModelMatrixFullscreen(glm::mat4& outMatrix);

		std::unique_ptr<CanvasPass>		mCustomPostPass = nullptr;

		
		std::unordered_map<CanvasMaterialType, CanvasPass> mStockCanvasPasses;

		bool constructCanvasPassItem(CanvasMaterialType type, utility::ErrorState error);
		UniformMat4Instance* ensureUniformMat4(const std::string& uniformName, UniformStructInstance* structInstance, utility::ErrorState& error);
		UniformVec3Instance* ensureUniformVec3(const std::string& uniformName, UniformStructInstance* structInstance, utility::ErrorState& error);
		UniformFloatInstance* ensureUniformFloat(const std::string& uniformName, UniformStructInstance* structInstance, utility::ErrorState& error);
		Sampler2DInstance* ensureSampler(const std::string& samplerName, MaterialInstance* materialInstance, utility::ErrorState& error);
		bool constructTextureAndRenderTarget(ResourcePtr<RenderTarget>& renderTarget, ResourcePtr<RenderTexture2D>& texture, bool transparent, utility::ErrorState& error);
		
		bool mIsControlViewDraw = false;

	protected:

		virtual void onDraw(IRenderTarget& renderTarget, VkCommandBuffer commandBuffer, const glm::mat4& viewmatrix, const glm::mat4& projectionMatrix) override;

	private:
		using DoubleBufferedRenderTarget = std::array<rtti::ObjectPtr<RenderTarget>, 2>;
		//TODO: make this a ResourcePtr<Canvas>?
		
		DoubleBufferedRenderTarget		mDoubleBufferTarget;
		ResourcePtr<RenderTarget>		mCurrentInternalRT;
		ResourcePtr<ImageFromFile>		mMask;
		VideoPlayer*					mVideoPlayer = nullptr;
		ResourcePtr<RenderTarget>		mFinalRenderTarget;
		ResourcePtr<RenderTexture2D>	mFinalTexture;
		std::vector<glm::vec2>			mCornerOffsets;

		float*							mAspectRatio = nullptr;
		int*							mResolution = nullptr;

		ResourcePtr<PlaneMesh>			mHeadlessPlaneMesh; //1x1 plane mesh
		ResourcePtr<PlaneMesh>			mFinalPlaneMesh;	//10x10 plane mesh because warp vertex shader needs more geometry for uv data so it doesn't get distorted

		TransformComponentInstance*	mTransformComponent = nullptr;

		RenderService*				mRenderService = nullptr;

		Vec3VertexAttribute*		mOffsetVec3Uniform = nullptr;

		glm::mat4x4					mModelMatrix;

		bool setupPlaneMesh(ResourcePtr<PlaneMesh> planeMesh, int resX, int resY, nap::utility::ErrorState errorState);

		void setWarpCornerUniforms();

		void videoChanged(VideoPlayer& player);
		nap::Slot<VideoPlayer&> mVideoChangedSlot = { this, &RenderCanvasComponentInstance::videoChanged };
	};
}