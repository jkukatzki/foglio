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
#include <fftaudionodecomponent.h>
#include <sequenceplayereventoutput.h>
#include <sequenceevent.h>
#include <midiinputcomponent.h>
#include <cmath>




RTTI_BEGIN_CLASS_NO_DEFAULT_CONSTRUCTOR(nap::foglioApp)
	RTTI_CONSTRUCTOR(nap::Core&)
RTTI_END_CLASS

static constexpr double plotDelta = 1.0 / 60.0;

namespace nap 
{
	float lerp(float start, float end, float t) {
		return start + t * (end - start);
	}
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
		mMainWindow1 = mResourceManager->findObject<nap::RenderWindow>("MainWindow");
		if (!error.check(mMainWindow1 != nullptr, "unable to find render window with name: %s", "MainWindow"))
			return false;
		mMainWindow2 = mResourceManager->findObject<nap::RenderWindow>("MainWindow2");
		if (!error.check(mMainWindow2 != nullptr, "unable to find render window with name: %s", "MainWindow"))
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
		mVideoWall1Entity = mScene->findEntity("VideoWallEntity");
		if (!error.check(mVideoWall1Entity != nullptr, "unable to find video wall entity with name: %s", "VideoWallEntity"))
			return false;
		if (mVideoWall1Entity->hasComponent<CanvasGroupComponentInstance>()) {
			mPresentationWindow1 = mVideoWall1Entity->findComponent<CanvasGroupComponentInstance>()->getPresentationWindow();
		}
		mVideoWall2Entity = mScene->findEntity("VideoWall2Entity");
		if (!error.check(mVideoWall2Entity != nullptr, "unable to find video wall entity with name: %s", "VideoWallEntity"))
			return false;
		if (mVideoWall2Entity->hasComponent<CanvasGroupComponentInstance>()) {
			mPresentationWindow2 = mVideoWall2Entity->findComponent<CanvasGroupComponentInstance>()->getPresentationWindow();
		}
		else {
			nap::Logger::error("No canvas group component");
		}
		mAudioEntity = mScene->findEntity("AudioEntity");
		if (!error.check(mAudioEntity != nullptr, "unable to find audio entity with name: %s", "AudioEntity"))
			return false;
		if (mAudioEntity->hasComponent<FFTAudioNodeComponentInstance>()) {
			fft_comp = &mAudioEntity->getComponent<FFTAudioNodeComponentInstance>();
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
		CanvasGroupComponentInstance* canvasGroupComponent = &mVideoWall1Entity->getComponent<CanvasGroupComponentInstance>();
		canvasGroupComponent->handleTimeDependentAction(deltaTime);
		const auto& amps = fft_comp->getFFTBuffer().getAmplitudeSpectrum();
		//smooth amps

		

		if (smoothedAmps.size() != amps.size()) {
			smoothedAmps.resize(amps.size());
		}
		for (int i = 0; i < amps.size(); i++) {
			smoothedAmps[i] = 0.0f;
			for (int j = -2; j < 3; j++) {
				if (i + j >= 0 && i + j < amps.size()) {
					smoothedAmps[i] += amps[i + j] / 5.0f;
				}
			}
		}
		if (smoothedAmps.size() != amps.size()) {
			smoothedAmps.resize(amps.size());
			smoothedAmps.assign(amps.size(), 0.0f);
		}
		for (int i = 0; i < amps.size(); i++) {
			smoothedAmps[i] += (amps[i] - smoothedAmps[i]) * deltaTime / mSpectrumSmoothAmount;
		}

		int start = spectrumCrop[0] * smoothedAmps.size();
		int end = spectrumCrop[1] * smoothedAmps.size();
		auto v = smoothedAmps;
		if (start < 0) start = 0;
		if (end > v.size()) end = v.size();
		if (start > end) start = end;

		// Create a new vector with the sliced elements
		croppedSmoothedAmps.assign(v.begin() + start, v.begin() + end);
		if (mTimer.getElapsedTime() > plotDelta)
		{
			mBassRangeSum = 0;
			mMidRangeSum = 0;
			mHighRangeSum = 0;
			const int sampleSize = rangeSampleSize;
			for (int i = 0; i < sampleSize; i++) {
				const int sampleIndexBass = static_cast<int>((mBassRange[0] + ((mBassRange[1] - mBassRange[0]) * i / (sampleSize - 1))) * croppedSmoothedAmps.size() - 1);
				const int sampleIndexMids = static_cast<int>((mMidsRange[0] + ((mMidsRange[1] - mMidsRange[0]) * i / (sampleSize - 1))) * croppedSmoothedAmps.size() - 1);
				const int sampleIndexHighs = static_cast<int>((mHighsRange[0] + ((mHighsRange[1] - mHighsRange[0]) * i / (sampleSize - 1))) * croppedSmoothedAmps.size() - 1);
				mBassRangeSum += croppedSmoothedAmps[sampleIndexBass];
				mMidRangeSum += croppedSmoothedAmps[sampleIndexMids];
				mHighRangeSum += croppedSmoothedAmps[sampleIndexHighs];
			}
			mBassRangeSum *= mMasterGain * mBassGain / sampleSize;
			mMidRangeSum *= mMasterGain * mMidsGain / sampleSize;
			mHighRangeSum *= mMasterGain * mHighsGain / sampleSize;

			mBassRangeSumTimeLerped = lerp(mBassRangeSumTimeLerped, mBassRangeSum, deltaTime * mRangeTimeLerpSmoothAmount);
			mMidRangeSumTimeLerped = lerp(mMidRangeSumTimeLerped, mMidRangeSum, deltaTime * mRangeTimeLerpSmoothAmount);
			mHighRangeSumTimeLerped = lerp(mHighRangeSumTimeLerped, mHighRangeSum, deltaTime * mRangeTimeLerpSmoothAmount);
			mPlotvaluesBass[mTickIdx] = mBassRangeSumTimeLerped;
			mPlotvaluesMids[mTickIdx] = mMidRangeSumTimeLerped;
			mPlotvaluesHighs[mTickIdx] = mHighRangeSumTimeLerped;
			if (++mTickIdx == mPlotvaluesBass.size())
				mTickIdx = 0;

			mTimer.reset();
		}


	}
	
	
	// Called when the window is going to render
	void foglioApp::render()
	{
		auto canvas_comp = mScene->findEntity("BackgroundCanvasEntity")->findComponent<RenderCanvasComponentInstance>();
		UniformStructInstance* ubo = canvas_comp->mCustomPostPass->mUBO;
		UniformFloatInstance* uniform = ubo->findUniform<UniformFloatInstance>("audio_bass");
		uniform->setValue(mBassRangeSumTimeLerped);
		uniform = ubo->findUniform<UniformFloatInstance>("audio_mids");
		uniform->setValue(mMidRangeSumTimeLerped);
		uniform = ubo->findUniform<UniformFloatInstance>("audio_highs");
		uniform->setValue(mHighRangeSumTimeLerped);
		canvas_comp = mScene->findEntity("Background2CanvasEntity")->findComponent<RenderCanvasComponentInstance>();
		ubo = canvas_comp->mCustomPostPass->mUBO;
		uniform = ubo->findUniform<UniformFloatInstance>("audio_bass");
		uniform->setValue(mBassRangeSumTimeLerped);
		uniform = ubo->findUniform<UniformFloatInstance>("audio_mids");
		uniform->setValue(mMidRangeSumTimeLerped);
		uniform = ubo->findUniform<UniformFloatInstance>("audio_highs");
		uniform->setValue(mHighRangeSumTimeLerped);
		// Signal the beginning of a new frame, allowing it to be recorded.
		// The system might wait until all commands that were previously associated with the new frame have been processed on the GPU.
		// Multiple frames are in flight at the same time, but if the graphics load is heavy the system might wait here to ensure resources are available.
		mRenderService->beginFrame();

		// Find the orthographic camera component
		nap::OrthoCameraComponentInstance& ortho_cam = mOrthoCameraEntity->getComponent<OrthoCameraComponentInstance>();
		//get canvases once again for the render service
		std::vector<nap::RenderableComponentInstance*> canvas_components_to_render;
		for (nap::EntityInstance* canvasEntity : mVideoWall1Entity->getChildren()) {
			canvas_components_to_render.emplace_back(&canvasEntity->getComponent<RenderableComponentInstance>());
		};

		CanvasGroupComponentInstance* canvasGroupComponent1 = &mVideoWall1Entity->getComponent<CanvasGroupComponentInstance>();
		CanvasGroupComponentInstance* canvasGroupComponent2 = &mVideoWall2Entity->getComponent<CanvasGroupComponentInstance>();
		// Start recording into the headless recording buffer.
		if (mRenderService->beginHeadlessRecording())
		{
			canvasGroupComponent1->drawAllHeadless();
			canvasGroupComponent1->drawSelectedInterface();
			canvasGroupComponent2->drawAllHeadless();
			canvasGroupComponent2->drawSelectedInterface();
			mRenderService->endHeadlessRecording();
		}
		canvasGroupComponent1->getSelected()->getComponent<RenderCanvasComponentInstance>().setFinalSampler(false);
		for (auto canvasEntity : mVideoWall1Entity->getChildren()) {
			canvasEntity->getComponent<RenderCanvasComponentInstance>().mIsControlViewDraw = false;
		}
		
		if (mRenderService->beginRecording(*mPresentationWindow1)) {
			// Begin render pass
			mMainWindow1->beginRendering();

			mRenderService->renderObjects(*mPresentationWindow1, ortho_cam, canvas_components_to_render);
			
			mGuiService->draw();

			// End render pass
			mMainWindow1->endRendering();

			// End recording
			mRenderService->endRecording();
		}
		canvas_components_to_render.clear();
		for (nap::EntityInstance* canvasEntity : mVideoWall2Entity->getChildren()) {
			canvas_components_to_render.emplace_back(&canvasEntity->getComponent<RenderableComponentInstance>());
		};
		if (mRenderService->beginRecording(*mPresentationWindow2)) {
			// Begin render pass
			mMainWindow2->beginRendering();

			mRenderService->renderObjects(*mPresentationWindow2, ortho_cam, canvas_components_to_render);

			mGuiService->draw();

			// End render pass
			mMainWindow2->endRendering();

			// End recording
			mRenderService->endRecording();
		}

		if (mRenderService->beginRecording(*mControlsWindow)) {
			// Begin render pass
			mControlsWindow->beginRendering();
			// render canvases
			if (canvasGroupComponent1->mDrawBackdrop) {
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
				currentCameraPath++;
				currentCameraPath = currentCameraPath % 3;
				auto canvas_comp = mScene->findEntity("BackgroundCanvasEntity")->findComponent<RenderCanvasComponentInstance>();
				UniformStructInstance* ubo = canvas_comp->mCustomPostPass->mUBO;
				UniformIntInstance* uniform = ubo->findUniform<UniformIntInstance>("cameraPath");
				uniform->setValue(currentCameraPath);
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
		
		if (mVideoWall1Entity->hasComponent<CanvasGroupComponentInstance>()) {
			mVideoWall1Entity->getComponent<CanvasGroupComponentInstance>().drawOutliner();
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
		mVideoWall1Entity->getComponent<CanvasGroupComponentInstance>().drawMidiInformation();
		const auto& amps = fft_comp->getFFTBuffer().getAmplitudeSpectrum();
		float bassRange[2] = { 0.0f, 0.3f };
		float midRange[2] = { 0.3f, 0.7f };
		float highRange[2] = { 0.7f, 1.0f };
		
		ImGui::Text(utility::stringFormat("Bass Value: %.02f", mBassRangeSumTimeLerped).c_str());
		ImGui::PlotHistogram("Bass", mPlotvaluesBass.data(), mPlotvaluesBass.size(), mTickIdx, nullptr, 0.0f, 1.0f, ImVec2(ImGui::GetColumnWidth(), 128));
		ImGui::Text(utility::stringFormat("Mids Value: %.02f", mMidRangeSumTimeLerped).c_str());
		ImGui::PlotHistogram("Mids", mPlotvaluesMids.data(), mPlotvaluesMids.size(), mTickIdx, nullptr, 0.0f, 1.0f, ImVec2(ImGui::GetColumnWidth(), 128));
		ImGui::Text(utility::stringFormat("Highs Value: %.02f", mHighRangeSumTimeLerped).c_str());
		ImGui::PlotHistogram("Highs", mPlotvaluesHighs.data(), mPlotvaluesHighs.size(), mTickIdx, nullptr, 0.0f, 1.0f, ImVec2(ImGui::GetColumnWidth(), 128));
		ImGui::SliderFloat2("Bass Range", mBassRange, 0.0f, 1.0f);
		ImGui::SliderFloat2("Mids Range", mMidsRange, 0.0f, 1.0f);
		ImGui::SliderFloat2("Highs Range", mHighsRange, 0.0f, 1.0f);
		ImGui::SliderFloat2("Spectrum Crop", spectrumCrop, 0.0f, 1.0f);
		ImGui::DragFloat("Range Time Lerp Smooth Amount", &mRangeTimeLerpSmoothAmount, 0.01f, 0.0f, 20.0f);
		ImGui::DragFloat("Spectrum Smooth Amount", &mSpectrumSmoothAmount, 0.01f, 0.0f, 10.0f);
		ImGui::DragInt("Range Sample Size", &rangeSampleSize, 1, 2, 32);
		

		ImGui::DragFloat("Bass Gain", &mBassGain, 0.01f, 0.0f, 10.0f);
		ImGui::DragFloat("Mids Gain", &mMidsGain, 0.01f, 0.0f, 10.0f);
		ImGui::DragFloat("Highs Gain", &mHighsGain, 0.01f, 0.0f, 10.0f);
		ImGui::DragFloat("Master Gain", &mMasterGain, 0.01f, 0.0f, 100.0f);

		

		ImGui::PlotLines("FFT Smoothed", croppedSmoothedAmps.data(), croppedSmoothedAmps.size(), 0);

		ImGui::PlotLines("FFT", amps.data(), spectrumCrop[1] * amps.size() - spectrumCrop[0] * amps.size(), spectrumCrop[0] * amps.size());


		ImGui::PlotLines("FFT", amps.data(), spectrumCrop[1] * amps.size() - spectrumCrop[0] * amps.size(), spectrumCrop[0]*amps.size());
		mVideoWall1Entity->getComponent<CanvasGroupComponentInstance>().drawSequenceEditor();
	}

	void foglioApp::toggleFullscreen() {
		if (!mFullscreen) {
			mMainWindow1->mBorderless = !&mMainWindow1->mBorderless;
			mMainWindow1->setWidth(mMainDisplay->getBounds().getWidth());
			mMainWindow1->setHeight(mMainDisplay->getBounds().getHeight());
			mMainWindow1->setPosition(mMainDisplay->getBounds().getMin());
			mMainWindow2->mBorderless = !&mMainWindow2->mBorderless;
			mFullscreen = true;
		}
		else {
			mMainWindow1->mBorderless = !&mMainWindow1->mBorderless;
			mMainWindow1->setWidth(mMainDisplay->getMax()[0] / 2);
			mMainWindow1->setHeight(mMainDisplay->getMax()[1] / 2);
			mMainWindow1->setPosition(glm::vec2(mMainDisplay->getMax()[0] / 4, mMainDisplay->getMax()[1] / 4));
			mMainWindow2->mBorderless = !&mMainWindow2->mBorderless;
			mMainWindow2->setWidth(mMainDisplay->getMax()[0] / 2);
			mMainWindow2->setHeight(mMainDisplay->getMax()[1] / 2);
			mMainWindow2->setPosition(glm::vec2(mMainDisplay->getMax()[0] / 4, mMainDisplay->getMax()[1] / 4));
			mFullscreen = false;
		}
	}
}
