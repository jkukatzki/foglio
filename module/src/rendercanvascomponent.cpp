#include "rendercanvascomponent.h"
#include "canvaswarpshader.h"
#include "canvasinterfaceshader.h"
#include "maskshader.h"

#include <videoshader.h>
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
RTTI_PROPERTY("VideoPlayer", &nap::RenderCanvasComponent::mVideoPlayer, nap::rtti::EPropertyMetaData::Default)
RTTI_PROPERTY("Aspect Ratio", &nap::RenderCanvasComponent::mAspectRatio, nap::rtti::EPropertyMetaData::Default)
RTTI_PROPERTY("Resolution", &nap::RenderCanvasComponent::mResolution, nap::rtti::EPropertyMetaData::Default)
RTTI_PROPERTY("CornerOffsets", &nap::RenderCanvasComponent::mCornerOffsets, nap::rtti::EPropertyMetaData::Default)
RTTI_PROPERTY("PostShader", &nap::RenderCanvasComponent::mPostShader, nap::rtti::EPropertyMetaData::Default)
RTTI_PROPERTY("Mask", &nap::RenderCanvasComponent::mMask, nap::rtti::EPropertyMetaData::Default)


RTTI_END_CLASS

RTTI_BEGIN_CLASS_NO_DEFAULT_CONSTRUCTOR(nap::RenderCanvasComponentInstance)
RTTI_CONSTRUCTOR(nap::EntityInstance&, nap::Component&)
RTTI_END_CLASS

namespace nap
{

	RenderCanvasComponentInstance::RenderCanvasComponentInstance(EntityInstance& entity, Component& resource) :
		RenderableComponentInstance(entity, resource),
		mHeadlessPlaneMesh(new PlaneMesh(*entity.getCore())),
		mFinalPlaneMesh(new PlaneMesh(*entity.getCore())),
		mFinalRenderTarget(new RenderTarget(*entity.getCore())),
		mFinalTexture(new RenderTexture2D(*entity.getCore()))
	{ }



	ResourcePtr<RenderTexture2D> RenderCanvasComponentInstance::getOutputTexture()
	{
		return mFinalTexture;
	}

	nap::VideoPlayer* RenderCanvasComponentInstance::getVideoPlayer()
	{
		return mVideoPlayer;
	}

