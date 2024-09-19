#pragma once
#include "canvasgroupcomponent.h"

#include <component.h>
#include <nap/resourceptr.h>
#include <videoplayer.h>
#include <image.h>
#include <entity.h>
#include <imguiservice.h>
#include <scene.h>
#include <rtti/objectptr.h>


namespace nap
{
	// Forward declares
	class FoglioComponentInstance;


	//handles pointer input events to edit canvases
	class NAPAPI FoglioComponent : public Component
	{
		RTTI_ENABLE(Component)
			DECLARE_COMPONENT(FoglioComponent, FoglioComponentInstance)

	public:

		virtual void getDependentComponents(std::vector<rtti::TypeInfo>& components) const override;

		std::vector<ResourcePtr<VideoPlayer>> mVideoPlayers;
		std::vector<ResourcePtr<Image>> mImages;

	};

	class NAPAPI FoglioComponentInstance : public ComponentInstance
	{
		RTTI_ENABLE(ComponentInstance)
			
	public:
		FoglioComponentInstance(EntityInstance& entity, Component& resource);

		virtual bool init(utility::ErrorState& errorState) override;

		void drawGUI();
		void drawVideosGUI();
		void renderVideos();
		bool mDrawBackdrop = false;
		EntityInstance* mSelectedCanvasGroup = nullptr;

		void drawCanvasGroupsHeadless();

		std::vector<EntityInstance*> mCanvasGroupEntities;

		void setControlWindowContext(bool isControlWindowDraw);

	private:

		void setCanvasShaderUniformsAndSamplers(nap::utility::ErrorState errorState);

		

		virtual void onDestroy() override;

		std::unique_ptr<Entity> mInternalInputsEntity = nullptr;
		std::unique_ptr<Entity> mVideoRenderEntity = nullptr;
		SpawnedEntityInstance   mSpawnedVideoRenderEntity;

		std::unique_ptr<nap::Scene> mInternalScene = nullptr;

		IMGuiService* mGuiService = nullptr;
		FoglioService* mFoglioService = nullptr;


		FoglioComponent* mResource = nullptr;


		std::vector<rtti::ObjectPtr<VideoPlayer>> mVideoPlayers;
		std::vector<rtti::ObjectPtr<Image>> mImages;

	};


}