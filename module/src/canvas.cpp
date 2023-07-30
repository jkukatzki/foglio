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
	CanvasMaterialItem::CanvasMaterialItem() {}

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

		mCanvasMaterialItems.insert(std::pair<std::string, CanvasMaterialItem>("video", CanvasMaterialItem()));
		constructMaterialInstance(CanvasMaterialTypes::VIDEO, errorState);
		//constructMaterialInstance(CanvasMaterialTypes::CANVASOUTPUT, errorState);

		

		
			
			
		

		
		/***mMaskSampler = ensureSampler(uniform::canvas::sampler::MaskSampler, errorState);
		if (mMaskImage != nullptr) {

			mMaskSampler->setTexture(*mMaskImage);
		}***/
		
		videoChanged(*mVideoPlayer);

		// init mPlane.mMeshInstance
		return errorState.check(mPlane->getMeshInstance().init(errorState), "Unable to initialize canvas plane %s", mID.c_str());
	}

	bool Canvas::constructMaterialInstance(CanvasMaterialTypes type, utility::ErrorState& error) {
		CanvasMaterialItem* materialItem = nullptr;
		switch (type) {
			case CanvasMaterialTypes::VIDEO : {
				materialItem = &mCanvasMaterialItems["video"];
				//create video material
				//create material resource
				materialItem->mMaterialInstResource = new MaterialInstanceResource();
				materialItem->mMaterialInstResource->mBlendMode = EBlendMode::AlphaBlend;
				materialItem->mMaterialInstResource->mDepthMode = EDepthMode::NoReadWrite;
				materialItem->mMaterial = mRenderService->getOrCreateMaterial<VideoShader>(error);
				break;
			}
			default:
			{
				nap::Logger::info("Unspecified shader in Canvas::constructMaterialInstance");
				break;
			}
		}

		if (materialItem == nullptr)
			return false;
		if (!error.check(materialItem->mMaterial != nullptr, "%s: unable to get or create video material", mID.c_str()))
			return false;
		materialItem->mMaterialInstance = new MaterialInstance();
		materialItem->mMaterialInstResource->mMaterial = materialItem->mMaterial;
		if (!error.check(materialItem->mMaterialInstance->init(*mRenderService, *materialItem->mMaterialInstResource, error), "%s: unable to instance %s material", this->mID.c_str(), type)) {
			return false;
		}
		//create mvp struct on material instance, regardless of type
		materialItem->mMVPStruct = materialItem->mMaterialInstance->getOrCreateUniform(uniform::mvpStruct);
		if (!error.check(materialItem->mMVPStruct != nullptr, "%s: Unable to find uniform MVP struct: %s in material: %s",
			this->mID.c_str(), uniform::mvpStruct, materialItem->mMaterial->mID.c_str()))
			return false;
		// Get all matrices
		materialItem->mModelMatrixUniform = ensureUniform(uniform::modelMatrix, *materialItem, error);
		materialItem->mProjectMatrixUniform = ensureUniform(uniform::projectionMatrix, *materialItem, error);
		materialItem->mViewMatrixUniform = ensureUniform(uniform::viewMatrix, *materialItem, error);

		if (materialItem->mModelMatrixUniform == nullptr || materialItem->mProjectMatrixUniform == nullptr || materialItem->mViewMatrixUniform == nullptr) {
			return false;
		}

		//sampler definitions
		switch (type) {
		case CanvasMaterialTypes::VIDEO: {
			materialItem->mSamplers["YSampler"] = ensureSampler(uniform::video::sampler::YSampler, *materialItem, error);
			materialItem->mSamplers["USampler"] = ensureSampler(uniform::video::sampler::USampler, *materialItem, error);
			materialItem->mSamplers["VSampler"] = ensureSampler(uniform::video::sampler::VSampler, *materialItem, error);

			if (materialItem->mSamplers["YSampler"] == nullptr || materialItem->mSamplers["USampler"] == nullptr || materialItem->mSamplers["VSampler"] == nullptr)
				return false;

		}
		default:
		{
			nap::Logger::info("Unspecified shader in Canvas::constructMaterialInstance");
			break;
		}
		}

	}

	nap::UniformMat4Instance* Canvas::ensureUniform(const std::string& uniformName, CanvasMaterialItem& materialItem, utility::ErrorState& error)
	{
		assert(mMVPStruct != nullptr);
		UniformMat4Instance* found_uniform = materialItem.mMVPStruct->getOrCreateUniform<UniformMat4Instance>(uniformName);
		if (!error.check(found_uniform != nullptr,
			"%s: unable to find uniform: %s in material: %s", this->mID.c_str(), uniformName.c_str(),
			materialItem.mMaterial->mID.c_str()))
			return nullptr;
		return found_uniform;
	}

	nap::Sampler2DInstance* Canvas::ensureSampler(const std::string& samplerName, CanvasMaterialItem& materialItem, utility::ErrorState& error)
	{
		Sampler2DInstance* found_sampler = materialItem.mMaterialInstance->getOrCreateSampler<Sampler2DInstance>(samplerName);
		if (!error.check(found_sampler != nullptr,
			"%s: unable to find sampler: %s in material: %s", this->mID.c_str(), samplerName.c_str(),
			materialItem.mMaterial->mID.c_str()))
			return nullptr;
		return found_sampler;
	}

	void Canvas::videoChanged(VideoPlayer& player)
	{
		mCanvasMaterialItems["video"].mSamplers["YSampler"]->setTexture(player.getYTexture());
		mCanvasMaterialItems["video"].mSamplers["USampler"]->setTexture(player.getUTexture());
		mCanvasMaterialItems["video"].mSamplers["VSampler"]->setTexture(player.getVTexture());
	}

}