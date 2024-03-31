#pragma once

#include "canvaspasscomponent.h"
#include "canvasinterfaceshader.h"
#include "canvaswarpshader.h"

#include <component.h>
#include <rendercomponent.h>
#include <renderwindow.h>
#include <nap/resourceptr.h>
#include <rendertexture2d.h>
#include <planemesh.h>
#include <rendertarget.h>
#include <materialinstance.h>
#include <renderablemesh.h>
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
		float							mAspectRatio = 1.0;
		int								mResolution = 4;
		std::vector<glm::vec2>			mCornerOffsets = std::vector<glm::vec2>(4);
		ResourcePtr<ShaderFromFile>		mPostShader = nullptr;
		ResourcePtr<ImageFromFile>		mMask = nullptr;

		virtual void getDependentComponents(std::vector<rtti::TypeInfo>& components) const override;
	};

	class NAPAPI RenderCanvasComponentInstance : public RenderableComponentInstance
	{
		RTTI_ENABLE(RenderableComponentInstance)
	public:
		RenderCanvasComponentInstance(EntityInstance& entity, Component& resource);

		virtual bool init(utility::ErrorState& errorState) override;

		void setupCanvasPassComponents(utility::ErrorState& errorState);

		void renderPasses();

		void setIsControlWindow(bool isControlView);

		virtual bool isSupported(nap::CameraComponentInstance& camera) const override;

		ResourcePtr<RenderTexture2D> getFinalOutputTexture();

		ResourcePtr<RenderTarget>	getRenderTarget();

		std::vector<glm::vec2>	getCornerOffsets() { return mCornerOffsets; }

		enum class CanvasMaterialType
		{
			WARP = 0, INTERFACE = 1
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

		void setFinalSamplerTexture(RenderTexture2D* texture);

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
		
		bool mIsControlWindowDraw = false;

	protected:
		virtual void onDraw(IRenderTarget& renderTarget, VkCommandBuffer commandBuffer, const glm::mat4& viewmatrix, const glm::mat4& projectionMatrix) override;

	private:
		using DoubleBufferedRenderTarget = std::array<rtti::ObjectPtr<RenderTarget>, 2>;
		
		DoubleBufferedRenderTarget		mDoubleBufferTarget;
		ResourcePtr<RenderTarget>		mCurrentInternalRT;
		ResourcePtr<ImageFromFile>		mMask;
		ResourcePtr<RenderTarget>		mFinalRenderTarget;
		ResourcePtr<RenderTexture2D>	mFinalTexture;
		std::vector<glm::vec2>			mCornerOffsets;
		ResourcePtr<RenderWindow>		mPresentationWindow;

		std::vector<CanvasPassComponentInstance*>	mCanvasPassComponents;

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
	};
}