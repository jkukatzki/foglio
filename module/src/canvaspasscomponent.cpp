#include "canvaspasscomponent.h"

#include <utility/fileutils.h>
#include <imagefromfile.h>
#include <videoshader.h>
#include <entity.h>
#include <orthocameracomponent.h>
#include <nap/core.h>
#include <renderservice.h>
#include <renderglobals.h>
#include <glm/gtc/matrix_transform.hpp>
#include <material.h>
#include <nap/resourceptr.h>
#include <nap/group.h>
#include <rtti/objectptr.h>


RTTI_BEGIN_STRUCT(nap::ShaderDeclarationOverride)
	RTTI_PROPERTY("Shader Declaration", &nap::ShaderDeclarationOverride::declarationName, nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("Override Value", &nap::ShaderDeclarationOverride::overrideValue, nap::rtti::EPropertyMetaData::Default)
RTTI_END_STRUCT

// nap::rendercanvascomponent run time class definition
RTTI_BEGIN_CLASS(nap::CanvasPassComponent)
	RTTI_PROPERTY("PassShader", &nap::CanvasPassComponent::mPassShader, nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("Unique Shader Declaration Overrides", &nap::CanvasPassComponent::shaderDeclarationOverrides, nap::rtti::EPropertyMetaData::Default)
RTTI_END_CLASS

RTTI_BEGIN_CLASS_NO_DEFAULT_CONSTRUCTOR(nap::CanvasPassComponentInstance)
RTTI_CONSTRUCTOR(nap::EntityInstance&, nap::Component&)
RTTI_END_CLASS



namespace nap
{

	CanvasPassComponentInstance::CanvasPassComponentInstance(EntityInstance& entity, Component& resource) :
		RenderableComponentInstance(entity, resource),
		mHeadlessPlaneMesh(new PlaneMesh(*entity.getCore())),
		mFinalRenderTarget(new RenderTarget(*entity.getCore())),
		mFinalTexture(new RenderTexture2D(*entity.getCore()))
	{ }

	bool CanvasPassComponentInstance::init(utility::ErrorState& errorState)
	{
		if (!RenderableComponentInstance::init(errorState))
			return false;
	
		// Extract render service
		mRenderService = getEntityInstance()->getCore()->getService<RenderService>();
		assert(mRenderService != nullptr);
		CanvasPassComponent* resource = getComponent<CanvasPassComponent>();
		mResourceManager = getEntityInstance()->getCore()->getResourceManager();
		// Get shader - make this a file link directly to .frag file?
		if (!errorState.check(resource->mPassShader != nullptr, "%s: no pass shader", this->mID.c_str())){
				return false;
		}
		else {
			mPassShader = resource->mPassShader;
		}
		// Construct material
		if (!errorState.check(constructMaterial(errorState), "%s: construct material call returned false", this->mID.c_str())) {
			return false;
		}
		// In texture (previous pass out texture)
		mSamplers["inTextureSampler"] = ensureSampler("inTexture", errorState);
		// create plane and initialize it
		if (!setupPlaneMesh(mHeadlessPlaneMesh, errorState)) {
			return false;
		}
		mRenderableMesh = mRenderService->createRenderableMesh(*mHeadlessPlaneMesh, *mMaterialInstance, errorState);
		if (!errorState.check(mRenderableMesh.isValid(), "%s: unable to construct renderable mesh for pass", mID.c_str()))
			return false;
		// Create uniforms
		if (!errorState.check(createUniforms(errorState), "%s: create uniforms call returned false", this->mID.c_str())) {
			return false;
		} 
		
		return true;

	}

	bool CanvasPassComponentInstance::initPassTargetAndTexture(ResourcePtr<RenderTarget> canvasRenderTarget, ResourcePtr<RenderTexture2D> canvasTexture, utility::ErrorState& errorState) {
		bool status = constructTextureAndRenderTarget(canvasRenderTarget, canvasTexture, true, errorState);
		// "hardcode" projection matrix at this stage because we do not change it
		// Create orthographic projection matrix
		glm::ivec2 size = mFinalRenderTarget->getBufferSize();
		glm::mat4 proj_matrix = OrthoCameraComponentInstance::createRenderProjectionMatrix(0.0f, (float)size.x, 0.0f, (float)size.y);
		mProjectMatrixUniform->setValue(proj_matrix);
		// Update the model matrix so that the plane mesh is of the same size as the render target
		computeModelMatrixFullscreen(mModelMatrix, (float)size.x, (float)size.y);
		mModelMatrixUniform->setValue(mModelMatrix);
		return status;
	}

	bool CanvasPassComponentInstance::constructTextureAndRenderTarget(ResourcePtr<RenderTarget>& referenceTarget, ResourcePtr<RenderTexture2D>& referenceTexture, bool transparent, utility::ErrorState& errorState) {
		mFinalTexture->mWidth = referenceTexture->mWidth;
		mFinalTexture->mHeight = referenceTexture->mHeight;
		mFinalTexture->mFormat = RenderTexture2D::EFormat::RGBA8;
		if (!mFinalTexture->init(errorState))
			return false;
		if (transparent) {
			mFinalRenderTarget->mClearColor = RGBAColor8(255, 255, 255, 0).convert<RGBAColorFloat>();
		}
		else {
			mFinalRenderTarget->mClearColor = RGBColor8(255, 0, 0).convert<RGBColorFloat>();
		}
		mFinalRenderTarget->mColorTexture = mFinalTexture;
		mFinalRenderTarget->mSampleShading = true;
		mFinalRenderTarget->mRequestedSamples = ERasterizationSamples::One;
		if (!mFinalRenderTarget->init(errorState))
			return false;
	}

	bool CanvasPassComponentInstance::createUniforms(utility::ErrorState& errorState) {
		mMVPStruct = mMaterialInstance->getOrCreateUniform(uniform::mvpStruct);
		if (!errorState.check(mMVPStruct != nullptr, "%s: Unable to find uniform MVP struct: %s in material: %s",
			this->mID.c_str(), uniform::mvpStruct, mMaterialInstResource->mMaterial->mID.c_str()))
			return false;
		// Get all matrices
		mModelMatrixUniform = mMVPStruct->getOrCreateUniform<UniformMat4Instance>(uniform::modelMatrix);//ensureUniformMat4(uniform::modelMatrix, mMVPStruct, errorState);
		mProjectMatrixUniform = mMVPStruct->getOrCreateUniform<UniformMat4Instance>(uniform::projectionMatrix);//ensureUniformMat4(uniform::projectionMatrix, mMVPStruct, errorState);
		mViewMatrixUniform = mMVPStruct->getOrCreateUniform<UniformMat4Instance>(uniform::viewMatrix);//ensureUniformMat4(uniform::viewMatrix, mMVPStruct, errorState);
		bool mvpFulfilled = !(mModelMatrixUniform == nullptr || mProjectMatrixUniform == nullptr || mViewMatrixUniform == nullptr);
		if (!errorState.check(mvpFulfilled, "%s: unable to construct mvp uniforms for custom pass", getEntityInstance()->mID.c_str()))
			return false;
		// "hardcode" view matrix because it always looks down the z axis // do this in shader? and have no viewmatrix as uniform at all
		mViewMatrixUniform->setValue(glm::mat4());
		if (mPassShader->getSSBODeclarations().size() > 0) {
			for (auto& ssboDecl : mPassShader->getSSBODeclarations()) {
				nap::Logger::info("%s: ssboDecl", ssboDecl.mName);
			}
		}
		mUBO = mMaterialInstance->getOrCreateUniform("UBO");
		if (!errorState.check(mUBO != nullptr, "%s: Unable to create or find UBO struct", this->mID.c_str()))
			return false;
		
	}

	void CanvasPassComponentInstance::setInTextureSampler(Texture2D& inTexture) {
		mSamplers["inTextureSampler"]->setTexture(inTexture);
	}

	bool CanvasPassComponentInstance::constructMaterial(utility::ErrorState& errorState) {
		mMaterialInstResource = std::make_unique<MaterialInstanceResource>(MaterialInstanceResource());
		mMaterialInstResource->mBlendMode = EBlendMode::Opaque;
		mMaterialInstResource->mDepthMode = EDepthMode::NoReadWrite;
		mMaterialInstResource->mMaterial = ResourcePtr<Material>(new Material(*getEntityInstance()->getCore())); //mRenderService->getOrCreateMaterial<ShaderFromFile>(errorState);
		mMaterialInstResource->mMaterial->mShader = mPassShader.get();
		if (!errorState.check(mMaterialInstResource->mMaterial->init(errorState), "%s: unable to init material", this->mID.c_str()))
			return false;
		mMaterialInstance = new MaterialInstance();
		if (!errorState.check(mMaterialInstance->init(*mRenderService, *mMaterialInstResource, errorState), "%s: unable to instance material", this->mID.c_str()))
			return false;
	}
	void CanvasPassComponentInstance::draw()
	{
		const DescriptorSet* descriptor_set = &mMaterialInstance->update();
		// Get current command buffer, should be headless.
		VkCommandBuffer command_buffer = mRenderService->getCurrentCommandBuffer();
		//begin headless rendering
		mFinalRenderTarget->beginRendering();
		// Gather draw info
		MeshInstance& mesh_instance = mRenderableMesh.getMesh().getMeshInstance();
		GPUMesh& mesh = mesh_instance.getGPUMesh();
		// Get pipeline to to render with
		utility::ErrorState error_state;
		RenderService::Pipeline pipeline = mRenderService->getOrCreatePipeline(*mFinalRenderTarget, mRenderableMesh.getMesh(), mRenderableMesh.getMaterialInstance(), error_state);
		vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.mPipeline);
		vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.mLayout, 0, 1, &descriptor_set->mSet, 0, nullptr);
		// Bind buffers and draw
		const std::vector<VkBuffer>& vertexBuffers = mRenderableMesh.getVertexBuffers();
		const std::vector<VkDeviceSize>& vertexBufferOffsets = mRenderableMesh.getVertexBufferOffsets();
		vkCmdBindVertexBuffers(command_buffer, 0, vertexBuffers.size(), vertexBuffers.data(), vertexBufferOffsets.data());
		for (int index = 0; index < mesh_instance.getNumShapes(); ++index)
		{
			const IndexBuffer& index_buffer = mesh.getIndexBuffer(index);
			vkCmdBindIndexBuffer(command_buffer, index_buffer.getBuffer(), 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(command_buffer, index_buffer.getCount(), 1, 0, 0, 0);
		}
		mFinalRenderTarget->endRendering();
	}

	void CanvasPassComponentInstance::onDraw(IRenderTarget& renderTarget, VkCommandBuffer commandBuffer, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix)
	{
		// Get valid descriptor set
		const DescriptorSet& descriptor_set = mMaterialInstance->update();

		// Gather draw info
		MeshInstance& mesh_instance = mRenderableMesh.getMesh().getMeshInstance();
		GPUMesh& mesh = mesh_instance.getGPUMesh();

		// Get pipeline to to render with
		utility::ErrorState error_state;
		RenderService::Pipeline pipeline = mRenderService->getOrCreatePipeline(renderTarget, mRenderableMesh.getMesh(), *mMaterialInstance, error_state);
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

	void CanvasPassComponentInstance::computeModelMatrixFullscreen(glm::mat4& outMatrix, float sizeX, float sizeY) {
		//aspect ratio should be right because we set mTarget textures height and width to video players?
		// Transform to middle of target
		outMatrix = glm::translate(glm::mat4(), glm::vec3(
			sizeX / 2.0f,
			sizeY / 2.0f,
			0.0f));

		// Scale to fit targets
		outMatrix = glm::scale(outMatrix, glm::vec3(sizeX, sizeY, 1.0f));
	}

	bool CanvasPassComponentInstance::setupPlaneMesh(ResourcePtr<PlaneMesh> planeMesh, nap::utility::ErrorState errorState) {
		planeMesh->mSize = glm::vec2(1, 1);
		planeMesh->mPosition = glm::vec3(0.0f, 0.0f, 0.0f);
		planeMesh->mCullMode = ECullMode::Back;
		planeMesh->mUsage = EMemoryUsage::Static;
		planeMesh->mColumns = 1;
		planeMesh->mRows = 1;
		if (!errorState.check(planeMesh->setup(errorState), "Unable to setup plane mesh for pass %s", mID.c_str()))
			return false;
		return errorState.check(planeMesh->getMeshInstance().init(errorState), "Unable to initialize plane mesh instance %s", mID.c_str());
	}

	nap::UniformMat4Instance* CanvasPassComponentInstance::ensureUniformMat4(const std::string& uniformName, UniformStructInstance* structInstance, utility::ErrorState& error)
	{
		// DOES NOT WORK FOR SOME REASON, maybe found_uniform gets destructed once it leaves this scope?
		UniformMat4Instance* found_uniform = structInstance->getOrCreateUniform<UniformMat4Instance>(uniformName);
		if (found_uniform != nullptr) {
			found_uniform->setValue(glm::mat4());
		}
		if (!error.check(found_uniform != nullptr, "%s: unable to find uniform: %s in material", getEntityInstance()->mID.c_str(), uniformName.c_str()));
		return nullptr;

		return found_uniform;
	}

	nap::UniformVec3Instance* CanvasPassComponentInstance::ensureUniformVec3(const std::string& uniformName, UniformStructInstance* structInstance, utility::ErrorState& error)
	{
		UniformVec3Instance* found_uniform = structInstance->getOrCreateUniform<UniformVec3Instance>(uniformName);
		if (!error.check(found_uniform != nullptr, "%s: unable to find uniform: %s in material", this->mID.c_str(), uniformName.c_str()))
			return nullptr;
		return found_uniform;
	}

	nap::UniformFloatInstance* CanvasPassComponentInstance::ensureUniformFloat(const std::string& uniformName, UniformStructInstance* structInstance, utility::ErrorState& error)
	{
		UniformFloatInstance* found_uniform = structInstance->getOrCreateUniform<UniformFloatInstance>(uniformName);
		if (!error.check(found_uniform != nullptr, "%s: unable to find uniform: %s in material", this->mID.c_str(), uniformName.c_str()))
			return nullptr;
		return found_uniform;
	}

	nap::Sampler2DInstance* CanvasPassComponentInstance::ensureSampler(const std::string& samplerName, utility::ErrorState& error)
	{
		Sampler2DInstance* found_sampler = mMaterialInstance->getOrCreateSampler<Sampler2DInstance>(samplerName);
		if (!error.check(found_sampler != nullptr,
			"%s: unable to find sampler: %s in material", getEntityInstance()->mID.c_str(), samplerName.c_str()))
			return nullptr;
		return found_sampler;
	}
}