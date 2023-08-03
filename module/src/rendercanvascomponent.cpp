#include "rendercanvascomponent.h"
#include "canvaswarpshader.h"
#include "canvasinterfaceshader.h"

#include <entity.h>
#include <orthocameracomponent.h>
#include <nap/core.h>
#include <renderservice.h>
#include <renderglobals.h>
#include <glm/gtc/matrix_transform.hpp>
#include <transformcomponent.h>
#include <material.h>
#include <nap/resourceptr.h>
#include <rtti/objectptr.h>


// nap::rendercanvascomponent run time class definition
RTTI_BEGIN_CLASS(nap::RenderCanvasComponent)
RTTI_PROPERTY("Canvas", &nap::RenderCanvasComponent::mCanvas, nap::rtti::EPropertyMetaData::Default)
RTTI_PROPERTY("CornerOffsets", &nap::RenderCanvasComponent::mCornerOffsets, nap::rtti::EPropertyMetaData::Default)
RTTI_END_CLASS

RTTI_BEGIN_CLASS_NO_DEFAULT_CONSTRUCTOR(nap::RenderCanvasComponentInstance)
RTTI_CONSTRUCTOR(nap::EntityInstance&, nap::Component&)
RTTI_END_CLASS

namespace nap
{

	RenderCanvasComponentInstance::RenderCanvasComponentInstance(EntityInstance& entity, Component& resource) :
		RenderableComponentInstance(entity, resource),
		mPlane(new PlaneMesh(*entity.getCore()))
	{ }



	nap::Texture2D& RenderCanvasComponentInstance::getOutputTexture()
	{
		return *mCanvas->getOutputTexture();
	}

	nap::VideoPlayer* RenderCanvasComponentInstance::getVideoPlayer()
	{
		return mCanvas->mVideoPlayer.get();
	}

	bool RenderCanvasComponentInstance::init(utility::ErrorState& errorState)
	{
		if (!RenderableComponentInstance::init(errorState))
			return false;
		// Get resource
		RenderCanvasComponent* resource = getComponent<RenderCanvasComponent>();
		mTransformComponent = getEntityInstance()->findComponent<TransformComponentInstance>();
		// Now create a plane and initialize it
		// The plane is positioned on update based on current texture output size and transform component
		mCanvas = resource->mCanvas.get();
		if (!errorState.check(mCanvas != nullptr, "unable to find canvas resource in: %s", getEntityInstance()->mID))
			return false;
		//TODO: can this canvas init be removed?
		if (!errorState.check(mCanvas->init(errorState), "%s: unable to init canvas resource", resource->mID.c_str()))
			return false;

		// Setup double buffer target for internal render
		for (int target_idx = 0; target_idx < 2; target_idx++)
		{
			auto tex = getEntityInstance()->getCore()->getResourceManager()->createObject<RenderTexture2D>();
			ResourcePtr<RenderTexture2D> outputTexRef = mCanvas->getOutputTexture();
			tex->mWidth = outputTexRef->mWidth;
			tex->mHeight = outputTexRef->mHeight;
			tex->mFormat = outputTexRef->mFormat;
			tex->mUsage = ETextureUsage::Static;
			if (!tex->init(errorState))
			{
				errorState.fail("%s: Failed to initialize internal render texture", tex->mID.c_str());
				return false;
			}

			auto target = getEntityInstance()->getCore()->getResourceManager()->createObject<RenderTarget>();
			target->mColorTexture = tex;
			target->mClearColor = RGBAColor8(255, 255, 255, 0).convert<RGBAColorFloat>();
			target->mSampleShading = false;
			target->mRequestedSamples = ERasterizationSamples::One;
			if (!target->init(errorState))
			{
				errorState.fail("%s: Failed to initialize internal render target", target->mID.c_str());
				return false;
			}

			mDoubleBufferTarget[target_idx] = target;
		}

		// Extract render service
		mRenderService = getEntityInstance()->getCore()->getService<RenderService>();
		assert(mRenderService != nullptr);

		//canvas_material->mVertexAttributeBindings = mCanvas->mMaterialWithBindings->mVertexAttributeBindings;
		
		// TODO: move these renderable mesh creations into canvas resource?
		mHeadlessVideoMesh = mRenderService->createRenderableMesh(*mCanvas->getMesh().get(), *mCanvas->mCanvasMaterialItems["video"].mMaterialInstance, errorState);
		mHeadlessInterfaceMesh = mRenderService->createRenderableMesh(*mCanvas->getMesh().get(), *mCanvas->mCanvasMaterialItems["interface"].mMaterialInstance, errorState);
		if (!mHeadlessVideoMesh.isValid() || !mHeadlessInterfaceMesh.isValid())
			return false;
		try {
			mCanvas->mCanvasMaterialItems.at("mask");
			mHeadlessMaskMesh = mRenderService->createRenderableMesh(*mCanvas->getMesh().get(), *mCanvas->mCanvasMaterialItems["mask"].mMaterialInstance, errorState);
			if (!mHeadlessMaskMesh.isValid())
				return false;
		}
		catch (const std::out_of_range& e) {
		}
		
		// Create the renderable for the final headless material output
		mRenderableOutputMesh = mRenderService->createRenderableMesh(*mCanvas->getMesh().get(), *mCanvas->mCanvasMaterialItems["warp"].mMaterialInstance, errorState);
		if (!mRenderableOutputMesh.isValid())
			return false;
		mCornerOffsets = resource->mCornerOffsets;
		setWarpCornerUniforms();
		mCanvas->mCanvasMaterialItems["interface"].mUBO->getOrCreateUniform<UniformVec3Instance>(uniform::canvasinterface::mousePos)->setValue(glm::vec3());
		mCanvas->mCanvasMaterialItems["interface"].mUBO->getOrCreateUniform<UniformFloatInstance>(uniform::canvasinterface::frameThickness)->setValue(0.01);
		//TODO: put this in canvas resource or canvasgroup
		// Listen to video selection changes
		//mPlayer->VideoChanged.connect(mVideoChangedSlot);
		//videoChanged(*mPlayer);

		return true;

	}

