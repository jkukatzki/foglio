#pragma once


// Core includes
#include <nap/resourcemanager.h>
#include <nap/resourceptr.h>
#include <nap/group.h>

// Module includes
#include <sequenceeditorgui.h>
#include <sequenceplayereventoutput.h>
#include <sequenceevent.h>
#include <sequence.h>
#include <renderservice.h>
#include <imguiservice.h>
#include <sceneservice.h>
#include <inputservice.h>
#include <scene.h>
#include <renderwindow.h>
#include <entity.h>
#include <videoplayer.h>
#include <app.h>
#include <fftaudionodecomponent.h>

namespace nap
{
	using namespace rtti;

	/**
	 * Main application that is called from within the main loop
	 */
	class foglioApp : public App
	{
		RTTI_ENABLE(App)
	public:
		/**
		 * Constructor
		 * @param core instance of the NAP core system
		 */
		foglioApp(nap::Core& core) : App(core) { }
		
		/**
		 * Initialize all the services and app specific data structures
		 * @param error contains the error code when initialization fails
		 * @return if initialization succeeded
		*/
		bool init(utility::ErrorState& error) override;
		
		/**
		 * Update is called every frame, before render.
		 * @param deltaTime the time in seconds between calls
		 */
		void update(double deltaTime) override;

		/**
		 * Render is called after update. Use this call to render objects to a specific target
		 */
		void render() override;

		/**
		 * Called when the app receives a window message.
		 * @param windowEvent the window message that occurred
		 */
		void windowMessageReceived(WindowEventPtr windowEvent) override;
		
		/**
		 * Called when the app receives an input message (from a mouse, keyboard etc.)
		 * @param inputEvent the input event that occurred
		 */
		void inputMessageReceived(InputEventPtr inputEvent) override;
		
		/**
		 * Called when the app is shutting down after quit() has been invoked
		 * @return the application exit code, this is returned when the main loop is exited
		 */
		virtual int shutdown() override;

		void toggleFullscreen();

	private:
		ResourceManager*			mResourceManager = nullptr;		///< Manages all the loaded data
		std::string					mFilename = "";					///< The JSON file that is loaded on initialization
		RenderService*				mRenderService = nullptr;		///< Render Service that handles render calls
		SceneService*				mSceneService = nullptr;		///< Manages all the objects in the scene
		InputService*				mInputService = nullptr;		///< Input service for processing input
		IMGuiService*				mGuiService = nullptr;			///< Manages GUI related update / draw calls
		ObjectPtr<RenderWindow>		mMainWindow1 = nullptr;					///< Pointer to the main render window
		ObjectPtr<RenderWindow>		mMainWindow2 = nullptr;					///< Pointer to the main render window
		ObjectPtr<RenderWindow>		mControlsWindow = nullptr;					///< Pointer to the controls window	
		ObjectPtr<Scene>			mScene = nullptr;				///< Pointer to the main scene

		ResourcePtr<RenderWindow>	mPresentationWindow1 = nullptr;
		ResourcePtr<RenderWindow>	mPresentationWindow2 = nullptr;
		nap::Display*				mMainDisplay = nullptr;


		bool						mQueuedExitDialog = false;
		bool						mQueuedExitFullscreenDialog = false;

		std::array<audio::ControllerValue, 128> mPlotvaluesBass = {};
		std::array<audio::ControllerValue, 128> mPlotvaluesMids = {};
		std::array<audio::ControllerValue, 128> mPlotvaluesHighs = {};

		float mBassRange[2] = { 0.0f, 0.036f };
		float mMidsRange[2] = { 0.079f, 0.417f };
		float mHighsRange[2] = { 0.76f, 1.0f };
		float spectrumCrop[2] = { 0.0f, 0.2f };
		float mBassGain = 1.0f;
		float mMidsGain = 1.0f;
		float mHighsGain = 1.0f;
		float mMasterGain = 50.0f;
		float mBassRangeSum = 0.0f;
		float mBassRangeSumTimeLerped = 0.0f;
		float mMidRangeSum = 0.0f;
		float mMidRangeSumTimeLerped = 0.0f;
		float mHighRangeSum = 0.0f;
		float mHighRangeSumTimeLerped = 0.0f;
		float mRangeTimeLerpSmoothAmount = 4.5f;
		float mSpectrumSmoothAmount = 0.01f;
		int rangeSampleSize = 7;
		std::vector<float> smoothedAmps;
		std::vector<float> croppedSmoothedAmps;

		nap::SteadyTimer mTimer;
		uint32 mTickSum = 0;
		uint32 mTickIdx = 0;

		int currentCameraPath = 0;

		ObjectPtr<SequenceEditorGUI>mCanvasSequenceEditorGUI = nullptr;

		ObjectPtr<EntityInstance>	mCameraEntity = nullptr;		///< Pointer to the entity that holds the perspective camera
		ObjectPtr<EntityInstance>	mOrthoCameraEntity = nullptr;
		ObjectPtr<EntityInstance>	mAudioEntity = nullptr;		///< Pointer to the entity that holds the canvas
		ObjectPtr<EntityInstance>	mGnomonEntity = nullptr;		///< Pointer to the entity that can render the gnomon
		ObjectPtr<EntityInstance>	mVideoWall1Entity = nullptr;
		ObjectPtr<EntityInstance>	mVideoWall2Entity = nullptr;
		
		FFTAudioNodeComponentInstance* fft_comp = nullptr;

		bool						mFullscreen = false;
		/**
		 * Sets up the GUI every frame
		 */
		void updateGUI();
	};
}
