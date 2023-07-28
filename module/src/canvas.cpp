#include "canvas.h"
#include "canvasshader.h"

#include "videoshader.h"
#include "nap/core.h"
#include "renderservice.h"

#include "renderglobals.h"



RTTI_BEGIN_CLASS_NO_DEFAULT_CONSTRUCTOR(nap::Canvas)
RTTI_CONSTRUCTOR(nap::Core&)
	RTTI_PROPERTY("VideoPlayer", &nap::Canvas::mVideoPlayer, nap::rtti::EPropertyMetaData::Required)
	RTTI_PROPERTY("MaskImage", &nap::Canvas::mMaskImage, nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("DefaultVertexBinding", &nap::Canvas::mMaterialWithBindings, nap::rtti::EPropertyMetaData::Required)
	RTTI_PROPERTY("CornerOffsets", &nap::Canvas::mCornerOffsets, nap::rtti::EPropertyMetaData::Default)
	RTTI_PROPERTY("Name", &nap::Canvas::mID, nap::rtti::EPropertyMetaData::Default);
RTTI_END_CLASS

namespace nap {
	Canvas::Canvas(Core& core) :
		mRenderService(core.getService<RenderService>()),
		mPlane(new PlaneMesh(core)),
		mOutputTexture(new RenderTexture2D(core))
	{
	}

	MeshInstance& Canvas::getMeshInstance() {
		return mPlane->getMeshInstance();
	}

	ResourcePtr<PlaneMesh> Canvas::getMesh() {
		return mPlane;
	}

	ResourcePtr<RenderTexture2D> Canvas::getOutputTexture() {
		return mOutputTexture;
	}


	bool Canvas::init(utility::ErrorState& errorState)
	{
		//init mOutputTexture TODO: resize when videoChanged event?
		mOutputTexture->mWidth = mVideoPlayer->getWidth();
		mOutputTexture->mHeight = mVideoPlayer->getHeight();
		mOutputTexture->mFormat = RenderTexture2D::EFormat::RGBA8;
		if (!mOutputTexture->init(errorState))
			return false;
		// Now create a plane and initialize it
		// The plane is positioned on update based on current texture output size and transform component
		mPlane->mSize = glm::vec2(1.0f, 1.0f);
		mPlane->mPosition = glm::vec3(0.0f, 0.0f, 0.0f);
		mPlane->mCullMode = ECullMode::Back;
		mPlane->mUsage = EMemoryUsage::DynamicWrite;
		mPlane->mColumns = 1;
		mPlane->mRows = 1;
		if (!errorState.check(mPlane->setup(errorState), "Unable to setup canvas plane %s", mID.c_str()))
			return false;
		// add plane vertex attributes
		mCornerOffsetAttribute = &(mPlane->getMeshInstance()).getOrCreateAttribute<glm::vec3>(vertexid::canvas::CornerOffset);
		mCornerOffsetAttribute->addData(mCornerOffsets[0]);
		mCornerOffsetAttribute->addData(mCornerOffsets[1]);
		mCornerOffsetAttribute->addData(mCornerOffsets[2]);
		mCornerOffsetAttribute->addData(mCornerOffsets[3]);

		//	create video material
		mVideoMaterial = mRenderService->getOrCreateMaterial<VideoShader>(errorState);
		if (!errorState.check(mVideoMaterial != nullptr, "%s: unable to get or create canvas material", mID.c_str()))
			return false;

		// create resource for video material instance
		mVideoMaterialInstResource.mBlendMode = EBlendMode::AlphaBlend;
		mVideoMaterialInstResource.mDepthMode = EDepthMode::NoReadWrite;
		mVideoMaterialInstResource.mMaterial = mVideoMaterial;
		if (!errorState.check(mVideoMaterialInstance.init(*mRenderService, mVideoMaterialInstResource, errorState), "%s: unable to instance video material", this->mID.c_str())) {
			return false;
		}

		// Ensure the mvp struct is available
		mMVPStruct = mVideoMaterialInstance.getOrCreateUniform(uniform::mvpStruct);
		if (!errorState.check(mMVPStruct != nullptr, "%s: Unable to find uniform MVP struct: %s in material: %s",
			this->mID.c_str(), uniform::mvpStruct, mVideoMaterialInstance.getMaterial().mID.c_str()))
			return false;

		// Get all matrices
		mModelMatrixUniform = ensureUniform(uniform::modelMatrix, errorState);
		mProjectMatrixUniform = ensureUniform(uniform::projectionMatrix, errorState);
		mViewMatrixUniform = ensureUniform(uniform::viewMatrix, errorState);



		
		if (mModelMatrixUniform == nullptr || mProjectMatrixUniform == nullptr || mViewMatrixUniform == nullptr) {
			return false;
		}
			
			
		

		// Get sampler inputs to update from canvas material
		mYSampler = ensureSampler(uniform::video::sampler::YSampler, errorState);
		mUSampler = ensureSampler(uniform::video::sampler::USampler, errorState);
		mVSampler = ensureSampler(uniform::video::sampler::VSampler, errorState);
		/***mMaskSampler = ensureSampler(uniform::canvas::sampler::MaskSampler, errorState);
		if (mMaskImage != nullptr) {

			mMaskSampler->setTexture(*mMaskImage);
		}***/
		if (mYSampler == nullptr || mUSampler == nullptr || mVSampler == nullptr) {
			return false;
		}
		
		videoChanged(*mVideoPlayer);

		// init mPlane.mMeshInstance
		return errorState.check(mPlane->getMeshInstance().init(errorState), "Unable to initialize canvas plane %s", mID.c_str());
	}

	nap::UniformMat4Instance* Canvas::ensureUniform(const std::string& uniformName, utility::ErrorState& error)
	{
		assert(mMVPStruct != nullptr);
		UniformMat4Instance* found_uniform = mMVPStruct->getOrCreateUniform<UniformMat4Instance>(uniformName);
		if (!error.check(found_uniform != nullptr,
			"%s: unable to find uniform: %s in material: %s", this->mID.c_str(), uniformName.c_str(),
			mVideoMaterialInstance.getMaterial().mID.c_str()))
			return nullptr;
		return found_uniform;
	}

	nap::Sampler2DInstance* Canvas::ensureSampler(const std::string& samplerName, utility::ErrorState& error)
	{
		Sampler2DInstance* found_sampler = mVideoMaterialInstance.getOrCreateSampler<Sampler2DInstance>(samplerName);
		if (!error.check(found_sampler != nullptr,
			"%s: unable to find sampler: %s in material: %s", this->mID.c_str(), samplerName.c_str(),
			mVideoMaterialInstance.getMaterial().mID.c_str()))
			return nullptr;
		return found_sampler;
	}

	void Canvas::videoChanged(VideoPlayer& player)
	{
		mYSampler->setTexture(player.getYTexture());
		mUSampler->setTexture(player.getUTexture());
		mVSampler->setTexture(player.getVTexture());
	}

}