	bool RenderCanvasComponentInstance::init(utility::ErrorState& errorState)
	{
		if (!RenderableComponentInstance::init(errorState))
			return false;
		// Get resource
		RenderCanvasComponent* resource = getComponent<RenderCanvasComponent>();
		mTransformComponent = getEntityInstance()->findComponent<TransformComponentInstance>();
		
		
		// create planes and initialize them
		// The plane is positioned on update based on current texture output size and transform component, if its headless it's always fullscreen
		if (!setupPlaneMesh(mHeadlessPlaneMesh, 1, 1, errorState)) {
			return false;
		}
		if (!setupPlaneMesh(mFinalPlaneMesh, 10, 10, errorState)) {
			return false;
		}
		mResolution = new int(resource->mResolution);
		mAspectRatio = new float(resource->mAspectRatio);
		mVideoPlayer = resource->mVideoPlayer.get();
		constructTextureAndRenderTarget(mFinalRenderTarget, mFinalTexture, true, errorState);
		// Setup double buffer target for internal render
		for (int target_idx = 0; target_idx < 2; target_idx++)
		{

			auto tex = getEntityInstance()->getCore()->getResourceManager()->createObject<RenderTexture2D>();
			auto target = getEntityInstance()->getCore()->getResourceManager()->createObject<RenderTarget>();
			if (!errorState.check(constructTextureAndRenderTarget(target, tex, true, errorState), "%s: unable to construct internal render target", resource->mID.c_str()))
				return false;

			mDoubleBufferTarget[target_idx] = target;
		}

		// Extract render service
		mRenderService = getEntityInstance()->getCore()->getService<RenderService>();
		assert(mRenderService != nullptr);

		// Get video player
		
		if (mVideoPlayer != nullptr) {
			mVideoPlayer->play();
			if (!constructCanvasPassItem(CanvasMaterialType::VIDEO, errorState))
				return false;
			mVideoPlayer->VideoChanged.connect(mVideoChangedSlot);
			videoChanged(*mVideoPlayer);
		}
		

		
		mMask = resource->mMask.get();
		if (mMask !=  nullptr) {
			if(!constructCanvasPassItem(CanvasMaterialType::MASK, errorState))
				return false;
			mStockCanvasPasses[CanvasMaterialType::MASK].mSamplers["maskSampler"]->setTexture(*mMask.get());
		}
		if (!constructCanvasPassItem(CanvasMaterialType::INTERFACE, errorState))
			return false;
		if (!constructCanvasPassItem(CanvasMaterialType::WARP, errorState))
			return false;
		
		mCornerOffsets = resource->mCornerOffsets;
		setWarpCornerUniforms();
		mStockCanvasPasses[CanvasMaterialType::INTERFACE].mUBO->getOrCreateUniform<UniformVec3Instance>(uniform::canvasinterface::mousePos)->setValue(glm::vec3());
		mStockCanvasPasses[CanvasMaterialType::INTERFACE].mUBO->getOrCreateUniform<UniformFloatInstance>(uniform::canvasinterface::frameThickness)->setValue(0.01);
		mStockCanvasPasses[CanvasMaterialType::INTERFACE].mSamplers["inTextureSampler"]->setTexture(*mFinalTexture);
		mStockCanvasPasses[CanvasMaterialType::WARP].mSamplers["inTextureSampler"]->setTexture(*mFinalTexture);
		
		
		if (resource->mPostShader.get() != nullptr) {
			if (!errorState.check(resource->mPostShader.get()->init(errorState), "%s: unable to init post shader resource", resource->mID.c_str()))
				return false;
			mCustomPostPass = std::make_unique<CanvasPass>(CanvasPass());
			mCustomPostPass->mMaterialInstResource = std::make_unique<MaterialInstanceResource>(MaterialInstanceResource());
			mCustomPostPass->mMaterialInstResource->mBlendMode = resource->mPostShader->mBlendMode;
			mCustomPostPass->mMaterialInstResource->mDepthMode = resource->mPostShader->mDepthMode;
			mCustomPostPass->mMaterialInstResource->mMaterial = resource->mPostShader;
			mCustomPostPass->mMaterialInstance = new MaterialInstance();
			if (!errorState.check(mCustomPostPass->mMaterialInstance->init(*mRenderService, *mCustomPostPass->mMaterialInstResource, errorState), "%s: unable to instance material", this->mID.c_str()))
				return false;
			
			//create mvp struct on material instance, regardless of type
			mCustomPostPass->mMVPStruct = mCustomPostPass->mMaterialInstance->getOrCreateUniform(uniform::mvpStruct);
			if (!errorState.check(mCustomPostPass->mMVPStruct != nullptr, "%s: Unable to find uniform MVP struct: %s in material: %s",
				this->mID.c_str(), uniform::mvpStruct, mCustomPostPass->mMaterialInstResource->mMaterial->mID.c_str()))
				return false;
			// Get all matrices
			
			mCustomPostPass->mModelMatrixUniform = mCustomPostPass->mMVPStruct->getOrCreateUniform<UniformMat4Instance>(uniform::modelMatrix);//ensureUniformMat4(uniform::modelMatrix, mCustomPostPass->mMVPStruct, errorState);
			mCustomPostPass->mProjectMatrixUniform = mCustomPostPass->mMVPStruct->getOrCreateUniform<UniformMat4Instance>(uniform::projectionMatrix);//ensureUniformMat4(uniform::projectionMatrix, mCustomPostPass->mMVPStruct, errorState);
			mCustomPostPass->mViewMatrixUniform = mCustomPostPass->mMVPStruct->getOrCreateUniform<UniformMat4Instance>(uniform::viewMatrix);//ensureUniformMat4(uniform::viewMatrix, mCustomPostPass->mMVPStruct, errorState);
			bool mvpFulfilled = !(mCustomPostPass->mModelMatrixUniform == nullptr || mCustomPostPass->mProjectMatrixUniform == nullptr || mCustomPostPass->mViewMatrixUniform == nullptr);
			if (!errorState.check(mvpFulfilled, "%s: unable to construct mvp uniforms for custom pass", getEntityInstance()->mID.c_str()))
				return false;

			mCustomPostPass->mUBO = mCustomPostPass->mUBO = mCustomPostPass->mMaterialInstance->getOrCreateUniform("UBO");
			if (!errorState.check(mCustomPostPass->mUBO != nullptr, "%s: Unable to find UBO struct: %s in material: %s",
				this->mID.c_str(), uniform::canvaswarp::uboStructWarp, mCustomPostPass->mMaterialInstResource->mMaterial->mID.c_str()))
				return false;
			ensureUniformFloat("iTime", mCustomPostPass->mUBO, errorState);
			ensureUniformFloat("power_to", mCustomPostPass->mUBO, errorState);
			mCustomPostPass->mUBO->getOrCreateUniform<UniformFloatInstance>("iTime")->setValue(float(getCurrentDateTime().getMilliSecond()));
			mCustomPostPass->mUBO->getOrCreateUniform<UniformFloatInstance>("power_to")->setValue(1.0);
			
			mCustomPostPass->mSamplers["inTextureSampler"] = ensureSampler("inTexture", mCustomPostPass->mMaterialInstance, errorState);
			mCustomPostPass->mRenderableMesh = mRenderService->createRenderableMesh(*mHeadlessPlaneMesh, *mCustomPostPass->mMaterialInstance, errorState);
			if (!errorState.check(mCustomPostPass->mRenderableMesh.isValid(), "%s: unable to construct renderable mesh for custom pass", getEntityInstance()->mID.c_str()))
				return false;
		}

		return true;

	}

