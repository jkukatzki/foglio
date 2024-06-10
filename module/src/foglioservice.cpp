// Local Includes
#include "foglioservice.h"
#include "foglio_driver.h"
#include "canvasgroupcomponent.h"
#include "IconsFontAwesome6.h"

// External Includes
#include <nap/core.h>
#include <nap/resourcemanager.h>
#include <nap/logger.h>
#include <iostream>
#include <imgui/imgui.h>
#include <imguiutils.h>

#include <parameter.h>
#include <parametergroup.h>

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
		Logger::info("Initializing FoglioService");
		mSceneService = getCore().getService<SceneService>();
		mRenderService = getCore().getService<RenderService>();
		mGuiService = getCore().getService<IMGuiService>();
		mVideoRenderEntity = getCore().getResourceManager()->createObject<Entity>();
		return true;
	}


	void FoglioService::update(double deltaTime)
	{
		nap::utility::ErrorState errorState;
		mGuiService->selectWindow(getCore().getResourceManager()->findObject("ControlsWindow"));
		ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x / 2.0, 0.0f));
		ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x / 2.0, ImGui::GetIO().DisplaySize.y));
		ImGui::Begin("Foglio Dashboard");
		ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_None;
		if (ImGui::BeginTabBar("FoglioDashboardTabBar", tab_bar_flags))
		{
			if (ImGui::BeginTabItem("Videos"))
			{
				updateVideosGUI(errorState);
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("Parameters"))
			{
				for (auto parameterGUI : mParameterGUIObjects) {
					parameterGUI->show(false);
				}
				
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("MIDI"))
			{
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}
		
		ImGui::End();
	}

	void FoglioService::preResourcesLoaded() {
		nap::Logger::info("FoglioService: preResourcesLoaded call");
		mParameterGUIObjects.clear();
		mRenderVideoComponentsMap.clear();
		mVideoRenderEntityInstance = nullptr;
	}
	
	void FoglioService::postResourcesLoaded() {
		nap::utility::ErrorState errorState = nap::utility::ErrorState();
		//put parameters that do not belong to a ParameterGroup into general group so they can be displayed using ParameterGui::show
		//also create ParameterGUI objects for every group
		mNoGroupParametersGroup = getCore().getResourceManager()->createObject<ParameterGroup>();
		mNoGroupParametersGroup->mID = "foglioNoGroupParametersGroup";
		auto parameters = getCore().getResourceManager()->getObjects<Parameter>();
		auto parameterGroups = getCore().getResourceManager()->getObjects<ParameterGroup>();
		for (auto parameter : parameters) {
			bool found = false;
			nap::Logger::info("FoglioService: checking parameter %s", parameter->mID.c_str());
			for (auto group : parameterGroups) {
				if (group->findObjectRecursive(parameter->mID) != nullptr) {
					found = true;
					break;
				}
			}
			if (!found) {
				nap::Logger::info("FoglioService: No parameter group associated with parameter %s, adding it to dynamically created group for GUI", parameter);
				mNoGroupParametersGroup->mMembers.emplace_back(parameter);
			}
		}
		if (!mNoGroupParametersGroup->init(errorState)) {
			nap::Logger::error("Could not init mNoGroupParametersGroup");
		}
		for (auto group : parameterGroups) {
			nap::Logger::info("creating parameter gui automatically for group %s", group->mID.c_str());
			auto parameterGUI = getCore().getResourceManager()->createObject<ParameterGUI>();
			parameterGUI->mParameterGroup = group;
			parameterGUI->mID = "foglioAutomaticParameterGUI" + group->mID;
			if (!parameterGUI->init(errorState)) {
				nap::Logger::error("Init of parameterGUI failed %s", parameterGUI->mID.c_str());
			}
			mParameterGUIObjects.emplace_back(parameterGUI);
		}
		//check for dependencies in foglio drivers
		auto drivers = getCore().getResourceManager()->getObjects<FoglioDriver>();
		for (auto driver : drivers) {
			if (driver->mMidiRelations.size() > 0) {
				//MIDI INPUT
				nap::utility::ErrorState errorState = nap::utility::ErrorState();
				Scene* scene = *mSceneService->getScenes().begin();
				Entity* midiInputEntity = new Entity();
				int addedIdIfExisting = 0;
				while (getCore().getResourceManager()->findObject("midiInputEntity" + addedIdIfExisting) != nullptr) {
					addedIdIfExisting++;
				}
				midiInputEntity->mID = "midiInputEntity" + std::to_string(addedIdIfExisting);
				MidiInputComponent* midiInputComponentResource = new MidiInputComponent();
				midiInputComponentResource->mID = "foglio_midiInputComponent" + addedIdIfExisting;
				nap::Logger::info("FoglioService : creating MidiInputComponent because driver with midi dependency is present");
				midiInputEntity->mComponents.emplace_back(midiInputComponentResource);
				mMidiInputEntityInstance = scene->spawn(*midiInputEntity, errorState).get();
				nap::Logger::info("FoglioService: midiInputEntityInstance spawned %s", mMidiInputEntityInstance->mID.c_str());
				for (auto cmp : mMidiInputEntityInstance->getComponents()) {
					nap::Logger::info("Component existing on dynamically created midi input entity: %s", cmp->mID.c_str());
				}
			}
		}
		// VIDEO RENDERING
		//Scene* scene = *(mSceneService->getScenes().begin());
		Scene* scene;
		for (auto it = mSceneService->getScenes().begin(); it != mSceneService->getScenes().end(); ++it) {
			auto& oneOfTheScenes = *it; // Dereference the iterator to access the scene object
			nap::Logger::info("FoglioService: Inspecting scene %s", oneOfTheScenes->mID.c_str());
			for (auto ent : oneOfTheScenes->getEntities()) {
				nap::Logger::info("FoglioService: (inside for loop that inspects all scenes) Entity %s in scene %s", ent->mID.c_str(), oneOfTheScenes->mID.c_str());
			}
			scene = oneOfTheScenes;
		}
		nap::Logger::info("amount of scenes: %i", mSceneService->getScenes().size());
		for (auto ent : scene->getEntities()) {
			nap::Logger::info("FoglioService: Entity %s in scene %s (last scene in SceneService::getScenes() SceneSet and before spawning EntityInstance for video rendering)", ent->mID.c_str(), scene->mID.c_str());
		}
		
		//handle the case that an entity defined by user in object.json with same name as automatically created entity exists
		int addedIdIfExisting = 0;
		while (getCore().getResourceManager()->findObject("videoRenderEntity" + addedIdIfExisting) != nullptr) {
			addedIdIfExisting++;
		}
		ResourcePtr<Entity> videoRenderEntity = getCore().getResourceManager()->createObject<Entity>();
		videoRenderEntity->mID = "videoRenderEntity" + std::to_string(addedIdIfExisting);
		//create a VideoRenderComponent for every VideoPlayer found by the ResourceManger and add it to our videoRenderEntity
		std::vector<ResourcePtr<VideoPlayer>> videoPlayers = getCore().getResourceManager()->getObjects<VideoPlayer>();
		for (auto videoPlayer : videoPlayers) {
			auto renderVideoComponentResource = getCore().getResourceManager()->createObject<RenderVideoComponent>();
			//TODO: handle case in which user has defined components with same name
			renderVideoComponentResource->mID = "foglio_renderVideoComponent" + videoPlayer->mID;
			nap::Logger::info("FoglioService : creating renderVideoComponent for video player %s", videoPlayer->mID.c_str());
			renderVideoComponentResource->mVideoPlayer = videoPlayer;
			auto texture = getCore().getResourceManager()->createObject<RenderTexture2D>();
			texture->mWidth = videoPlayer->getWidth();
			texture->mHeight = videoPlayer->getHeight();
			texture->mFormat = RenderTexture2D::EFormat::RGBA8;
			if (!texture->init(errorState))
				nap::Logger::error("FoglioService: could not init texture for video rendering");
			renderVideoComponentResource->mOutputTexture = texture;
			videoRenderEntity->mComponents.emplace_back(renderVideoComponentResource);
		}
		//spawn the entity holding all the video render components
		mVideoRenderEntityInstance = scene->spawn(*videoRenderEntity, errorState).get();
		nap::Logger::info("FoglioService: videoRenderEntityInstance spawned %s", mVideoRenderEntityInstance->mID.c_str());
		
		//assign values in mRenderVideoComponentsMap for later access of components
		std::vector<RenderVideoComponentInstance*> videoCmps;
		mVideoRenderEntityInstance->getComponentsOfType(videoCmps);
		for (auto cmp : videoCmps) {
			nap::Logger::info("Component existing on dynamically created video render entity: %s", cmp->mID.c_str());
			mRenderVideoComponentsMap[cmp->getComponent<RenderVideoComponent>()->mVideoPlayer] = cmp;
		}
		for (auto ent : scene->getEntities()) {
			nap::Logger::info("FoglioService: Entity %s in scene %s (last scene in SceneService::getScenes() SceneSet and after spawning EntityInstances for video rendering)", ent->mID.c_str(), scene->mID.c_str());
		}

		// canvas pass shader declarations handling
		for (EntityInstance* entity : scene->getEntities()) {
			std::vector<CanvasPassComponentInstance*> canvas_passes;
			entity->getComponentsOfType(canvas_passes);
			for (CanvasPassComponentInstance* canvasPass : canvas_passes) {
				for (auto samplerDeclaration : canvasPass->mPassShader->getSamplerDeclarations()) {
					nap::Logger::info("Setting up sampler declaration %s", samplerDeclaration.mName);
					std::string key = samplerDeclaration.mName;
					// sampler declaration relating to image found (starts with "i_")
					if (key.find("i_") == 0) {
						std::string keyStripped = key.substr(2, key.length());
						nap::Logger::info("%s: creating sampler for image resource %s", entity->mID.c_str(), key);
						canvasPass->mSamplers[key] = canvasPass->ensureSampler(key, errorState);
						auto image = getCore().getResourceManager()->findObject(keyStripped);
						if (image != nullptr) 
						{
							if (image.getWrappedType() == rtti::TypeInfo::get<ImageFromFile>()) 
							{
								ResourcePtr<ImageFromFile> imageResource = getCore().getResourceManager()->findObject<ImageFromFile>(keyStripped);
								if (imageResource != nullptr) 
								{
									nap::Logger::info("Setting texture of sampler to image %s", keyStripped.c_str());
									canvasPass->mSamplers[key]->setTexture(*imageResource.get());
								}
							}
							else 
							{
								nap::Logger::error("%s: sampler relating to image exists with same name as resource in scene that is not of type <ImageFromFile>, therefore not binding %s", canvasPass->mID.c_str(), key);
								canvasPass->mShaderDeclarationSetupErrors[key].emplace_back(canvasPass->mID + ": image sampler declaration links to resource which is not of type <ImageFromFile>");
							}
						}
						else 
						{
							nap::Logger::error("%s: sampler declaration links to resource which does not exist with name %s", canvasPass->mID.c_str(), keyStripped);
							canvasPass->mShaderDeclarationSetupErrors[key].emplace_back(canvasPass->mID + ": image sampler declaration links to resource which does not exist ("+keyStripped+")");
						}
					}
					// sampler declaration relating to video player found (starts with "v_")
					else if (key.find("v_") == 0) {
						std::string keyStripped = key.substr(2, key.length());
						nap::Logger::info("%s: creating sampler for video player resource %s", entity->mID.c_str(), key);
						canvasPass->mSamplers[key] = canvasPass->ensureSampler(key, errorState);
						auto videoPlayer = getCore().getResourceManager()->findObject(keyStripped);
						if (videoPlayer != nullptr)
						{
							if (videoPlayer.getWrappedType() == rtti::TypeInfo::get<VideoPlayer>())
							{
								ResourcePtr<VideoPlayer> videoPlayerResource = getCore().getResourceManager()->findObject<VideoPlayer>(key);
								if (videoPlayerResource != nullptr)
								{
									canvasPass->mSamplers[key]->setTexture(mRenderVideoComponentsMap[videoPlayerResource]->getOutputTexture());
								}
							}
							else
							{
								nap::Logger::error("%s: sampler relating to video player exists with same name as resource in scene that is not of type <VideoPlayer>, therefore not binding %s", canvasPass->mID.c_str(), key);
								canvasPass->mShaderDeclarationSetupErrors[key].emplace_back(canvasPass->mID + ": video player sampler declaration links to resource which is not of type <VideoPlayer>");
							}
						}
						else
						{
							nap::Logger::error("%s: sampler declaration links to resource which does not exist with name %s", canvasPass->mID.c_str(), keyStripped);
							canvasPass->mShaderDeclarationSetupErrors[key].emplace_back(canvasPass->mID + ": video player sampler declaration links to resource which does not exist (" + keyStripped + ")");
						}
					}
				}
			}
		}
		

	}

	void FoglioService::renderRequiredVideos() {
		std::vector<RenderVideoComponentInstance*> renderVideoComponents;
		
		if (mVideoRenderEntityInstance != nullptr) {
			mVideoRenderEntityInstance->getComponentsOfType(renderVideoComponents);
		}

		for (RenderVideoComponentInstance* renderVideoComponent : renderVideoComponents) {
			renderVideoComponent->draw();
		}
	}

	void FoglioService::updateVideosGUI(nap::utility::ErrorState errorState) {
		std::vector<RenderVideoComponentInstance*> renderVideoComponents;
		if (mVideoRenderEntityInstance != nullptr) {
			mVideoRenderEntityInstance->getComponentsOfType(renderVideoComponents);
		}
		else {
			ImGui::Text("FoglioService: no videoRenderEntityInstance present");
		}

		for (RenderVideoComponentInstance* renderVideoComponent : renderVideoComponents) {
			VideoPlayer* video_player = renderVideoComponent->getComponent<RenderVideoComponent>()->mVideoPlayer.get();
			ImGui::Text(video_player->mID.c_str());
			ImGui::Text(video_player->getFile().mPath.c_str());
			////texture display
			Texture2D& video_tex = renderVideoComponent->getOutputTexture();
			float col_width = ImGui::GetContentRegionAvailWidth();
			float ratio_video_tex = static_cast<float>(video_tex.getWidth()) / static_cast<float>(video_tex.getHeight());
			ImGui::Image(video_tex, { col_width , col_width / ratio_video_tex });
			//video player controls
			
			float current_time = video_player->getCurrentTime();
			if (ImGui::SliderFloat("", &current_time, 0.0f, video_player->getDuration(), "%.3fs", 1.0f))
				video_player->seek(current_time);
			ImGui::Text("Total time: %fs", video_player->getDuration());
			ImGui::BeginGroup();
			std::string playControlIcon = video_player->isPlaying() ? nap::icon::sequencer::pause : nap::icon::sequencer::play;

			if (ImGui::ArrowButton(("##left" + renderVideoComponent->mID).c_str(), ImGuiDir_Left)) {
				if (video_player->getIndex() == 0) {
					video_player->selectVideo(video_player->getCount() - 1, errorState);
					video_player->play();
				}
				else {
					video_player->selectVideo((video_player->getIndex() - 1) % video_player->getCount(), errorState);
					video_player->play();
				}
			}
			ImGui::SameLine();
			if (ImGui::ImageButton(mGuiService->getIcon(playControlIcon.c_str())))
			{
				if (video_player->isPlaying()) {
					video_player->stopPlayback();
				}
				else {
					video_player->play();
				}
			}
			ImGui::SameLine();
			if (ImGui::ArrowButton(("##right" + renderVideoComponent->mID).c_str(), ImGuiDir_Right)) {
				video_player->selectVideo((video_player->getIndex() + 1) % video_player->getCount(), errorState);
				video_player->play();
			}
			ImGui::EndGroup();
		}
		
	}

	void FoglioService::getDependentServices(std::vector<rtti::TypeInfo>& dependencies)
	{
		dependencies.emplace_back(RTTI_OF(VideoService));
		dependencies.emplace_back(RTTI_OF(RenderService));
		dependencies.emplace_back(RTTI_OF(SceneService));
		dependencies.emplace_back(RTTI_OF(IMGuiService));
	}
	

	void FoglioService::shutdown()
	{
	}
}
