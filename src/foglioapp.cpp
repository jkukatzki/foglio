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
#include <midiinputcomponent.h>



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
		if (mVideoWallEntity->hasComponent<CanvasGroupComponentInstance>()) {
			mPresentationWindow = mVideoWallEntity->findComponent<CanvasGroupComponentInstance>()->getPresentationWindow();
		}
		else {
			nap::Logger::error("No canvas group component");
		}
		//set second monitor as main display
		DisplayList displays = mRenderService->getDisplays();
		if (displays.size() < 2) {
			mMainDisplay = new Display(displays[0]);
		}
		else {
			mMainDisplay = new Display(displays[1]);
		}

		// limit framerate
		setFramerate(30.0);
		capFramerate(true);

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
		CanvasGroupComponentInstance* canvasGroupComponent = &mVideoWallEntity->getComponent<CanvasGroupComponentInstance>();
		canvasGroupComponent->handleTimeDependentAction(deltaTime);
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
			mRenderService->endHeadlessRecording();
		}
		canvasGroupComponent->getSelected()->getComponent<RenderCanvasComponentInstance>().setFinalSampler(false);
		for (auto canvasEntity : mVideoWallEntity->getChildren()) {
			canvasEntity->getComponent<RenderCanvasComponentInstance>().mIsControlViewDraw = false;
		}
		
		if (mRenderService->beginRecording(*mPresentationWindow)) {
			// Begin render pass
			mMainWindow->beginRendering();

			mRenderService->renderObjects(*mPresentationWindow, ortho_cam, canvas_components_to_render);
			
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
			if (press_event->mKey == nap::EKeyCode::KEY_ESCAPE) {
				mQueuedExitDialog = true;
			}
				
			// f is pressed, toggle full-screen
			if (press_event->mKey == nap::EKeyCode::KEY_f) {
				if (mFullscreen) {
					mQueuedExitFullscreenDialog = true;
				}
				else {
					toggleFullscreen();
				}
			}

			if (press_event->mKey == nap::EKeyCode::KEY_l && press_event->mWindow == mControlsWindow->getNumber()) {
				ResourcePtr<VideoPlayer> player = mScene->findEntity("NameCardCanvasEntity")->findComponent<RenderCanvasComponentInstance>()->getVideoPlayer();
				nap::utility::ErrorState error;
				player->selectVideo((player->getIndex() + 1) % player->getCount(), error);
				player->play();
				player = mScene->findEntity("SoundBoxEntity")->findComponent<RenderCanvasComponentInstance>()->getVideoPlayer();
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
		
		ImVec2 center = ImVec2(ImGui::GetIO().DisplaySize.x / 2.0, ImGui::GetIO().DisplaySize.y / 2.0);
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		if (ImGui::BeginPopupModal("Exit?", NULL, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::Text("Exit?");
			ImGui::Separator();

			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
			ImGui::PopStyleVar();

			if (ImGui::Button("OK", ImVec2(120, 0))) {
				ImGui::CloseCurrentPopup();
				quit();
			}
			ImGui::SetItemDefaultFocus();
			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
			ImGui::EndPopup();
		}
		if (mQueuedExitDialog) {
			ImGui::OpenPopup("Exit?");
			mQueuedExitDialog = false;
		}
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		if (ImGui::BeginPopupModal("Exit fullscreen?", NULL, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::Text("Exit fullscreen?");
			ImGui::Separator();

			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
			ImGui::PopStyleVar();

			if (ImGui::Button("OK", ImVec2(120, 0))) {
				ImGui::CloseCurrentPopup();
				toggleFullscreen();
			}
			ImGui::SetItemDefaultFocus();
			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
			ImGui::EndPopup();
		}
		if (mQueuedExitFullscreenDialog) {
			ImGui::OpenPopup("Exit fullscreen?");
			mQueuedExitFullscreenDialog = false;
		}

		ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
		ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x / 2.0, ImGui::GetIO().DisplaySize.y * 0.75));
		
		ImGui::Begin("Outliner");
		
		if (mVideoWallEntity->hasComponent<CanvasGroupComponentInstance>()) {
			mVideoWallEntity->getComponent<CanvasGroupComponentInstance>().drawOutliner();
		}
		else {
			ImGui::Text("No CanvasGroupComponentInstance found");
		}
		
		ImGui::End();


		//general info window
		ImGui::SetNextWindowPos(ImVec2(0.0f, ImGui::GetIO().DisplaySize.y * 0.75));
		ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x / 2.0, ImGui::GetIO().DisplaySize.y * 0.25));
		ImGui::Begin("Info");
		ImGui::Text(getCurrentDateTime().toString().c_str());
		ImGui::Text(utility::stringFormat("Framerate: %.02f", getCore().getFramerate()).c_str());
		float requestedFramerateTemp = getRequestedFramerate();
		ImGui::DragFloat("Frame Rate Limit", &requestedFramerateTemp, 1.0f, 10.0, 300.0, "%.0f", 1.0);

		//display select // BROKEN: cannot switch back
		nap::DisplayList displays = mRenderService->getDisplays();
		if (ImGui::BeginCombo("Display##displaySelect", std::to_string(mMainDisplay->getIndex()).c_str()))
		{
			for (int n = 0; n <= IM_ARRAYSIZE(displays.data()); n++)
			{
				bool is_selected = (*mMainDisplay == displays[n]);
				if (ImGui::Selectable(("Monitor "+std::to_string(displays[n].getIndex())).c_str(), is_selected)) {
					delete mMainDisplay;
					mMainDisplay = new Display(displays.at(n));
					mFullscreen = false;
					toggleFullscreen();	
					if (is_selected)
						ImGui::SetItemDefaultFocus();
				}
				
			}
			ImGui::EndCombo();
		}
		if (ImGui::Button("Toggle fullscreen")) {
			toggleFullscreen();
		}

		if (requestedFramerateTemp != getRequestedFramerate()) {
			setFramerate(requestedFramerateTemp);
		}

		ImGui::End();

		//midi and osc info window
		ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x / 2.0, 0.0f));
		ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x / 2.0, ImGui::GetIO().DisplaySize.y));
		mVideoWallEntity->getComponent<CanvasGroupComponentInstance>().drawMidiInformation();
		
		mVideoWallEntity->getComponent<CanvasGroupComponentInstance>().drawSequenceEditor();
	}

	void foglioApp::toggleFullscreen() {
		if (!mFullscreen) {
			mMainWindow->mBorderless = !&mMainWindow->mBorderless;
			mMainWindow->setWidth(mMainDisplay->getBounds().getWidth());
			mMainWindow->setHeight(mMainDisplay->getBounds().getHeight());
			mMainWindow->setPosition(mMainDisplay->getBounds().getMin());
			mFullscreen = true;
		}
		else {
			mMainWindow->mBorderless = !&mMainWindow->mBorderless;
			mMainWindow->setWidth(mMainDisplay->getMax()[0] / 2);
			mMainWindow->setHeight(mMainDisplay->getMax()[1] / 2);
			mMainWindow->setPosition(glm::vec2(mMainDisplay->getMax()[0] / 4, mMainDisplay->getMax()[1] / 4));
			mFullscreen = false;
		}
	}
}
