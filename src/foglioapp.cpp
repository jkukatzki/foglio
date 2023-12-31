#include "foglioapp.h"

// External Includes

#include <utility/fileutils.h>
#include <nap/logger.h>
#include <nap/core.h>
#include <imgui/imgui.h>
#include <inputrouter.h>
#include <rendercanvascomponent.h>
#include <canvasgroupcomponent.h>
#include <foglioservice.h>
#include <perspcameracomponent.h>
#include <orthocameracomponent.h>
#include <imguiutils.h>

#include <sequenceplayereventoutput.h>
#include <sequenceevent.h>



RTTI_BEGIN_CLASS_NO_DEFAULT_CONSTRUCTOR(nap::foglioApp)
	RTTI_CONSTRUCTOR(nap::Core&)
RTTI_END_CLASS

namespace nap 
{
	/**
	 * Initialize all the resources and instances used for drawing
	 * slowly migrating all functionality to NAP
	 */
	bool foglioApp::init(utility::ErrorState& error)
	{
		// Retrieve services
		mRenderService = getCore().getService<nap::RenderService>();
		mSceneService = getCore().getService<nap::SceneService>();
		mInputService = getCore().getService<nap::InputService>();
		mGuiService = getCore().getService<nap::IMGuiService>();

		// Fetch the resource manager
		mResourceManager = getCore().getResourceManager();

		// Get the render window
		ResourcePtr<nap::RenderWindow> dynWindow = mResourceManager->createObject<nap::RenderWindow>();
		mMainWindow = mResourceManager->findObject<nap::RenderWindow>("MainWindow");
		if (!error.check(mMainWindow != nullptr, "unable to find render window with name: %s", "MainWindow"))
			return false;
		// Get the render window
		mControlsWindow = mResourceManager->findObject<nap::RenderWindow>("ControlsWindow");
		if (!error.check(mControlsWindow != nullptr, "unable to find render window with name: %s", "ControlsWindow"))
			return false;
		mCanvasSequenceEditorGUI = mResourceManager->findObject<nap::SequenceEditorGUI>("CanvasSequenceEditorGUI");
		// Get the scene that contains our entities and components
		mScene = mResourceManager->findObject<Scene>("Scene");
		if (!error.check(mScene != nullptr, "unable to find scene with name: %s", "Scene"))
			return false;


		// Get the camera entity
		mCameraEntity = mScene->findEntity("CameraEntity");
		if (!error.check(mCameraEntity != nullptr, "unable to find camera entity with name: %s", "CameraEntity"))
			return false;
		mOrthoCameraEntity = mScene->findEntity("OrthoCameraEntity");
		if (!error.check(mOrthoCameraEntity != nullptr, "unable to find camera entity with name: %s", "OrthoCameraEntity"))
			return false;
		mVideoWallEntity = mScene->findEntity("VideoWallEntity");
		if (!error.check(mVideoWallEntity != nullptr, "unable to find video wall entity with name: %s", "VideoWallEntity"))
			return false;

		// All done!
		return true;
	}
	
	
	// Called when the window is updating
	void foglioApp::update(double deltaTime)
	{
		// Use a default input router to forward input events (recursively) to all input components in the default scene
		nap::DefaultInputRouter input_router(true);
		//mInputService->processWindowEvents(*mMainWindow, input_router, { &mScene->getRootEntity() });
		mInputService->processWindowEvents(*mControlsWindow, input_router, { &mScene->getRootEntity() });
		updateGUI();
	}
	
	
	// Called when the window is going to render
	void foglioApp::render()
	{
		// Signal the beginning of a new frame, allowing it to be recorded.
		// The system might wait until all commands that were previously associated with the new frame have been processed on the GPU.
		// Multiple frames are in flight at the same time, but if the graphics load is heavy the system might wait here to ensure resources are available.
		mRenderService->beginFrame();

		// Find the orthographic camera component
		nap::OrthoCameraComponentInstance& ortho_cam = mOrthoCameraEntity->getComponent<OrthoCameraComponentInstance>();
		//get canvases once again for the render service
		std::vector<nap::RenderableComponentInstance*> canvas_components_to_render;
		for (nap::EntityInstance* canvasEntity : mVideoWallEntity->getChildren()) {
			canvas_components_to_render.emplace_back(&canvasEntity->getComponent<RenderableComponentInstance>());
		};

		CanvasGroupComponentInstance* canvasGroupComponent = &mVideoWallEntity->getComponent<CanvasGroupComponentInstance>();
		// Start recording into the headless recording buffer.
		if (mRenderService->beginHeadlessRecording())
		{
			canvasGroupComponent->drawAllHeadless();
			canvasGroupComponent->drawSelectedInterface();
			// Tell the render service we are done rendering into render-targets.
			// The queue is submitted and executed.
			mRenderService->endHeadlessRecording();
		}

		canvasGroupComponent->getSelected()->getComponent<RenderCanvasComponentInstance>().setFinalSampler(false);
		for (auto canvasEntity : mVideoWallEntity->getChildren()) {
			canvasEntity->getComponent<RenderCanvasComponentInstance>().mIsControlViewDraw = false;
		}
		if (mRenderService->beginRecording(*mMainWindow)) {
			// Begin render pass
			mMainWindow->beginRendering();

			mRenderService->renderObjects(*mMainWindow, ortho_cam, canvas_components_to_render);
			
			mGuiService->draw();

			// End render pass
			mMainWindow->endRendering();

			// End recording
			mRenderService->endRecording();
		}
		
		for (auto canvasEntity : mVideoWallEntity->getChildren()) {
			canvasEntity->getComponent<RenderCanvasComponentInstance>().mIsControlViewDraw = true;
		}
		canvasGroupComponent->getSelected()->getComponent<RenderCanvasComponentInstance>().setFinalSampler(true);

		if (mRenderService->beginRecording(*mControlsWindow)) {
			// Begin render pass
			mControlsWindow->beginRendering();
			// render canvases
			if (canvasGroupComponent->mDrawBackdrop) {
				mRenderService->renderObjects(*mControlsWindow, ortho_cam, canvas_components_to_render);
			}
			// Render GUI elements
			mGuiService->draw();
			
			// End render pass
			mControlsWindow->endRendering();
			
			// End recording
			mRenderService->endRecording();
		}
		// Proceed to next frame
		mRenderService->endFrame();



	}
	