	void RenderCanvasComponentInstance::drawAllHeadlessPasses() {
		if (mStockCanvasPasses.find(CanvasMaterialType::MASK) != mStockCanvasPasses.end() || mCustomPostPass != nullptr)
		{
			//mask pass or custom pass present, render video into internal render target so it can be used in the mask/custom materials sampler
			mCurrentInternalRT = mDoubleBufferTarget[0];
		}
		else 
		{
			//no mask pass
			mCurrentInternalRT = mFinalRenderTarget;
		}
		if (mStockCanvasPasses.find(CanvasMaterialType::VIDEO) != mStockCanvasPasses.end()) {
			drawHeadlessPass(mStockCanvasPasses[CanvasMaterialType::VIDEO]);
		}
		
		if (mCustomPostPass != nullptr) {
			mCustomPostPass->mSamplers["inTextureSampler"]->setTexture(*mCurrentInternalRT->mColorTexture);
			mCustomPostPass->mUBO->getOrCreateUniform<UniformFloatInstance>("iTime")->setValue(float(getEntityInstance()->getCore()->getElapsedTime()));
			mCurrentInternalRT = (mStockCanvasPasses.find(CanvasMaterialType::MASK) != mStockCanvasPasses.end()) ? mDoubleBufferTarget[1] : mFinalRenderTarget;
			drawHeadlessPass(*mCustomPostPass);
		}

		if (mStockCanvasPasses.find(CanvasMaterialType::MASK) != mStockCanvasPasses.end())
		{
			mStockCanvasPasses[CanvasMaterialType::MASK].mSamplers["inTextureSampler"]->setTexture(*mCurrentInternalRT->mColorTexture);
			mCurrentInternalRT = mFinalRenderTarget;
			drawHeadlessPass(mStockCanvasPasses[CanvasMaterialType::MASK]);
		}
	}

