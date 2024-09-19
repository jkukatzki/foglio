#include "fogliocomponent.h"
#include "foglioservice.h"

#include <nap/logger.h>
#include <imgui/imgui.h>
#include <imguiutils.h>
#include <rendervideocomponent.h>
#include <nap/core.h>
#include <entity.h>
#include <scene.h>
#include <canvasgroupcomponent.h>
#include <sequenceguiservice.h>

// nap::rendercanvascomponent run time class definition
RTTI_BEGIN_CLASS(nap::FoglioComponent)
RTTI_PROPERTY("VideoPlayers", &nap::FoglioComponent::mVideoPlayers, nap::rtti::EPropertyMetaData::Required)
RTTI_PROPERTY("Images", &nap::FoglioComponent::mImages, nap::rtti::EPropertyMetaData::Required)
RTTI_END_CLASS

RTTI_BEGIN_CLASS_NO_DEFAULT_CONSTRUCTOR(nap::FoglioComponentInstance)
RTTI_CONSTRUCTOR(nap::EntityInstance&, nap::Component&)
RTTI_END_CLASS

namespace nap
{
	void FoglioComponent::getDependentComponents(std::vector<rtti::TypeInfo>& components) const
	{

	}


	FoglioComponentInstance::FoglioComponentInstance(EntityInstance& entity, Component& resource) :
		ComponentInstance(entity, resource),
		mInternalScene(new Scene(*entity.getCore()))
	{
	}



	bool FoglioComponentInstance::init(utility::ErrorState& errorState)
	{
		// services
		mGuiService = getEntityInstance()->getCore()->getService<IMGuiService>();
		mFoglioService = getEntityInstance()->getCore()->getService<FoglioService>();

		mInternalScene->mID = "foglioInternalScene";
		if (!mInternalScene->init(errorState))
			return false;

		mResource = getComponent<FoglioComponent>();
		mVideoPlayers = mResource->mVideoPlayers;

		if (!mVideoPlayers.size() > 0) {

			nap::Logger::info("No VideoPlayer resources declared in FoglioComponent");

		}
		else {
			//create video render entity
			std::string uuid = math::generateUUID();
			mVideoRenderEntity = std::make_unique<Entity>();
			mVideoRenderEntity->mID = utility::stringFormat("%_video_%s", getEntityInstance()->mID.c_str(), uuid.c_str());

			for (auto videoPlayerRsc : mVideoPlayers) {
				auto videoPlayer = videoPlayerRsc.get();
				auto renderVideoComponentResource = new RenderVideoComponent();
				//TODO: handle case in which user has defined components with same name
				renderVideoComponentResource->mID = utility::stringFormat("%_video_render_%s", getEntityInstance()->mID.c_str(), uuid.c_str());
				nap::Logger::info("FoglioService : creating renderVideoComponent for video player %s", videoPlayer->mID.c_str());
				renderVideoComponentResource->mVideoPlayer = videoPlayer;
				auto texture = getEntityInstance()->getCore()->getResourceManager()->createObject<RenderTexture2D>();
				texture->mWidth = videoPlayer->getWidth();
				texture->mHeight = videoPlayer->getHeight();
				texture->mColorFormat = RenderTexture2D::EFormat::RGBA8;
				if (!texture->init(errorState))
					nap::Logger::error("FoglioService: could not init texture for video rendering");
				renderVideoComponentResource->mOutputTexture = texture;
				mVideoRenderEntity->mComponents.emplace_back(renderVideoComponentResource);
			}
			//mVideoRenderEntity->init(errorState);
			assert(mInternalScene != nullptr);
			mSpawnedVideoRenderEntity = mFoglioService->mInternalScene->spawn(*mVideoRenderEntity, errorState);
			

		}
		
		mImages = mResource->mImages;

		if (!mImages.size() > 0) {

			nap::Logger::info("No Image resources declared in FoglioComponent");

		}
		
		// populate mCanvasGroups
		for (auto entity : getEntityInstance()->getChildren()) {
			if (entity->hasComponent<CanvasGroupComponentInstance>()) {
				mCanvasGroupEntities.emplace_back(entity);
			}
		}
		setCanvasShaderUniformsAndSamplers(errorState);

		// set first canvas group as selected
		if (mCanvasGroupEntities.size() > 0) {
			mSelectedCanvasGroup = mCanvasGroupEntities[0];
		}

	}

	void FoglioComponentInstance::setControlWindowContext(bool isControlWindowDraw) {
		for (auto canvasGroup : mCanvasGroupEntities) {
			for (auto canvasEntity : canvasGroup->getChildren()) {
				canvasEntity->getComponent<RenderCanvasComponentInstance>().mIsControlWindowDraw = false;
			}
			canvasGroup->getComponent<CanvasGroupComponentInstance>().setSelectedTextureControlOverlay(false);
		}
	}

