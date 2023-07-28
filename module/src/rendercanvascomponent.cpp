#include "rendercanvascomponent.h"
#include "canvasshader.h"

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
	RTTI_PROPERTY("VideoPlayer", &nap::RenderCanvasComponent::mVideoPlayer, nap::rtti::EPropertyMetaData::Required)
	RTTI_PROPERTY("MaskImage", &nap::RenderCanvasComponent::mMaskImage, nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("Index", &nap::RenderCanvasComponent::mVideoIndex, nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("Canvas", &nap::RenderCanvasComponent::mCanvas, nap::rtti::EPropertyMetaData::Default)
RTTI_END_CLASS

RTTI_BEGIN_CLASS_NO_DEFAULT_CONSTRUCTOR(nap::RenderCanvasComponentInstance)
	RTTI_CONSTRUCTOR(nap::EntityInstance&, nap::Component&)
RTTI_END_CLASS

namespace nap
{
	
	RenderCanvasComponentInstance::RenderCanvasComponentInstance(EntityInstance& entity, Component& resource) :
		RenderableComponentInstance(entity, resource),
		mTarget(*entity.getCore()),
		mPlane(new PlaneMesh(*entity.getCore())),
		mCanvasOutputMaterial(new Material(*entity.getCore()))
		{ }

	

	nap::Texture2D& RenderCanvasComponentInstance::getOutputTexture()
	{
		return mTarget.getColorTexture();
	}

	nap::VideoPlayer* RenderCanvasComponentInstance::getVideoPlayer()
	{
		return mPlayer;
	}

	bool RenderCanvasComponentInstance::init(utility::ErrorState& errorState)
	{
		mTransformComponent = getEntityInstance()->findComponent<TransformComponentInstance>();
		if (!RenderableComponentInstance::init(errorState))
			return false;
		// Get resource
		RenderCanvasComponent* resource = getComponent<RenderCanvasComponent>();

		
		// T          O                D              O
		// create new member plane that doesnt carry canvasshader, only use canvasshader in final output in ondraw()
		

		

		// Now create a plane and initialize it
		// The plane is positioned on update based on current texture output size and transform component
		mCanvas = resource->mCanvas.get();
		//if (errorState.check(mCanvas->init(errorState), "%s: unable to init canvas resource", resource->mID.c_str()))
			//return false;
		if (!errorState.check(mCanvas != nullptr, "unable to find canvas resource in: %s", getEntityInstance()->mID))
			return false;
		//TODO: add error check
		mCanvas->init(errorState);

		// Extract player
		mPlayer = resource->mCanvas->mVideoPlayer.get();
		if (!errorState.check(mPlayer != nullptr, "%s: no video player", resource->mID.c_str()))
			return false;
		// Setup output texture
		
		mOutputTexture = mCanvas->getOutputTexture();
		// Setup render target and initialize
		mTarget.mClearColor = RGBAColor8(255, 255, 255, 255).convert<RGBAColorFloat>();
		mTarget.mColorTexture = mOutputTexture;
		mTarget.mSampleShading = true;
		mTarget.mRequestedSamples = ERasterizationSamples::One;
		if (!mTarget.init(errorState))
			return false;
		mMask = resource->mCanvas->mMaskImage.get();

		
		
		// Extract render service
		mRenderService = getEntityInstance()->getCore()->getService<RenderService>();
		assert(mRenderService != nullptr);


		// create canvas material
		Material* canvas_material = mRenderService->getOrCreateMaterial<CanvasShader>(errorState);
		if (!errorState.check(canvas_material != nullptr, "%s: unable to get or create canvas material", resource->mID.c_str()))
			return false;
		if (canvas_material == nullptr) {
			return false;
		}

		canvas_material->mVertexAttributeBindings = mCanvas->mMaterialWithBindings->mVertexAttributeBindings;
		
		
		

		// Create resource for the final output canvas material instance
		mOutputMaterialInstResource.mBlendMode = EBlendMode::AlphaBlend;
		mOutputMaterialInstResource.mDepthMode = EDepthMode::NoReadWrite;
		mOutputMaterialInstResource.mMaterial = canvas_material; //replace canvas material with new 

		// Initialize canvas material instance, used for rendering canvas
		if (!mOutputMaterialInstance.init(*mRenderService, mOutputMaterialInstResource, errorState))
			return false;

		// Ensure the mvp struct is available
		mMVPStruct = mOutputMaterialInstance.getOrCreateUniform(uniform::mvpStruct);
		if (!errorState.check(mMVPStruct != nullptr, "%s: Unable to find uniform MVP struct: %s in material: %s",
			this->mID.c_str(), uniform::mvpStruct, mOutputMaterialInstance.getMaterial().mID.c_str()))
			return false;

		

		// Get all matrices
		mModelMatrixUniform = ensureUniform(uniform::modelMatrix, errorState);
		mProjectMatrixUniform = ensureUniform(uniform::projectionMatrix, errorState);
		mViewMatrixUniform = ensureUniform(uniform::viewMatrix, errorState);

		if (mModelMatrixUniform == nullptr || mProjectMatrixUniform == nullptr || mViewMatrixUniform == nullptr)
			return false;

		
		

		// Get sampler inputs to update from canvas material
		mYSampler = ensureSampler(uniform::canvas::sampler::YSampler, errorState);
		mUSampler = ensureSampler(uniform::canvas::sampler::USampler, errorState);
		mVSampler = ensureSampler(uniform::canvas::sampler::VSampler, errorState);
		mMaskSampler = ensureSampler(uniform::canvas::sampler::MaskSampler, errorState);
		if (mMask != nullptr) {
			
			mMaskSampler->setTexture(*mMask);
		}
		else {
			RenderTexture2D* blank = new RenderTexture2D(*getEntityInstance()->getCore());
			blank->mClearColor = RGBAColor8(255, 255, 255, 255).convert<RGBAColorFloat>();
			mMaskSampler->setTexture(*blank);
		}
		
		
		if (mYSampler == nullptr || mUSampler == nullptr || mVSampler == nullptr)
			return false;

		// Create the renderable mesh, which represents a valid mesh / material combination
		mRenderableOutputMesh = mRenderService->createRenderableMesh(*mCanvas->getMesh().get(), mCanvas->mVideoMaterialInstance, errorState);
		if (!mRenderableOutputMesh.isValid())
			return false;

		// Listen to video selection changes
		mPlayer->VideoChanged.connect(mVideoChangedSlot);

		videoChanged(*mPlayer);

		return true;



	}

	

	
	void RenderCanvasComponentInstance::draw()
	{
		// Get current command buffer, should be headless.
		VkCommandBuffer command_buffer = mRenderService->getCurrentCommandBuffer();

		// Create orthographic projection matrix
		glm::ivec2 size = mTarget.getBufferSize();

		// Create projection matrix
		glm::mat4 proj_matrix = OrthoCameraComponentInstance::createRenderProjectionMatrix(0.0f, (float)size.x, 0.0f, (float)size.y);

		// do onDraw() but headless
		mTarget.beginRendering();
		
		// Update the model matrix so that the plane mesh is of the same size as the render target
		// maybe do this only on update and store in member when window is resized for example to prevent unnecessary calculations
		computeModelMatrixFullscreen(mModelMatrix);
		mModelMatrixUniform->setValue(mModelMatrix);

		// Update matrices, projection and model are required
		mProjectMatrixUniform->setValue(proj_matrix);
		mViewMatrixUniform->setValue(glm::mat4());

		// Get valid descriptor set
		const DescriptorSet& descriptor_set = mOutputMaterialInstance.update();

		// Gather draw info
		MeshInstance& mesh_instance = mRenderableOutputMesh.getMesh().getMeshInstance();
		GPUMesh& mesh = mesh_instance.getGPUMesh();

		// Get pipeline to to render with
		utility::ErrorState error_state;
		RenderService::Pipeline pipeline = mRenderService->getOrCreatePipeline(mTarget, mRenderableOutputMesh.getMesh(), mCanvas->mVideoMaterialInstance, error_state);
		vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.mPipeline);
		vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.mLayout, 0, 1, &descriptor_set.mSet, 0, nullptr);

		// Bind buffers and draw
		const std::vector<VkBuffer>& vertexBuffers = mRenderableOutputMesh.getVertexBuffers();
		const std::vector<VkDeviceSize>& vertexBufferOffsets = mRenderableOutputMesh.getVertexBufferOffsets();

		vkCmdBindVertexBuffers(command_buffer, 0, vertexBuffers.size(), vertexBuffers.data(), vertexBufferOffsets.data());
		for (int index = 0; index < mesh_instance.getNumShapes(); ++index)
		{
			const IndexBuffer& index_buffer = mesh.getIndexBuffer(index);
			vkCmdBindIndexBuffer(command_buffer, index_buffer.getBuffer(), 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(command_buffer, index_buffer.getCount(), 1, 0, 0, 0);
		}

		mTarget.endRendering();
	}


	void RenderCanvasComponentInstance::onDraw(IRenderTarget& renderTarget, VkCommandBuffer commandBuffer, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix)
	{
		
		// compute the model matrix with aspect ratio calculated with mOutputTexture and size and position with mTransformComponent
		computeModelMatrix(renderTarget, mModelMatrix, mOutputTexture, mTransformComponent);
		mModelMatrixUniform->setValue(mModelMatrix);

		// Update matrices, projection and model are required
		mProjectMatrixUniform->setValue(projectionMatrix);
		mViewMatrixUniform->setValue(viewMatrix);

		// Get valid descriptor set
		const DescriptorSet& descriptor_set = mOutputMaterialInstance.update();

		// Gather draw info
		MeshInstance& mesh_instance = mRenderableOutputMesh.getMesh().getMeshInstance();
		GPUMesh& mesh = mesh_instance.getGPUMesh();

		// Get pipeline to to render with
		utility::ErrorState error_state;
		RenderService::Pipeline pipeline = mRenderService->getOrCreatePipeline(renderTarget, mRenderableOutputMesh.getMesh(), mCanvas->mVideoMaterialInstance, error_state);
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
		glm::ivec2 tex_size = mTarget.getBufferSize();
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
			translate.x + tex_size.x / 2.0f,
			translate.y + tex_size.y / 2.0f,
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

	}

	

	nap::UniformMat4Instance* RenderCanvasComponentInstance::ensureUniform(const std::string& uniformName, utility::ErrorState& error)
	{
		assert(mMVPStruct != nullptr);
		UniformMat4Instance* found_uniform = mMVPStruct->getOrCreateUniform<UniformMat4Instance>(uniformName);
		if (!error.check(found_uniform != nullptr,
			"%s: unable to find uniform: %s in material: %s", this->mID.c_str(), uniformName.c_str(),
			mOutputMaterialInstance.getMaterial().mID.c_str()))
			return nullptr;
		return found_uniform;
	}

	nap::Sampler2DInstance* RenderCanvasComponentInstance::ensureSampler(const std::string& samplerName, utility::ErrorState& error)
	{
		Sampler2DInstance* found_sampler = mOutputMaterialInstance.getOrCreateSampler<Sampler2DInstance>(samplerName);
		if (!error.check(found_sampler != nullptr,
			"%s: unable to find sampler: %s in material: %s", this->mID.c_str(), samplerName.c_str(),
			mOutputMaterialInstance.getMaterial().mID.c_str()))
			return nullptr;
		return found_sampler;
	}

	void RenderCanvasComponentInstance::videoChanged(VideoPlayer& player)
	{
		mYSampler->setTexture(player.getYTexture());
		mUSampler->setTexture(player.getUTexture());
		mVSampler->setTexture(player.getVTexture());
	}
}