	void RenderCanvasComponentInstance::drawAllHeadlessPasses() {
		if (mHeadlessMaskMesh.isValid()) {
			mCurrentInternalRT = mDoubleBufferTarget[0];
		}
		else {
			mCurrentInternalRT = mCanvas->mOutputRenderTarget;
		}	
		drawHeadlessPass(Canvas::CanvasMaterialTypes::VIDEO);
		if (mHeadlessMaskMesh.isValid()) {
			mCanvas->mCanvasMaterialItems["mask"].mSamplers["inTextureSampler"]->setTexture(*mCurrentInternalRT->mColorTexture);
			mCurrentInternalRT = mCanvas->mOutputRenderTarget;
			drawHeadlessPass(Canvas::CanvasMaterialTypes::MASK);
		}
		mCanvas->mCanvasMaterialItems["warp"].mSamplers["inTextureSampler"]->setTexture(*mCanvas->getOutputTexture());
	}

	void RenderCanvasComponentInstance::drawAndSetTextureInterface()
	{
		mCanvas->mCanvasMaterialItems["interface"].mSamplers["inTextureSampler"]->setTexture(*mCanvas->mOutputRenderTarget->mColorTexture);
		mCurrentInternalRT = mDoubleBufferTarget[0];
		drawHeadlessPass(Canvas::CanvasMaterialTypes::INTERFACE);
		mCanvas->mCanvasMaterialItems["warp"].mSamplers["inTextureSampler"]->setTexture(*mCurrentInternalRT->mColorTexture);
	}