	void RenderCanvasComponentInstance::drawHeadlessPass(CanvasPass& pass)
	{
		// Create orthographic projection matrix
		glm::ivec2 size = mCurrentInternalRT->getBufferSize();
		glm::mat4 proj_matrix = OrthoCameraComponentInstance::createRenderProjectionMatrix(0.0f, (float)size.x, 0.0f, (float)size.y);
		// Update the model matrix so that the plane mesh is of the same size as the render target
		// maybe do this only on update and store in member when window is resized for example to prevent unnecessary calculations
		computeModelMatrixFullscreen(mModelMatrix);
		// Update matrices, projection and model are required
		pass.mModelMatrixUniform->setValue(mModelMatrix);
		pass.mProjectMatrixUniform->setValue(proj_matrix);
		pass.mViewMatrixUniform->setValue(glm::mat4());
		//get descriptor set
		const DescriptorSet* descriptor_set = &pass.mMaterialInstance->update();
		draw(pass.mRenderableMesh, descriptor_set);
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

	void RenderCanvasComponentInstance::drawInterface(rtti::ObjectPtr<RenderTarget> interfaceTarget)
	{
		mCurrentInternalRT = interfaceTarget;
		drawHeadlessPass(mStockCanvasPasses[CanvasMaterialType::INTERFACE]);
	}

	void RenderCanvasComponentInstance::setFinalSampler(bool isInterface) 
	{
		if (isInterface) {
			mStockCanvasPasses[CanvasMaterialType::WARP].mSamplers["inTextureSampler"]->setTexture(*mCurrentInternalRT->mColorTexture);
		}
		else {
			mStockCanvasPasses[CanvasMaterialType::WARP].mSamplers["inTextureSampler"]->setTexture(*mFinalTexture);
		}
	}

	


	void RenderCanvasComponentInstance::onDraw(IRenderTarget& renderTarget, VkCommandBuffer commandBuffer, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix)
	{
		// compute the model matrix with aspect ratio calculated with outputTexture and size and position with mTransformComponent
		if (mIsControlViewDraw)
		{
			ResourcePtr<RenderWindow> mainWindowRenderTarget = getEntityInstance()->getCore()->getResourceManager()->findObject<RenderWindow>("MainWindow");
			computeModelMatrix(*mainWindowRenderTarget, mModelMatrix, mFinalTexture, mTransformComponent);
			/***if (renderTarget.getBufferSize().x > renderTarget.getBufferSize().y) {
				glm::vec3 translate = mTransformComponent->getTranslate();
				mModelMatrix = glm::translate(mModelMatrix, glm::vec3(-mainWindowRenderTarget->getBufferSize().x * translate.x + , 0, 0));
			}***/
		}
		else {
			computeModelMatrix(renderTarget, mModelMatrix, mFinalTexture, mTransformComponent);
		}
			
		mStockCanvasPasses[CanvasMaterialType::WARP].mModelMatrixUniform->setValue(mModelMatrix);

		// Update matrices, projection and model are required
		mStockCanvasPasses[CanvasMaterialType::WARP].mProjectMatrixUniform->setValue(projectionMatrix);
		mStockCanvasPasses[CanvasMaterialType::WARP].mViewMatrixUniform->setValue(viewMatrix);

		// Get valid descriptor set
		const DescriptorSet& descriptor_set = mStockCanvasPasses[CanvasMaterialType::WARP].mMaterialInstance->update();

		// Gather draw info
		MeshInstance& mesh_instance = mStockCanvasPasses[CanvasMaterialType::WARP].mRenderableMesh.getMesh().getMeshInstance();
		GPUMesh& mesh = mesh_instance.getGPUMesh();

		// Get pipeline to to render with
		utility::ErrorState error_state;
		RenderService::Pipeline pipeline = mRenderService->getOrCreatePipeline(renderTarget, mStockCanvasPasses[CanvasMaterialType::WARP].mRenderableMesh.getMesh(), *mStockCanvasPasses[CanvasMaterialType::WARP].mMaterialInstance, error_state);
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.mPipeline);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.mLayout, 0, 1, &descriptor_set.mSet, 0, nullptr);