	void FoglioComponentInstance::drawGUI() {
		ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x / 2.0, 0.0f));
		ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x / 2.0, ImGui::GetIO().DisplaySize.y));
		ImGui::Begin("Foglio Dashboard");
		if (ImGui::Button("Toggle Backdrop")) {
			mDrawBackdrop = !mDrawBackdrop;
		}
		ImGui::Text("Canvas Groups");
		for (EntityInstance* canvasGroupEntity : getEntityInstance()->getChildren()) {
			ImGuiTreeNodeFlags node_flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
			if (mSelectedCanvasGroup == canvasGroupEntity) {
				node_flags |= ImGuiTreeNodeFlags_Selected;
			}
			ImGui::TreeNodeEx((EntityInstance*)canvasGroupEntity, node_flags, canvasGroupEntity->getEntity()->mID.c_str());
			if (ImGui::IsItemClicked())
			{
				mSelectedCanvasGroup = canvasGroupEntity;
			}
		}

		ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_None;
		if (ImGui::BeginTabBar("FoglioDashboardTabBar", tab_bar_flags))
		{
			if (ImGui::BeginTabItem("Videos"))
			{
				drawVideosGUI();
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("Parameters"))
			{
				//for (auto parameterGUI : mParameterGUIObjects) {
				//	parameterGUI->show(false);
				//}

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

	void FoglioComponentInstance::drawCanvasGroupsHeadless() {
		for (auto canvasGroup : mCanvasGroupEntities) {
			auto cmp = canvasGroup->findComponent<CanvasGroupComponentInstance>();
			cmp->drawAllHeadless();
			cmp->drawSelectedInterface();
		}
	}

	void FoglioComponentInstance::drawVideosGUI() {

		nap::utility::ErrorState errorState = nap::utility::ErrorState();
		std::vector<RenderVideoComponentInstance*> renderVideoComponents;
		if (mSpawnedVideoRenderEntity != nullptr) {
			ImGui::Text("FoglioService: videoRenderEntityInstance present");
			mSpawnedVideoRenderEntity->getComponentsOfType(renderVideoComponents);
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

	void FoglioComponentInstance::setCanvasShaderUniformsAndSamplers(nap::utility::ErrorState errorState) {
		// canvas pass shader declarations handling
		for (EntityInstance* entity : getEntityInstance()->getChildren()) {
			std::vector<CanvasGroupComponentInstance*> canvas_groups;
			entity->getComponentsOfType(canvas_groups);


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
						canvasPass->mSamplers[key] = canvasPass->ensureSampler(key, canvasPass->mMaterialInstance, errorState);

						ResourcePtr<ImageFromFile> image = nullptr;
						auto imageIterator = std::find_if(mImages.cbegin(), mImages.cend(), [&keyStripped](const ResourcePtr<ImageFromFile>& image) { return image->mID == keyStripped; });
						if (imageIterator != mImages.end()) {
							//image found with matching name
							image = *imageIterator;
							nap::Logger::info("Setting texture of sampler to image %s", keyStripped.c_str());
							canvasPass->mSamplers[key]->setTexture(*image.get());
						}
						else {
							nap::Logger::error("%s: sampler declaration links to resource which does not exist with name %s", canvasPass->mID.c_str(), keyStripped);
							canvasPass->mShaderDeclarationSetupErrors[key].emplace_back(canvasPass->mID + ": image sampler declaration links to resource which does not exist (" + keyStripped + ")");
						}
					}
					// sampler declaration relating to video player found (starts with "v_")
					else if (key.find("v_") == 0) {
						std::string keyStripped = key.substr(2, key.length());
						nap::Logger::info("%s: creating sampler for video player resource %s", entity->mID.c_str(), key);
						canvasPass->mSamplers[key] = canvasPass->ensureSampler(key, canvasPass->mMaterialInstance, errorState);

						std::vector<RenderVideoComponentInstance*> renderVideoComponents;
						if (mSpawnedVideoRenderEntity != nullptr) {
							mSpawnedVideoRenderEntity->getComponentsOfType(renderVideoComponents);
						}
						else {
							nap::Logger::error("FoglioService: no videoRenderEntityInstance present");
						}
						auto videoIterator = std::find_if(renderVideoComponents.cbegin(), renderVideoComponents.cend(), [&keyStripped](RenderVideoComponentInstance* const & videoRender) { return videoRender->getComponent<RenderVideoComponent>()->mVideoPlayer->mID == keyStripped; });
						if (videoIterator != renderVideoComponents.end()) {
							//image found with matching name
							RenderVideoComponentInstance* videoRender = *videoIterator;
							nap::Logger::info("Setting texture of sampler to video %s", keyStripped.c_str());
							canvasPass->mSamplers[key]->setTexture(videoRender->getOutputTexture());
						}
						else {
							nap::Logger::error("%s: sampler declaration links to resource which does not exist with name %s", canvasPass->mID.c_str(), keyStripped);
							canvasPass->mShaderDeclarationSetupErrors[key].emplace_back(canvasPass->mID + ": video sampler declaration links to resource which does not exist (" + keyStripped + ")");
						}
					}
				}
			}
		}
	}

	void FoglioComponentInstance::renderVideos() {
		std::vector<RenderVideoComponentInstance*> renderVideoComponents;

		if (mSpawnedVideoRenderEntity != nullptr) {
			mSpawnedVideoRenderEntity->getComponentsOfType(renderVideoComponents);
		}

		for (RenderVideoComponentInstance* renderVideoComponent : renderVideoComponents) {
			renderVideoComponent->draw();
		}
	}

	void FoglioComponentInstance::onDestroy()
	{
		if (mInternalScene) {
			mInternalScene->onDestroy();
			mInternalScene.reset();
		}
	}
}