	void RenderCanvasComponentInstance::draw(RenderableMesh renderableMesh, const DescriptorSet* descriptor_set)
	{
		// Get current command buffer, should be headless.
		VkCommandBuffer command_buffer = mRenderService->getCurrentCommandBuffer();

		//begin headless rendering
		mCurrentInternalRT->beginRendering();

		// Gather draw info
		MeshInstance& mesh_instance = renderableMesh.getMesh().getMeshInstance();
		GPUMesh& mesh = mesh_instance.getGPUMesh();

		// Get pipeline to to render with
		utility::ErrorState error_state;
		RenderService::Pipeline pipeline = mRenderService->getOrCreatePipeline(*mCurrentInternalRT, renderableMesh.getMesh(), renderableMesh.getMaterialInstance(), error_state);
		vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.mPipeline);
		vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.mLayout, 0, 1, &descriptor_set->mSet, 0, nullptr);

		// Bind buffers and draw
		const std::vector<VkBuffer>& vertexBuffers = renderableMesh.getVertexBuffers();
		const std::vector<VkDeviceSize>& vertexBufferOffsets = renderableMesh.getVertexBufferOffsets();

		vkCmdBindVertexBuffers(command_buffer, 0, vertexBuffers.size(), vertexBuffers.data(), vertexBufferOffsets.data());
		for (int index = 0; index < mesh_instance.getNumShapes(); ++index)
		{
			const IndexBuffer& index_buffer = mesh.getIndexBuffer(index);
			vkCmdBindIndexBuffer(command_buffer, index_buffer.getBuffer(), 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(command_buffer, index_buffer.getCount(), 1, 0, 0, 0);
		}

		mCurrentInternalRT->endRendering();
	}

	void RenderCanvasComponentInstance::drawHeadlessPass(Canvas::CanvasMaterialTypes type)
	{
		// Create orthographic projection matrix
		glm::ivec2 size = mCurrentInternalRT->getBufferSize();
		glm::mat4 proj_matrix = OrthoCameraComponentInstance::createRenderProjectionMatrix(0.0f, (float)size.x, 0.0f, (float)size.y);
		// Update the model matrix so that the plane mesh is of the same size as the render target
		// maybe do this only on update and store in member when window is resized for example to prevent unnecessary calculations
		computeModelMatrixFullscreen(mModelMatrix);
		switch (type) {
			case Canvas::CanvasMaterialTypes::VIDEO: {
				// Update matrices, projection and model are required
				mCanvas->mCanvasMaterialItems["video"].mModelMatrixUniform->setValue(mModelMatrix);
				mCanvas->mCanvasMaterialItems["video"].mProjectMatrixUniform->setValue(proj_matrix);
				mCanvas->mCanvasMaterialItems["video"].mViewMatrixUniform->setValue(glm::mat4());
				//get descriptor set
				const DescriptorSet* descriptor_set = &mCanvas->mCanvasMaterialItems["video"].mMaterialInstance->update();
				draw(mHeadlessVideoMesh, descriptor_set);
				break;
			}
			case Canvas::CanvasMaterialTypes::MASK: {
				// Update matrices, projection and model are required
				mCanvas->mCanvasMaterialItems["mask"].mModelMatrixUniform->setValue(mModelMatrix);
				mCanvas->mCanvasMaterialItems["mask"].mProjectMatrixUniform->setValue(proj_matrix);
				mCanvas->mCanvasMaterialItems["mask"].mViewMatrixUniform->setValue(glm::mat4());
				//get descriptor set
				const DescriptorSet* descriptor_set = &mCanvas->mCanvasMaterialItems["mask"].mMaterialInstance->update();
				draw(mHeadlessMaskMesh, descriptor_set);
				break;
			}
			case Canvas::CanvasMaterialTypes::INTERFACE: {
				// Update matrices, projection and model are required
				mCanvas->mCanvasMaterialItems["interface"].mModelMatrixUniform->setValue(mModelMatrix);
				mCanvas->mCanvasMaterialItems["interface"].mProjectMatrixUniform->setValue(proj_matrix);
				mCanvas->mCanvasMaterialItems["interface"].mViewMatrixUniform->setValue(glm::mat4());
				//get descriptor set
				const DescriptorSet* descriptor_set = &mCanvas->mCanvasMaterialItems["interface"].mMaterialInstance->update();
				draw(mHeadlessInterfaceMesh, descriptor_set);
				break;
			}
			default: {
				break;
			}
		}
	}


	void RenderCanvasComponentInstance::onDraw(IRenderTarget& renderTarget, VkCommandBuffer commandBuffer, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix)
	{
		// compute the model matrix with aspect ratio calculated with outputTexture and size and position with mTransformComponent
		computeModelMatrix(renderTarget, mModelMatrix, mCanvas->getOutputTexture(), mTransformComponent);
		mCanvas->mCanvasMaterialItems["warp"].mModelMatrixUniform->setValue(mModelMatrix);

		// Update matrices, projection and model are required
		mCanvas->mCanvasMaterialItems["warp"].mProjectMatrixUniform->setValue(projectionMatrix);
		mCanvas->mCanvasMaterialItems["warp"].mViewMatrixUniform->setValue(viewMatrix);

		// Get valid descriptor set
		const DescriptorSet& descriptor_set = mCanvas->mCanvasMaterialItems["warp"].mMaterialInstance->update();

		// Gather draw info
		MeshInstance& mesh_instance = mRenderableOutputMesh.getMesh().getMeshInstance();
		GPUMesh& mesh = mesh_instance.getGPUMesh();

		// Get pipeline to to render with
		utility::ErrorState error_state;
		RenderService::Pipeline pipeline = mRenderService->getOrCreatePipeline(renderTarget, mRenderableOutputMesh.getMesh(), *mCanvas->mCanvasMaterialItems["warp"].mMaterialInstance, error_state);
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.mPipeline);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.mLayout, 0, 1, &descriptor_set.mSet, 0, nullptr);

		// Bind buffers and draw
		const std::vector<VkBuffer>& vertexBuffers = mRenderableOutputMesh.getVertexBuffers();
		const std::vector<VkDeviceSize>& vertexBufferOffsets = mRenderableOutputMesh.getVertexBufferOffsets();

		vkCmdBindVertexBuffers(commandBuffer, 0, vertexBuffers.size(), vertexBuffers.data(), vertexBufferOffsets.data());
		for (int index = 0; index < mesh_instance.getNumShapes(); ++index)
		{
			const IndexBuffer& index_buffer = mesh.getIndexBuffer(index);
			vkCmdBindIndexBuffer(commandBuffer, index_buffer.getBuffer(), 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(commandBuffer, index_buffer.getCount(), 1, 0, 0, 0);
		}
	}

	bool RenderCanvasComponentInstance::isSupported(nap::CameraComponentInstance& camera) const
	{
		return camera.get_type().is_derived_from(RTTI_OF(OrthoCameraComponentInstance));
	}

	void RenderCanvasComponentInstance::computeModelMatrixFullscreen(glm::mat4& outMatrix) {
		//aspect ratio should be right because we set mTarget textures height and width to video players?
		// Transform to middle of target
		glm::ivec2 tex_size = mCurrentInternalRT->getBufferSize();
		outMatrix = glm::translate(glm::mat4(), glm::vec3(
			tex_size.x / 2.0f,
			tex_size.y / 2.0f,
			0.0f));

		// Scale to fit target
		outMatrix = glm::scale(outMatrix, glm::vec3(tex_size.x, tex_size.y, 1.0f));
	}

	void RenderCanvasComponentInstance::computeModelMatrix(const nap::IRenderTarget& target, glm::mat4& outMatrix, ResourcePtr<RenderTexture2D> canvas_output_texture, TransformComponentInstance* transform_comp)
	{
		//target should be window target

		glm::vec3 translate = transform_comp->getTranslate();
		glm::vec3 scale = transform_comp->getScale();
		glm::ivec2 canvas_tex_size = canvas_output_texture->getSize();
		glm::ivec2 tex_size = target.getBufferSize();
		glm::ivec2 new_size;
		outMatrix = glm::translate(glm::mat4(), glm::vec3(
			translate.x*tex_size.x + tex_size.x / 2.0f,
			translate.y*tex_size.y + tex_size.y / 2.0f,
			0.0f));
		// Scale correlating to target
		// Calculate ratio
		float canvas_ratio = static_cast<float>(canvas_tex_size.x) / static_cast<float>(canvas_tex_size.y);
		float window_ratio = static_cast<float>(tex_size.x) / static_cast<float>(tex_size.y);

		if (window_ratio > canvas_ratio) {
			tex_size.x = tex_size.y * canvas_ratio;
		}
		else {
			tex_size.y = tex_size.x / canvas_ratio;
		}

		outMatrix = glm::scale(outMatrix, glm::vec3(tex_size.x * scale.x, tex_size.y * scale.y, 1.0f));
		//outMatrix = glm::rotate(outMatrix, transform_comp->getRotate());

	}

	void RenderCanvasComponentInstance::setCornerOffsets(std::vector<glm::vec2> offsets) {
		mCornerOffsets[0] = offsets[0];
		mCornerOffsets[1] = offsets[1];
		mCornerOffsets[2] = offsets[2];
		mCornerOffsets[2] = offsets[3];
		setWarpCornerUniforms();
	}

	void RenderCanvasComponentInstance::setWarpCornerUniforms() {
		mCanvas->mCanvasMaterialItems["warp"].mUBO->getOrCreateUniform<UniformVec3Instance>(uniform::canvaswarp::topLeft)->setValue(glm::vec3(mCornerOffsets[0].x, mCornerOffsets[0].y*(-1), 0));
		mCanvas->mCanvasMaterialItems["warp"].mUBO->getOrCreateUniform<UniformVec3Instance>(uniform::canvaswarp::topRight)->setValue(glm::vec3(mCornerOffsets[1].x*(-1), mCornerOffsets[1].y*(-1), 0));
		mCanvas->mCanvasMaterialItems["warp"].mUBO->getOrCreateUniform<UniformVec3Instance>(uniform::canvaswarp::bottomLeft)->setValue(glm::vec3(mCornerOffsets[2].x, mCornerOffsets[2].y, 0));
		mCanvas->mCanvasMaterialItems["warp"].mUBO->getOrCreateUniform<UniformVec3Instance>(uniform::canvaswarp::bottomRight)->setValue(glm::vec3(mCornerOffsets[3].x*(-1), mCornerOffsets[3].y, 0));
	}

}