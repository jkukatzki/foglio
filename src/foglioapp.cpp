#include "foglioapp.h"

// External Includes
#include <utility/fileutils.h>
#include <nap/logger.h>
#include <imgui/imgui.h>
#include <inputrouter.h>
#include <rendergnomoncomponent.h>
#include <rendercanvascomponent.h>
#include <foglioservice.h>
#include <perspcameracomponent.h>
#include <orthocameracomponent.h>
#include <imguiutils.h>

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
		mRenderService	= getCore().getService<nap::RenderService>();
		mSceneService	= getCore().getService<nap::SceneService>();
		mInputService	= getCore().getService<nap::InputService>();
		mGuiService		= getCore().getService<nap::IMGuiService>();

		// Fetch the resource manager
		mResourceManager = getCore().getResourceManager();

		// Get the render window
		mMainWindow = mResourceManager->findObject<nap::RenderWindow>("MainWindow");
		if (!error.check(mMainWindow != nullptr, "unable to find render window with name: %s", "MainWindow"))
			return false;
		// Get the render window
		mControlsWindow = mResourceManager->findObject<nap::RenderWindow>("ControlsWindow");
		if (!error.check(mControlsWindow != nullptr, "unable to find render window with name: %s", "ControlsWindow"))
			return false;
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
		for (EntityInstance* canvasEntity : mVideoWallEntity->getChildren()) {
			canvasEntity->getComponent<RenderCanvasComponentInstance>().getVideoPlayer()->play();
		}

		// All done!
		return true;
	}
	
	
	// Called when the window is updating
	void foglioApp::update(double deltaTime)
	{
		// Use a default input router to forward input events (recursively) to all input components in the default scene
		nap::DefaultInputRouter input_router(true);
		mInputService->processWindowEvents(*mMainWindow, input_router, { &mScene->getRootEntity() });

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
		if (mRenderService->beginRecording(*mMainWindow)) {
			// Begin render pass
			mMainWindow->beginRendering();
			
			// Find the video wall entity and draw all canvases with the orthographic camera
			std::vector<nap::RenderableComponentInstance*> canvas_components_to_render;
			for (nap::EntityInstance* canvasEntity : mVideoWallEntity->getChildren()) {
				canvas_components_to_render.emplace_back(&canvasEntity->getComponent<RenderCanvasComponentInstance>());
			}
			mRenderService->renderObjects(*mMainWindow, ortho_cam, canvas_components_to_render);

			// End render pass
			mMainWindow->endRendering();

			// End recording
			mRenderService->endRecording();
		}
	

		if (mRenderService->beginRecording(*mControlsWindow)) {
			// Begin render pass
			mControlsWindow->beginRendering();
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
			if (press_event->mKey == nap::EKeyCode::KEY_f)
				mMainWindow->toggleFullscreen();
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

		ImGui::Begin("Controls");
		ImGui::Text(getCurrentDateTime().toString().c_str());
		ImGui::Text(utility::stringFormat("Framerate: %.02f", getCore().getFramerate()).c_str());

		if (ImGui::CollapsingHeader("Canvas Overview"))
		{
			for (EntityInstance* canvasEntity : mVideoWallEntity->getChildren()) {
				ImGui::Text("canvas!");
				RenderCanvasComponentInstance& canvas_comp = canvasEntity->getComponent<RenderCanvasComponentInstance>();
				TransformComponentInstance& canvas_transform_comp = canvasEntity->getComponent<TransformComponentInstance>();
				Texture2D& canvas_tex = canvas_comp.getOutputTexture();
				float col_width = ImGui::GetContentRegionAvailWidth();
				float ratio_canvas_tex = static_cast<float>(canvas_tex.getWidth()) / static_cast<float>(canvas_tex.getHeight());
				ImGui::Image(canvas_tex, { col_width , col_width / ratio_canvas_tex });
				if (ImGui::CollapsingHeader("Playback"))
				{
					float current_time = canvas_comp.getVideoPlayer()->getCurrentTime();
					if (ImGui::SliderFloat("Current Time", &current_time, 0.0f, canvas_comp.getVideoPlayer()->getDuration(), "%.3fs", 1.0f))
						canvas_comp.getVideoPlayer()->seek(current_time);
					ImGui::Text("Total time: %fs", canvas_comp.getVideoPlayer()->getDuration());

					float current_pos_x = canvas_transform_comp.getTranslate().x;
					if (ImGui::SliderFloat("offset x"+*canvasEntity->mID.c_str(), &current_pos_x, 0.0f, 100, "%.3fs", 1.0f))
						canvas_transform_comp.setTranslate(glm::vec3(current_pos_x, 0.0f, 0.0f));
				}
			}
		}
		ImGui::End();
	}
}
