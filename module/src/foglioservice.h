#pragma once

// External Includes
#include <nap/service.h>
#include <nap/signalslot.h>
#include <videoservice.h>
#include <sceneservice.h>
#include <renderservice.h>
#include <imguiservice.h>
#include <rendervideocomponent.h>
#include <rtti/objectptr.h>
#include <scene.h>
#include <parametergui.h>

namespace nap
{
	class NAPAPI FoglioService : public Service
	{
		RTTI_ENABLE(Service)
	public:
		// Default Constructor
		FoglioService(ServiceConfiguration* configuration) : Service(configuration) {	}

		/**
		 * Use this call to register service dependencies
		 * A service that depends on another service is initialized after all it's associated dependencies
		 * This will ensure correct order of initialization, update calls and shutdown of all services
		 * @param dependencies rtti information of the services this service depends on
		 */
		virtual void getDependentServices(std::vector<rtti::TypeInfo>& dependencies) override;
		
		/**
		 * Initializes the service
		 * @param errorState contains the error message on failure
		 * @return if the foglio service was initialized correctly
		 */
		virtual bool init(nap::utility::ErrorState& errorState) override;
		
		/**
		 * Invoked by core in the app loop. Update order depends on service dependency
		 * This call is invoked after the resource manager has loaded any file changes but before
		 * the app update call. If service B depends on A, A:s:update() is called before B::update()
		 * @param deltaTime: the time in seconds between calls
		*/
		virtual void update(double deltaTime) override;
		
		/**
		 * Invoked when exiting the main loop, after app shutdown is called
		 * Use this function to close service specific handles, drivers or devices
		 * When service B depends on A, Service B is shutdown before A
		 */
		virtual void shutdown() override;

		void renderRequiredVideos();
		void updateVideosGUI(nap::utility::ErrorState errorState);

	private:
		ResourcePtr<Entity> mVideoRenderEntity;
		rtti::ObjectPtr<EntityInstance> mVideoRenderEntityInstance = nullptr;
		std::unique_ptr<SpawnedEntityInstance> mVideoRenderSpawnedEntityInstance = nullptr;
		std::map<ResourcePtr<VideoPlayer>, RenderVideoComponentInstance*> mRenderVideoComponentsMap;
		rtti::ObjectPtr<EntityInstance> mMidiInputEntityInstance = nullptr;

		SceneService* mSceneService = nullptr;
		RenderService* mRenderService = nullptr;
		IMGuiService* mGuiService = nullptr;

		std::unique_ptr<ParameterGroup> mNoGroupParametersGroup = nullptr;
		std::vector<ResourcePtr<ParameterGUI>> mParameterGUIObjects;
		std::unique_ptr<Scene> mDynamicScene = std::unique_ptr<Scene>(nullptr);

		void setupVideoRendering(nap::utility::ErrorState errorState);
		void setupMIDI();
		void setupCanvasShaderUniformsAndSamplers(Scene* scene, nap::utility::ErrorState errorState);
		void setupDrivers(nap::utility::ErrorState errorState);

		void setupParametersGUI(nap::utility::ErrorState errorState);
		


	protected:
		/**
		 * Called when a json file has been (re)loaded. Used to re-apply the presets.
		 */
		virtual void preResourcesLoaded() override;
		virtual void postResourcesLoaded() override;

		

	
	};
}