		// Bind buffers and draw
		const std::vector<VkBuffer>& vertexBuffers = mStockCanvasPasses[CanvasMaterialType::WARP].mRenderableMesh.getVertexBuffers();
		const std::vector<VkDeviceSize>& vertexBufferOffsets = mStockCanvasPasses[CanvasMaterialType::WARP].mRenderableMesh.getVertexBufferOffsets();

		vkCmdBindVertexBuffers(commandBuffer, 0, vertexBuffers.size(), vertexBuffers.data(), vertexBufferOffsets.data());
		for (int index = 0; index < mesh_instance.getNumShapes(); ++index)
		{
			const IndexBuffer& index_buffer = mesh.getIndexBuffer(index);
			vkCmdBindIndexBuffer(commandBuffer, index_buffer.getBuffer(), 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(commandBuffer, index_buffer.getCount(), 1, 0, 0, 0);
		}
	}

	bool RenderCanvasComponentInstance::constructCanvasPassItem(CanvasMaterialType type, utility::ErrorState error) {
		CanvasPass* pass = &mStockCanvasPasses[type];
		switch (type) {
		case CanvasMaterialType::VIDEO: {
			//create video material
			pass->mMaterialInstResource = std::make_unique<MaterialInstanceResource>(MaterialInstanceResource());
			pass->mMaterialInstResource->mBlendMode = EBlendMode::Opaque;
			pass->mMaterialInstResource->mDepthMode = EDepthMode::NoReadWrite;
			pass->mMaterial = mRenderService->getOrCreateMaterial<VideoShader>(error);
			break;
		}
		case CanvasMaterialType::WARP: {
			//create canvas warp material
			pass->mMaterialInstResource = std::make_unique<MaterialInstanceResource>(MaterialInstanceResource());
			pass->mMaterialInstResource->mBlendMode = EBlendMode::AlphaBlend;
			pass->mMaterialInstResource->mDepthMode = EDepthMode::NoReadWrite;
			pass->mMaterial = mRenderService->getOrCreateMaterial<CanvasWarpShader>(error);
			break;
		}
		case CanvasMaterialType::INTERFACE: {
			//create canvas mask material
			pass->mMaterialInstResource = std::make_unique<MaterialInstanceResource>(MaterialInstanceResource());
			pass->mMaterialInstResource->mBlendMode = EBlendMode::AlphaBlend;
			pass->mMaterialInstResource->mDepthMode = EDepthMode::NoReadWrite;
			pass->mMaterial = mRenderService->getOrCreateMaterial<CanvasInterfaceShader>(error);
			break;
		}
		case CanvasMaterialType::MASK: {
			//create canvas mask material
			pass->mMaterialInstResource = std::make_unique<MaterialInstanceResource>(MaterialInstanceResource());
			pass->mMaterialInstResource->mBlendMode = EBlendMode::AlphaBlend;
			pass->mMaterialInstResource->mDepthMode = EDepthMode::NoReadWrite;
			pass->mMaterial = mRenderService->getOrCreateMaterial<MaskShader>(error);
			break;
		}
		default:
		{
			nap::Logger::info("Unspecified shader in Canvas::constructMaterialInstance");
			break;
		}
		}

		if (pass == nullptr)
			return false;
		if (!error.check(pass->mMaterial != nullptr, "%s: unable to get or create material", mID.c_str()))
			return false;
		pass->mMaterialInstance = new MaterialInstance();
		pass->mMaterialInstResource->mMaterial = pass->mMaterial;
		if (!error.check(pass->mMaterialInstance->init(*mRenderService, *pass->mMaterialInstResource, error), "%s: unable to instance material", this->mID.c_str())) {
			return false;
		}
		//create mvp struct on material instance, regardless of type
		pass->mMVPStruct = pass->mMaterialInstance->getOrCreateUniform(uniform::mvpStruct);
		if (!error.check(pass->mMVPStruct != nullptr, "%s: Unable to find uniform MVP struct: %s in material: %s",
			this->mID.c_str(), uniform::mvpStruct, pass->mMaterial->mID.c_str()))
			return false;
		// Get all matrices
		pass->mModelMatrixUniform = pass->mMVPStruct->getOrCreateUniform<UniformMat4Instance>(uniform::modelMatrix);// ensureUniformMat4(uniform::modelMatrix, pass->mMVPStruct, error);
		pass->mProjectMatrixUniform = pass->mMVPStruct->getOrCreateUniform<UniformMat4Instance>(uniform::projectionMatrix);// ensureUniformMat4(uniform::projectionMatrix, pass->mMVPStruct, error);
		pass->mViewMatrixUniform = pass->mMVPStruct->getOrCreateUniform<UniformMat4Instance>(uniform::viewMatrix);// ensureUniformMat4(uniform::viewMatrix, pass->mMVPStruct, error);

		if (pass->mModelMatrixUniform == nullptr || pass->mProjectMatrixUniform == nullptr || pass->mViewMatrixUniform == nullptr) {
			nap::Logger::info("failed to construct mvp struct uniforms");
			return false;
		}

		//sampler and uniform definitions
		switch (type) {

		case CanvasMaterialType::VIDEO:
		{
			pass->mSamplers["YSampler"] = ensureSampler(uniform::video::sampler::YSampler, pass->mMaterialInstance, error);
			pass->mSamplers["USampler"] = ensureSampler(uniform::video::sampler::USampler, pass->mMaterialInstance, error);
			pass->mSamplers["VSampler"] = ensureSampler(uniform::video::sampler::VSampler, pass->mMaterialInstance, error);
			if (pass->mSamplers["YSampler"] == nullptr || pass->mSamplers["USampler"] == nullptr || pass->mSamplers["VSampler"] == nullptr)
				return false;
			break;
		}

		case CanvasMaterialType::WARP:
		{
			pass->mSamplers["inTextureSampler"] = ensureSampler(uniform::canvaswarp::sampler::inTexture, pass->mMaterialInstance, error);
			if (pass->mSamplers["inTextureSampler"] == nullptr)
				return false;

			pass->mUBO = pass->mMaterialInstance->getOrCreateUniform(uniform::canvaswarp::uboStructWarp);
			if (!error.check(pass->mUBO != nullptr, "%s: Unable to find UBO struct: %s in material: %s",
				this->mID.c_str(), uniform::canvaswarp::uboStructWarp, pass->mMaterial->mID.c_str()))
				return false;
			// create all offset uniforms
			ensureUniformVec3(uniform::canvaswarp::topLeft, pass->mUBO, error);
			ensureUniformVec3(uniform::canvaswarp::topRight, pass->mUBO, error);
			ensureUniformVec3(uniform::canvaswarp::bottomLeft, pass->mUBO, error);
			ensureUniformVec3(uniform::canvaswarp::bottomRight, pass->mUBO, error);
			break;
		}

		case CanvasMaterialType::INTERFACE:
		{
			pass->mSamplers["inTextureSampler"] = ensureSampler(uniform::canvasinterface::sampler::inTexture, pass->mMaterialInstance, error);
			if (pass->mSamplers["inTextureSampler"] == nullptr)
				return false;
			pass->mUBO = pass->mMaterialInstance->getOrCreateUniform(uniform::canvasinterface::uboStructInterface);
			if (!error.check(pass->mUBO != nullptr, "%s: Unable to find UBO struct: %s in material", getEntityInstance()->mID.c_str(), uniform::canvaswarp::uboStructWarp))
				return false;
			// create uniforms
			ensureUniformFloat(uniform::canvasinterface::frameThickness, pass->mUBO, error);
			ensureUniformVec3(uniform::canvasinterface::mousePos, pass->mUBO, error);
			break;
		}

		case CanvasMaterialType::MASK:
		{
			nap::Logger::info("Constructing mask pass");
			pass->mSamplers["inTextureSampler"] = ensureSampler(uniform::mask::sampler::inTexture, pass->mMaterialInstance, error);
			pass->mSamplers["maskSampler"] = ensureSampler(uniform::mask::sampler::maskTexture, pass->mMaterialInstance, error);
			if (pass->mSamplers["inTextureSampler"] == nullptr || pass->mSamplers["maskSampler"] == nullptr)
				return false;
			break;
		}
		default:
		{
			nap::Logger::info("Unspecified shader in Canvas::constructMaterialInstance");
			break;
		}
		}
		if (type == CanvasMaterialType::WARP) {
			pass->mRenderableMesh = mRenderService->createRenderableMesh(*mFinalPlaneMesh, *pass->mMaterialInstance, error);
		}
		pass->mRenderableMesh = mRenderService->createRenderableMesh(*mHeadlessPlaneMesh, *pass->mMaterialInstance, error);
		if (pass->mRenderableMesh.isValid())
			return true;
	}

