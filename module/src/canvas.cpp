#include "canvas.h"
#include "canvasshader.h"
#include "nap/core.h"
#include "renderservice.h"


RTTI_BEGIN_CLASS_NO_DEFAULT_CONSTRUCTOR(nap::Canvas)
RTTI_CONSTRUCTOR(nap::Core&)
RTTI_PROPERTY("VideoPlayer", &nap::Canvas::mVideoPlayer, nap::rtti::EPropertyMetaData::Required)
RTTI_PROPERTY("MaskImage", &nap::Canvas::mMaskImage, nap::rtti::EPropertyMetaData::Default)
RTTI_PROPERTY("DefaultVertexBinding", &nap::Canvas::mMaterialWithBindings, nap::rtti::EPropertyMetaData::Required)
RTTI_PROPERTY("Name", &nap::Canvas::mID, nap::rtti::EPropertyMetaData::Default);
RTTI_END_CLASS

namespace nap {
	Canvas::Canvas(Core& core) :
		mRenderService(core.getService<RenderService>()),
		mPlane(new PlaneMesh(core))
	{
	}

	MeshInstance& Canvas::getMeshInstance() {
		return mPlane->getMeshInstance();
	}

	PlaneMesh* Canvas::getMesh() {
		return mPlane;
	}

	bool Canvas::init(utility::ErrorState& errorState)
	{
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

		mCornerOffsetAttribute = &(mPlane->getMeshInstance()).getOrCreateAttribute<glm::vec3>(vertexid::canvas::CornerOffset);
		mCornerOffsetAttribute->addData(glm::vec3(50.0, 50.0, 50.0));
		mCornerOffsetAttribute->addData(glm::vec3(50.0, -50.0, 50.0));
		return errorState.check(mPlane->init(errorState), "Unable to initialize canvas plane %s", mID.c_str());
	}

}