	void foglioApp::windowMessageReceived(WindowEventPtr windowEvent)
	{
		mRenderService->addEvent(std::move(windowEvent));
	}
	
	
	void foglioApp::inputMessageReceived(InputEventPtr inputEvent)
	{
		if (inputEvent->get_type().is_derived_from(RTTI_OF(nap::KeyPressEvent)))
		{
			// If we pressed escape, quit the loop
			nap::KeyPressEvent* press_event = static_cast<nap::KeyPressEvent*>(inputEvent.get());
			if (press_event->mKey == nap::EKeyCode::KEY_ESCAPE)
				quit();

			// f is pressed, toggle full-screen
			if (press_event->mKey == nap::EKeyCode::KEY_f && press_event->mWindow == mMainWindow->getNumber()) {
				if (!mFullscreen) {
					mMainWindow->mBorderless = !&mMainWindow->mBorderless;
					mMainWindow->setWidth(mRenderService->getDisplays()[0].getMax()[0]);
					mMainWindow->setHeight(mRenderService->getDisplays()[0].getMax()[1]);
					mMainWindow->setPosition(glm::vec2(0, 0));
					mFullscreen = true;
				}
				else {
					mMainWindow->mBorderless = !&mMainWindow->mBorderless;
					mMainWindow->setWidth(mRenderService->getDisplays()[0].getMax()[0]/2);
					mMainWindow->setHeight(mRenderService->getDisplays()[0].getMax()[1]/2);
					mMainWindow->setPosition(glm::vec2(mRenderService->getDisplays()[0].getMax()[0] / 4, mRenderService->getDisplays()[0].getMax()[1] / 4));
					mFullscreen = false;
				}
			}

			if (press_event->mKey == nap::EKeyCode::KEY_l && press_event->mWindow == mControlsWindow->getNumber()) {
				ResourcePtr<VideoPlayer> player = mScene->findEntity("BigCircleEntity")->findComponent<RenderCanvasComponentInstance>()->getVideoPlayer();
				nap::utility::ErrorState error;
				player->selectVideo((player->getIndex() + 1) % player->getCount(), error);
				player->play();
				player = mScene->findEntity("SmallCircle1Entity")->findComponent<RenderCanvasComponentInstance>()->getVideoPlayer();
				player->selectVideo((player->getIndex() + 1) % player->getCount(), error);
				player->play();
			}
		}
		// Add event, so it can be forwarded on update
		mInputService->addEvent(std::move(inputEvent));
	}

	
	int foglioApp::shutdown()
	{
		return 0;
	}

	// Draw some GUI elements
	void foglioApp::updateGUI()
	{
		mGuiService->selectWindow(mControlsWindow);
		#ifdef IMGUI_HAS_VIEWPORT
			ImGuiViewport* viewport = ImGui::GetMainViewport();
			ImGui::SetNextWindowPos(viewport->GetWorkPos());
			ImGui::SetNextWindowSize(viewport->GetWorkSize());
			ImGui::SetNextWindowViewport(viewport->ID);
		#else 
			ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
			ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
		#endif
		ImGui::Begin("Controls");
		ImGui::Text(getCurrentDateTime().toString().c_str());
		ImGui::Text(utility::stringFormat("Framerate: %.02f", getCore().getFramerate()).c_str());
		if (mVideoWallEntity->hasComponent<CanvasGroupComponentInstance>()) {
			mVideoWallEntity->getComponent<CanvasGroupComponentInstance>().drawOutliner();
		}
		
		//mVideoWallEntity->getComponent<CanvasGroupComponentInstance>().drawSequenceEditor();
		ImGui::End();
		
		

		mVideoWallEntity->getComponent<CanvasGroupComponentInstance>().drawSequenceEditor();
	}
}