	nap::UniformMat4Instance* RenderCanvasComponentInstance::ensureUniformMat4(const std::string& uniformName, UniformStructInstance* structInstance, utility::ErrorState& error)
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

	nap::UniformVec3Instance* RenderCanvasComponentInstance::ensureUniformVec3(const std::string& uniformName, UniformStructInstance* structInstance, utility::ErrorState& error)
	{
		UniformVec3Instance* found_uniform = structInstance->getOrCreateUniform<UniformVec3Instance>(uniformName);
		if (!error.check(found_uniform != nullptr, "%s: unable to find uniform: %s in material", this->mID.c_str(), uniformName.c_str()))
			return nullptr;
		return found_uniform;
	}

	nap::UniformFloatInstance* RenderCanvasComponentInstance::ensureUniformFloat(const std::string& uniformName, UniformStructInstance* structInstance, utility::ErrorState& error)
	{
		UniformFloatInstance* found_uniform = structInstance->getOrCreateUniform<UniformFloatInstance>(uniformName);
		if (!error.check(found_uniform != nullptr, "%s: unable to find uniform: %s in material", this->mID.c_str(), uniformName.c_str()))
			return nullptr;
		return found_uniform;
	}

	nap::Sampler2DInstance* RenderCanvasComponentInstance::ensureSampler(const std::string& samplerName, MaterialInstance* materialInstance, utility::ErrorState& error)
	{
		Sampler2DInstance* found_sampler = materialInstance->getOrCreateSampler<Sampler2DInstance>(samplerName);
		if (!error.check(found_sampler != nullptr,
			"%s: unable to find sampler: %s in material", getEntityInstance()->mID.c_str(), samplerName.c_str()))
			return nullptr;
		return found_sampler;
	}

