#pragma once

#include "rendercanvascomponent.h"

#include <component.h>
#include <inputcomponent.h>
#include <componentptr.h>
#include <sequenceeditorgui.h>
#include <sequenceeditor.h>
#include <sequence.h>
#include <sequenceevent.h>
#include <renderservice.h>
#include <midievent.h>
#include <rendertarget.h>
#include <rendertexture2d.h>
#include <entity.h>


namespace nap
{
	// Forward declares
	class CanvasGroupComponentInstance;

	//handles pointer input events to edit canvases
	class NAPAPI CanvasGroupComponent : public InputComponent
	{
		RTTI_ENABLE(InputComponent)
		DECLARE_COMPONENT(CanvasGroupComponent, CanvasGroupComponentInstance)

	public:
		ResourcePtr<SequenceEditor> mSequencePlayerEditor = nullptr;
		ResourcePtr<SequenceEditorGUI>	mSequencePlayerEditorGUI = nullptr;
		ResourcePtr<RenderWindow> mPresentationWindow = nullptr;

		virtual void getDependentComponents(std::vector<rtti::TypeInfo>& components) const override;

	};

	class NAPAPI CanvasGroupComponentInstance : public InputComponentInstance
	{
		RTTI_ENABLE(InputComponentInstance)
	public:
		CanvasGroupComponentInstance(EntityInstance& entity, Component& resource);

		virtual bool init(utility::ErrorState& errorState) override;

		void drawAllHeadless();

		void drawSelectedInterface();

		void setSelectedTextureControlOverlay(bool isControlWindowDraw);

		void drawOutliner();

		void drawMidiInformation();

		void drawSequenceEditor();

		void setSequencePlayer(); // for editor gui

		bool initSelectedRenderTarget();

		void handleTimeDependentAction(double deltaTime);

		ResourcePtr<RenderWindow> getPresentationWindow();

		EntityInstance* getSelected() { return mSelectedCanvas; }

		ResourcePtr<RenderTarget>					mSelectedOverlayRenderTarget;
		ResourcePtr<RenderTexture2D>				mSelectedOverlayTexture;
		bool										mDrawBackdrop = false;

		struct MidiData {
			float pitch = 0;
			float pitchAccumulative = 0;
			std::vector<std::string> mReceivedEvents;
		};



	protected:
		virtual void trigger(const nap::InputEvent& inEvent) override;

	private:
		RenderService* mRenderService = nullptr;
		ResourcePtr<SequenceEditorGUI>				mSequenceEditorGUI = nullptr;
		ResourcePtr<SequenceEditor>					mSequenceEditor = nullptr;
		std::vector<RenderCanvasComponentInstance*> mCanvases;
		EntityInstance*								mSelectedCanvas = nullptr;
		CanvasPassComponentInstance*				mSelectedCanvasPass = nullptr;
		std::unique_ptr<MidiData>					mMidiData = nullptr;
		char*										mAvailableDisplays;
		int											mCurrentDisplayIndex;
		ResourcePtr<RenderWindow>					mPresentationWindow;

		enum KEYBOARD_CANVAS_CONTROL
		{
			TRANSLATE,
			SCALE,
			CORNER
		};

		KEYBOARD_CANVAS_CONTROL						mCurrentKeyboardControlMode;
		int											mCurrentCanvasCornerKeyboardControl;
		float										mKeyboardControlStepSize = 0.01f;


		Slot<const MidiEvent&> midiEventReceivedSlot = { this, &CanvasGroupComponentInstance::onMidiEventReceived };
		double currentTime = 0;

		/**
		 *	Called by the slot when a new midi event is received
		 */
		void onMidiEventReceived(const MidiEvent&);

		void selectCanvas(EntityInstance* newSelectedCanvas);

		std::vector<glm::i16vec2> calculateScreenSpacePosition(EntityInstance* entity);

	};
}