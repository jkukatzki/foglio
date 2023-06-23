#include "rendercanvascomponent.h"

#include <entity.h>
#include <orthocameracomponent.h>
#include <nap/core.h>
#include <renderservice.h>
#include <renderglobals.h>
#include <foglioservice.h>
#include <glm/gtc/matrix_transform.hpp>

// nap::rendercanvascomponent run time class definition
RTTI_BEGIN_CLASS(nap::RenderCanvasComponent)
	RTTI_PROPERTY("VideoPlayer", &nap::RenderCanvasComponent::mVideoPlayer, nap::rtti::EPropertyMetaData::Required)
RTTI_END_CLASS

RTTI_BEGIN_CLASS_NO_DEFAULT_CONSTRUCTOR(nap::RenderCanvasComponentInstance)
	RTTI_CONSTRUCTOR(nap::EntityInstance&, nap::Component&)
RTTI_END_CLASS

namespace nap
{

	static void computeModelMatrix(const nap::IRenderTarget& target, glm::mat4& outMatrix)
	{
		// Transform to middle of target
		glm::ivec2 tex_size = target.getBufferSize();
		outMatrix = glm::translate(glm::mat4(), glm::vec3(
			tex_size.x / 2.0f,
			tex_size.y / 2.0f,
			0.0f));

		// Scale to fit target
		outMatrix = glm::scale(outMatrix, glm::vec3(tex_size.x, tex_size.y, 1.0f));
	}

	RenderCanvasComponentInstance::RenderCanvasComponentInstance(EntityInstance& entity, Component& resource) :
		RenderableComponentInstance(entity, resource),
		mTarget(*entity.getCore()),
		mPlane(*entity.getCore()),
		mOutputTexture(new RenderTexture2D(*entity.getCore())),
		mCanvasMaterial(new Material(*entity.getCore()))
		{ }

	bool RenderCanvasComponentInstance::init(utility::ErrorState& errorState)
	{
		if (!RenderableComponentInstance::init(errorState))
			return false;

		// Get resource
		RenderCanvasComponent* resource = getComponent<RenderCanvasComponent>();

		// Extract player
		mPlayer = resource->mVideoPlayer.get();
		if (!errorState.check(mPlayer != nullptr, "%s: no video player", resource->mID.c_str()))
			return false;

		// Setup output texture
		mOutputTexture->mFormat = RenderTexture2D::EFormat::RGBA8;
		if (!mOutputTexture->init(errorState))
			return false;

		// Setup render target and initialize
		mTarget.mClearColor = RGBAColor8( 255, 255, 255, 255).convert<RGBAColorFloat>();
		mTarget.mColorTexture = mOutputTexture;
		mTarget.mSampleShading = true;
		mTarget.mRequestedSamples = ERasterizationSamples::One;
		if (!mTarget.init(errorState))
			return false;

		// Now create a plane and initialize it
		// The plane is positioned on update based on current texture output size
		mPlane.mSize = glm::vec2(1.0f, 1.0f);
		mPlane.mPosition = glm::vec3(0.0f, 0.0f, 0.0f);
		mPlane.mCullMode = ECullMode::Back;
		mPlane.mUsage = EMemoryUsage::Static;
		mPlane.mColumns = 1;
		mPlane.mRows = 1;

		if (!mPlane.init(errorState))
			return false;

		// Extract render service
		mRenderService = getEntityInstance()->getCore()->getService<RenderService>();
		assert(mRenderService != nullptr);

		// Extract foglio service for asset 
		mFoglioService = getEntityInstance()->getCore()->getService<FoglioService>();
		std::string relative_path = utility::joinPath({ "shaders", utility::appendFileExtension("canvas", "vert") });
		const std::string frag_shader_path = mFoglioService->getModule().findAsset(relative_path);

		// create canvas material
		

	}

	bool RenderCanvasComponentInstance::isSupported(nap::CameraComponentInstance& camera) const
	{
		return camera.get_type().is_derived_from(RTTI_OF(OrthoCameraComponentInstance));
	}

	void RenderCanvasComponentInstance::onDraw(IRenderTarget& renderTarget, VkCommandBuffer commandBuffer, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix)
	{
		// Update the model matrix so that the plane mesh is of the same size as the render target
		computeModelMatrix(renderTarget, mModelMatrix);
		mModelMatrixUniform->setValue(mModelMatrix);

		// Update matrices, projection and model are required
		mProjectMatrixUniform->setValue(projectionMatrix);
		mViewMatrixUniform->setValue(viewMatrix);

		// Get valid descriptor set
		const DescriptorSet& descriptor_set = mMaterialInstance.update();

		// Gather draw info
		MeshInstance& mesh_instance = mRenderableMesh.getMesh().getMeshInstance();
		GPUMesh& mesh = mesh_instance.getGPUMesh();

		// Get pipeline to to render with
		utility::ErrorState error_state;
		RenderService::Pipeline pipeline = mRenderService->getOrCreatePipeline(renderTarget, mRenderableMesh.getMesh(), mMaterialInstance, error_state);
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.mPipeline);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.mLayout, 0, 1, &descriptor_set.mSet, 0, nullptr);

		// Bind buffers and draw
		const std::vector<VkBuffer>& vertexBuffers = mRenderableMesh.getVertexBuffers();
		const std::vector<VkDeviceSize>& vertexBufferOffsets = mRenderableMesh.getVertexBufferOffsets();

		vkCmdBindVertexBuffers(commandBuffer, 0, vertexBuffers.size(), vertexBuffers.data(), vertexBufferOffsets.data());
		for (int index = 0; index < mesh_instance.getNumShapes(); ++index)
		{
			const IndexBuffer& index_buffer = mesh.getIndexBuffer(index);
			vkCmdBindIndexBuffer(commandBuffer, index_buffer.getBuffer(), 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(commandBuffer, index_buffer.getCount(), 1, 0, 0, 0);
		}
	}

	void RenderCanvasComponentInstance::videoChanged(VideoPlayer& player)
	{
		mYSampler->setTexture(player.getYTexture());
		mUSampler->setTexture(player.getUTexture());
		mVSampler->setTexture(player.getVTexture());
	}
}