	void RenderCanvasComponentInstance::videoChanged(VideoPlayer& player)
	{
		nap::Logger::info("Video Changed for Canvas: %s", getEntityInstance()->mID.c_str());
		mStockCanvasPasses[CanvasMaterialType::VIDEO].mSamplers["YSampler"]->setTexture(player.getYTexture());
		mStockCanvasPasses[CanvasMaterialType::VIDEO].mSamplers["USampler"]->setTexture(player.getUTexture());
		mStockCanvasPasses[CanvasMaterialType::VIDEO].mSamplers["VSampler"]->setTexture(player.getVTexture());
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

	bool RenderCanvasComponentInstance::constructTextureAndRenderTarget(ResourcePtr<RenderTarget>& renderTarget, ResourcePtr<RenderTexture2D>& texture, bool transparent, utility::ErrorState& errorState) {
		//init mOutputTexture TODO: resize when videoChanged event?
		int width;
		int height;
		if (*mResolution > 19) { // > 19 to establish a minimum size
			if (*mAspectRatio > 0.05) {
				width = (*mResolution) * (*mAspectRatio);
				height = *mResolution;
			}
			else {
				if (mVideoPlayer != nullptr) {
					if (mVideoPlayer->getWidth() >= mVideoPlayer->getHeight()) {
						width = *mResolution * mVideoPlayer->getWidth()/mVideoPlayer->getHeight();
						height = *mResolution;
					}
					else {
						width = *mResolution;
						height = *mResolution * mVideoPlayer->getHeight()/mVideoPlayer->getWidth();
					}
				}
				else {
					width = *mResolution;
					height = *mResolution;
				}
			}
		}
		else {
			if (mVideoPlayer != nullptr)
			{
				if (*mAspectRatio > 0.05) {
					int max = std::max(mVideoPlayer->getWidth(), mVideoPlayer->getHeight());
					if (max < 20) {
						max = 20;
					}
					width = max * (*mAspectRatio);
					height = max / (*mAspectRatio);
				}
				else {
					width = mVideoPlayer->getWidth();
					height = mVideoPlayer->getHeight();
				}
				
			}
		}
		
		
		texture->mWidth = width;
		texture->mHeight = height;
		texture->mFormat = RenderTexture2D::EFormat::RGBA8;
		if (!texture->init(errorState))
			return false;
		if (transparent) {
			renderTarget->mClearColor = RGBAColor8(255, 255, 255, 0).convert<RGBAColorFloat>();
		}
		else {
			renderTarget->mClearColor = RGBColor8(255, 0, 0).convert<RGBColorFloat>();
		}
		renderTarget->mColorTexture = texture;
		renderTarget->mSampleShading = true;
		renderTarget->mRequestedSamples = ERasterizationSamples::One;
		if (!renderTarget->init(errorState))
			return false;
	}

	bool RenderCanvasComponentInstance::setupPlaneMesh(ResourcePtr<PlaneMesh> planeMesh, int resX, int resY, nap::utility::ErrorState errorState) {
		planeMesh->mSize = glm::vec2(1.0f, 1.0f);
		planeMesh->mPosition = glm::vec3(0.0f, 0.0f, 0.0f);
		planeMesh->mCullMode = ECullMode::Back;
		planeMesh->mUsage = EMemoryUsage::DynamicWrite;
		planeMesh->mColumns = 10;
		planeMesh->mRows = 10;
		if (!errorState.check(planeMesh->setup(errorState), "Unable to setup canvas plane %s", mID.c_str()))
			return false;
		return errorState.check(planeMesh->getMeshInstance().init(errorState), "Unable to initialize plane mesh instance %s", mID.c_str());
	}

	void RenderCanvasComponentInstance::setCornerOffsets(std::vector<glm::vec2> offsets) {
		mCornerOffsets[0] = offsets[0];
		mCornerOffsets[1] = offsets[1];
		mCornerOffsets[2] = offsets[2];
		mCornerOffsets[3] = offsets[3];
		setWarpCornerUniforms();
	}

	void RenderCanvasComponentInstance::setWarpCornerUniforms() {
		mStockCanvasPasses[CanvasMaterialType::WARP].mUBO->getOrCreateUniform<UniformVec3Instance>(uniform::canvaswarp::topLeft)->setValue(glm::vec3(mCornerOffsets[0].x, mCornerOffsets[0].y * (-1), 0));
		mStockCanvasPasses[CanvasMaterialType::WARP].mUBO->getOrCreateUniform<UniformVec3Instance>(uniform::canvaswarp::topRight)->setValue(glm::vec3(mCornerOffsets[1].x * (-1), mCornerOffsets[1].y * (-1), 0));
		mStockCanvasPasses[CanvasMaterialType::WARP].mUBO->getOrCreateUniform<UniformVec3Instance>(uniform::canvaswarp::bottomLeft)->setValue(glm::vec3(mCornerOffsets[2].x, mCornerOffsets[2].y, 0));
		mStockCanvasPasses[CanvasMaterialType::WARP].mUBO->getOrCreateUniform<UniformVec3Instance>(uniform::canvaswarp::bottomRight)->setValue(glm::vec3(mCornerOffsets[3].x * (-1), mCornerOffsets[3].y, 0));
	}

}