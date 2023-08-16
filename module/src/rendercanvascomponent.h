#pragma once

#include "canvas.h"

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
		ResourcePtr<Canvas>				mCanvas = nullptr;
		std::vector<glm::vec2>			mCornerOffsets = std::vector<glm::vec2>(4);
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

		Canvas* getCanvas() { return mCanvas; };

		std::vector<glm::vec2>	getCornerOffsets() { return mCornerOffsets; }

		void setCornerOffsets(std::vector<glm::vec2> offsets);

		void draw(RenderableMesh mesh, const DescriptorSet* descriptor_set);

		void drawHeadlessPass(Canvas::CanvasMaterialTypes type);

		void drawAllHeadlessPasses();

		void drawInterface(rtti::ObjectPtr<RenderTarget> interfaceTarget);

		void setFinalSampler(bool isInterface);

		void computeModelMatrix(const nap::IRenderTarget& target, glm::mat4& outMatrix, ResourcePtr<RenderTexture2D> canvas_output_texture, TransformComponentInstance* transform_comp);

		void computeModelMatrixFullscreen(glm::mat4& outMatrix);

	protected:

		virtual void onDraw(IRenderTarget& renderTarget, VkCommandBuffer commandBuffer, const glm::mat4& viewmatrix, const glm::mat4& projectionMatrix) override;

	private:
		using DoubleBufferedRenderTarget = std::array<rtti::ObjectPtr<RenderTarget>, 2>;

		Canvas*							mCanvas = nullptr;
		
		DoubleBufferedRenderTarget		mDoubleBufferTarget;
		rtti::ObjectPtr<RenderTarget>	mCurrentInternalRT;
		PlaneMesh*						mCanvasPlane;
		PlaneMesh*						mPlane;
		RenderableMesh					mRenderableOutputMesh;
		RenderableMesh					mHeadlessVideoMesh;
		RenderableMesh					mHeadlessInterfaceMesh;
		RenderableMesh					mHeadlessMaskMesh;
		
		std::vector<glm::vec2>			mCornerOffsets;

		TransformComponentInstance*	mTransformComponent = nullptr;

		RenderService*				mRenderService = nullptr;

		Vec3VertexAttribute*		mOffsetVec3Uniform = nullptr;

		glm::mat4x4					mModelMatrix;

		void setWarpCornerUniforms();

		//nap::Slot<VideoPlayer&> mVideoChangedSlot = { this, &RenderCanvasComponentInstance::videoChanged };

	};
}