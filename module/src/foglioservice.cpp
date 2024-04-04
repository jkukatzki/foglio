// Local Includes
#include "foglioservice.h"
#include "foglio_driver.h"
#include "canvasgroupcomponent.h"

// External Includes
#include <nap/core.h>
#include <nap/resourcemanager.h>
#include <nap/logger.h>
#include <iostream>
#include <midiinputcomponent.h>
#include <rendervideocomponent.h>
#include <scene.h>
#include <entity.h>
#include <rtti/objectptr.h>

RTTI_BEGIN_CLASS_NO_DEFAULT_CONSTRUCTOR(nap::FoglioService)
	RTTI_CONSTRUCTOR(nap::ServiceConfiguration*)
RTTI_END_CLASS

namespace nap
{
	bool FoglioService::init(nap::utility::ErrorState& errorState)
	{
		//Logger::info("Initializing FoglioService");
		mSceneService = getCore().getService<SceneService>();
		mRenderService = getCore().getService<RenderService>();
		mResourceManager = getCore().getResourceManager();
		return true;
	}


	void FoglioService::update(double deltaTime)
	{
	}
	
	void FoglioService::postResourcesLoaded() {
		auto drivers = getCore().getResourceManager()->getObjects<FoglioDriver>();
		//if object has items in midi relation array, check if midi input component exists, if not create it
		//for (auto driver : drivers) {
		//	if (driver->mMidiRelations.size() > 0 && getCore().getResourceManager()->getObjects<MidiInputComponent>().size() == 0) {
		//		EntityInstance* canvasGroupEntity = getCore().getResourceManager()->getObjects<CanvasGroupComponentInstance>()[0]->getEntityInstance();
		//		// Create a new MidiInputComponent and add it to canvasgroupcomponent
		//		MidiInputComponent* midiInputComponentResource = new MidiInputComponent();
		//		std::unique_ptr<ComponentInstance> midiInputComponent = std::make_unique<ComponentInstance>();
		//		const rtti::TypeInfo& instance_type = midiInputComponentResource->getInstanceType();
		//		std::unique_ptr<ComponentInstance> component_instance(instance_type.create<ComponentInstance>({ *canvasGroupEntity, *midiInputComponentResource }));
		//		assert(component_instance);
		//		getCore().getResourceManager()->getObjects<CanvasGroupComponentInstance>()[0]->getEntityInstance()->addComponent(std::move(component_instance));
		//	}
		//}
		nap::utility::ErrorState errorState = nap::utility::ErrorState();
		Scene* scene = *mSceneService->getScenes().begin();
		mVideoRenderEntity = new Entity();
		int addedIdIfExisting = 0;
		while (mResourceManager->findObject("videoRenderEntity" + addedIdIfExisting)) {
			addedIdIfExisting++;
		}
		mVideoRenderEntity->mID = "videoRenderEntity" + std::to_string(addedIdIfExisting);
		std::vector<ResourcePtr<VideoPlayer>> videoPlayers = getCore().getResourceManager()->getObjects<VideoPlayer>();
		for (auto videoPlayer : videoPlayers) {
			RenderVideoComponent* renderVideoComponentResource = new RenderVideoComponent();
			renderVideoComponentResource->mID = "renderVideoComponent" + videoPlayer->mID;
			nap::Logger::info("FoglioService : creating renderVideoComponent for video player %s", videoPlayer->mID.c_str());
			renderVideoComponentResource->mVideoPlayer = videoPlayer;
			auto texture = getCore().getResourceManager()->createObject<RenderTexture2D>();
			texture->mWidth = videoPlayer->getWidth();
			texture->mHeight = videoPlayer->getHeight();
			texture->mFormat = RenderTexture2D::EFormat::RGBA8;
			if (!texture->init(errorState))
				nap::Logger::error("FoglioService: could not init texture for video rendering");
			renderVideoComponentResource->mOutputTexture = texture;
			mVideoRenderEntity->mComponents.emplace_back(renderVideoComponentResource);
		}
		rtti::ObjectPtr<EntityInstance> videoRenderEntityInstance = scene->spawn(*mVideoRenderEntity, errorState).get();
		nap::Logger::info("FoglioService: videoRenderEntityInstance spawned %s", videoRenderEntityInstance->mID.c_str());
		for (auto cmp : videoRenderEntityInstance->getComponents()) {
			nap::Logger::info("Component existing on dynamically created video render entity: %s", cmp->mID.c_str());
		}
	}

	void FoglioService::renderRequiredVideos(nap::utility::ErrorState errorState) {
		 // create entity with rendervideocomponent
		for(auto mVideoRenderEntity->getComponents
	}

	void FoglioService::getDependentServices(std::vector<rtti::TypeInfo>& dependencies)
	{
		dependencies.emplace_back(RTTI_OF(RenderService));
		dependencies.emplace_back(RTTI_OF(SceneService));
	}
	

	void FoglioService::shutdown()
	{
	}
}
