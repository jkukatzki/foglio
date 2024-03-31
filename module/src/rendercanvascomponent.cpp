#include "rendercanvascomponent.h"
#include "canvaswarpshader.h"
#include "canvasinterfaceshader.h"
#include "maskshader.h"
#include "canvasgroupcomponent.h"
#include "canvaspasscomponent.h"

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
RTTI_PROPERTY("Aspect Ratio", &nap::RenderCanvasComponent::mAspectRatio, nap::rtti::EPropertyMetaData::Default)
RTTI_PROPERTY("Resolution", &nap::RenderCanvasComponent::mResolution, nap::rtti::EPropertyMetaData::Default)
RTTI_PROPERTY("CornerOffsets", &nap::RenderCanvasComponent::mCornerOffsets, nap::rtti::EPropertyMetaData::Default)
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

	void RenderCanvasComponent::getDependentComponents(std::vector<rtti::TypeInfo>& components) const
	{
		components.emplace_back(RTTI_OF(CanvasPassComponent));
	}

	ResourcePtr<RenderTexture2D> RenderCanvasComponentInstance::getFinalOutputTexture()
	{
		return mFinalTexture;
	}

	ResourcePtr<RenderTarget> RenderCanvasComponentInstance::getRenderTarget() {
		return mFinalRenderTarget;
	}


	bool RenderCanvasComponentInstance::init(utility::ErrorState& errorState)
	{
		if (!RenderableComponentInstance::init(errorState))
			return false;

		// Extract render service
		mRenderService = getEntityInstance()->getCore()->getService<RenderService>();
		assert(mRenderService != nullptr);

		// Get resource
		RenderCanvasComponent* resource = getComponent<RenderCanvasComponent>();

		// Get transform component
		mTransformComponent = getEntityInstance()->findComponent<TransformComponentInstance>();

		// Get window to render to
		CanvasGroupComponent* groupResource = getEntityInstance()->getParent()->findComponent<CanvasGroupComponentInstance>()->getComponent<CanvasGroupComponent>();
		if (groupResource != nullptr) {
			if (groupResource->mPresentationWindow != nullptr) {
				mPresentationWindow = groupResource->mPresentationWindow;
			}
		}
		else {
			nap::Logger::error("CanvasComponents entitys parent entity does not contain a CanvasGroupComponent");
			return false;
		}
		
		// Get all passes


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

		constructTextureAndRenderTarget(mFinalRenderTarget, mFinalTexture, true, errorState);

		if (!constructCanvasPassItem(CanvasMaterialType::INTERFACE, errorState))
			return false;
		if (!constructCanvasPassItem(CanvasMaterialType::WARP, errorState))
			return false;

		//PASSES
		// get pass components
		getEntityInstance()->getComponentsOfType(mCanvasPassComponents);
		// passes need some initialized members depending on the canvas size
		setupCanvasPassComponents(errorState);
		// set texture of shader that draws canvas to the wall to final pass out texture, (warp shader handles corner offsets in .vert)
		if (mCanvasPassComponents.size() > 0) {
			mStockCanvasPasses[CanvasMaterialType::WARP].mSamplers["inTextureSampler"]->setTexture(*mCanvasPassComponents.back()->getOutputTexture());
		}
		// Setup double buffer target for internal render
		for (int target_idx = 0; target_idx < 2; target_idx++)
		{
			auto tex = getEntityInstance()->getCore()->getResourceManager()->createObject<RenderTexture2D>();
			auto target = getEntityInstance()->getCore()->getResourceManager()->createObject<RenderTarget>();
			if (!errorState.check(constructTextureAndRenderTarget(target, tex, true, errorState), "%s: unable to construct internal render target", resource->mID.c_str()))
				return false;

			mDoubleBufferTarget[target_idx] = target;
		}

		
		
		mCornerOffsets = resource->mCornerOffsets;
		setWarpCornerUniforms();
		mStockCanvasPasses[CanvasMaterialType::INTERFACE].mUBO->getOrCreateUniform<UniformVec3Instance>(uniform::canvasinterface::mousePos)->setValue(glm::vec3());
		mStockCanvasPasses[CanvasMaterialType::INTERFACE].mUBO->getOrCreateUniform<UniformFloatInstance>(uniform::canvasinterface::frameThickness)->setValue(0.01);
		

		

		return true;

	}

	void RenderCanvasComponentInstance::setupCanvasPassComponents(utility::ErrorState& errorState) {
		if (mCanvasPassComponents.size() != 0) {
			getEntityInstance()->getComponentsOfType<CanvasPassComponentInstance>(mCanvasPassComponents);
			for (int i = 0; i < mCanvasPassComponents.size(); i++) {
				auto passComponent = mCanvasPassComponents[i];
				passComponent->initPassTargetAndTexture(mFinalRenderTarget, mFinalTexture, errorState);
				if (i > 0) {
					// set pass component in texture to that of previous one in queue // extend this when implementing muting of passes / transparencies ?
					passComponent->setInTextureSampler(mCanvasPassComponents[(i - 1)]->getOutputTexture());
				}
			}
			mFinalTexture = mCanvasPassComponents.back()->getOutputTexture();
			mStockCanvasPasses[CanvasMaterialType::INTERFACE].mSamplers["inTextureSampler"]->setTexture(*mFinalTexture);
			mStockCanvasPasses[CanvasMaterialType::WARP].mSamplers["inTextureSampler"]->setTexture(*mFinalTexture);
		}
		else {
			nap::Logger::error("%s has no CanvasPassComponents on same level", this->mID.c_str());
		}
	}

	void RenderCanvasComponentInstance::renderPasses() {
		for (auto canvasPass : mCanvasPassComponents) {
			canvasPass->draw();
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

	void RenderCanvasComponentInstance::setIsControlWindow(bool isControlWindowDraw) 
	{
		if (isControlWindowDraw) {
			mStockCanvasPasses[CanvasMaterialType::WARP].mSamplers["inTextureSampler"]->setTexture(*mCurrentInternalRT->mColorTexture);
		}
		else {
			mStockCanvasPasses[CanvasMaterialType::WARP].mSamplers["inTextureSampler"]->setTexture(*mFinalTexture);
		}
	}

	void RenderCanvasComponentInstance::setFinalSamplerTexture(RenderTexture2D* texture)
	{
		mStockCanvasPasses[CanvasMaterialType::WARP].mSamplers["inTextureSampler"]->setTexture(*texture);
	}

	


	void RenderCanvasComponentInstance::onDraw(IRenderTarget& renderTarget, VkCommandBuffer commandBuffer, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix)
	{
		// compute the model matrix with aspect ratio calculated with outputTexture and size and position with mTransformComponent
		
		computeModelMatrix(renderTarget, mModelMatrix, mFinalTexture, mTransformComponent);
		
			
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

		// Scale to fit targets
		outMatrix = glm::scale(outMatrix, glm::vec3(tex_size.x, tex_size.y, 1.0f));
	}

	void RenderCanvasComponentInstance::computeModelMatrix(const nap::IRenderTarget& target, glm::mat4& outMatrix, ResourcePtr<RenderTexture2D> canvas_output_texture, TransformComponentInstance* transform_comp)
	{
		//target is control window
		if (mIsControlWindowDraw)
		{
			glm::vec3 translate = transform_comp->getTranslate();
			glm::vec3 scale = transform_comp->getScale();
			glm::ivec2 canvas_tex_size = canvas_output_texture->getSize();
			glm::ivec2 target_size_main = mPresentationWindow->getBufferSize();
			glm::ivec2 target_size_controls = target.getBufferSize();

			
			// Calculate ratio
			float canvas_ratio = static_cast<float>(canvas_tex_size.x) / static_cast<float>(canvas_tex_size.y);
			float main_window_ratio = static_cast<float>(target_size_main.x) / static_cast<float>(target_size_main.y);
			float controls_window_ratio = static_cast<float>(target_size_controls.x) / static_cast<float>(target_size_controls.y);
			glm::ivec3 viewport_size = glm::highp_ivec3();
			//scale to a viewport dependent on main window ratio
			if (main_window_ratio > controls_window_ratio) {
				viewport_size.x = target_size_controls.x;
				viewport_size.y = target_size_controls.x / main_window_ratio;
			}
			else {
				viewport_size.y = target_size_controls.y;
				viewport_size.x = target_size_controls.y * main_window_ratio;
			}
			outMatrix = glm::translate(glm::mat4(), glm::vec3(
				translate.x * viewport_size.x + target_size_controls.x / 2.0f,
				translate.y * viewport_size.y + target_size_controls.y / 2.0f,
				0.0f));
			// normalize canvas size to viewport size
			if (main_window_ratio > canvas_ratio) {
				viewport_size.x = viewport_size.y * canvas_ratio;

			}
			else {
				viewport_size.y = viewport_size.x / canvas_ratio;
			}
			
			outMatrix = glm::scale(outMatrix, glm::vec3(
				viewport_size.x * scale.x,
				viewport_size.y * scale.y,
				1.0f));
		}
		//target is main window
		else {
			glm::vec3 translate = transform_comp->getTranslate();
			glm::vec3 scale = transform_comp->getScale();
			glm::ivec2 canvas_tex_size = canvas_output_texture->getSize();
			glm::ivec2 tex_size = target.getBufferSize();
			glm::ivec2 new_size;
			outMatrix = glm::translate(glm::mat4(), glm::vec3(
				translate.x * tex_size.x + tex_size.x / 2.0f,
				translate.y * tex_size.y + tex_size.y / 2.0f,
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

		

	}

	bool RenderCanvasComponentInstance::constructTextureAndRenderTarget(ResourcePtr<RenderTarget>& renderTarget, ResourcePtr<RenderTexture2D>& texture, bool transparent, utility::ErrorState& errorState) {
		//init mOutputTexture TODO: resize when videoChanged event?
		int width = 4;
		int height = 4;
		if (*mResolution >= 4) { // to establish a minimum texture resolution
			if (*mAspectRatio > 0.05) {
				width = (*mResolution) * (*mAspectRatio);
				height = *mResolution;
			}
			else {
				width = *mResolution;
				height = *mResolution;
			}
		}
		else {
			nap::Logger::info("%s: resolution left at minimum of 4", this->mID.c_str());
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