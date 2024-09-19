
#include "canvasgroupcomponent.h"
#include "rendercanvascomponent.h"
#include "canvaspasscomponent.h"
#include "inputcomponent.h"

#include <sequencecanvascomponent.h>
#include <midiinputcomponent.h>
#include <entity.h>
#include <nap/core.h>
#include <glm/gtc/type_ptr.hpp>
#include <imgui/imgui.h>
#include <imguiutils.h>
#include <midievent.h>

// nap::rendercanvascomponent run time class definition
RTTI_BEGIN_CLASS(nap::CanvasGroupComponent)
RTTI_PROPERTY("SequencePlayerEditor", &nap::CanvasGroupComponent::mSequencePlayerEditor, nap::rtti::EPropertyMetaData::Required)
RTTI_PROPERTY("SequencePlayerEditorGUI", &nap::CanvasGroupComponent::mSequencePlayerEditorGUI, nap::rtti::EPropertyMetaData::Required)
RTTI_PROPERTY("Presentation Window", &nap::CanvasGroupComponent::mPresentationWindow, nap::rtti::EPropertyMetaData::Required)
RTTI_END_CLASS

RTTI_BEGIN_CLASS_NO_DEFAULT_CONSTRUCTOR(nap::CanvasGroupComponentInstance)
	RTTI_CONSTRUCTOR(nap::EntityInstance&, nap::Component&)
RTTI_END_CLASS

namespace nap
{
	void CanvasGroupComponent::getDependentComponents(std::vector<rtti::TypeInfo>& components) const
	{
		components.emplace_back(RTTI_OF(nap::MidiInputComponent));

	}

	
	CanvasGroupComponentInstance::CanvasGroupComponentInstance(EntityInstance& entity, Component& resource) :
		InputComponentInstance(entity, resource)
		{ }

	

	bool CanvasGroupComponentInstance::init(utility::ErrorState& errorState)
	{
		if (!errorState.check(InputComponentInstance::init(errorState), "unable to init canvas group component: %s", getEntityInstance()->mID.c_str()))
			return false;
		// Get resource
		CanvasGroupComponent* resource = getComponent<CanvasGroupComponent>();
		mPresentationWindow = resource->mPresentationWindow;
		mSelectedCanvas = getEntityInstance()->getChildren()[0];
		mSelectedCanvasPass = mSelectedCanvas->getComponent<CanvasPassComponentInstance*>();
		if (!initSelectedRenderTarget()) {
			return false;
		}

		//sequencer stuff
		mSequenceEditor = resource->mSequencePlayerEditor.get();
		if (!errorState.check(mSequenceEditor->init(errorState), "%s: unable to init sequence editor", resource->mID.c_str()))
			return false;
		mSequenceEditorGUI = resource->mSequencePlayerEditorGUI.get();
		if (!errorState.check(mSequenceEditorGUI->init(errorState), "%s: unable to init sequence editor GUI", resource->mID.c_str()))
			return false;
		//setSequencePlayer();

		//MIDI stuff
		MidiInputComponentInstance* midi_input = getEntityInstance()->findComponent<MidiInputComponentInstance>();
		if (errorState.check(midi_input != nullptr, "%s: missing MidiInputComponent", mID.c_str())) {
			midi_input->messageReceived.connect(midiEventReceivedSlot);
			mMidiData = std::make_unique<MidiData>(MidiData());
		}
		return true;

	}

	void CanvasGroupComponentInstance::trigger(const nap::InputEvent& inEvent) {
		float stepSize = mKeyboardControlStepSize;
		RenderCanvasComponentInstance& canvas_comp = mSelectedCanvas->getComponent<RenderCanvasComponentInstance>();
		TransformComponentInstance& canvas_transform_comp = mSelectedCanvas->getComponent<TransformComponentInstance>();
		rtti::TypeInfo event_type = inEvent.get_type().get_raw_type();
		if (event_type == RTTI_OF(KeyPressEvent)){
			const KeyPressEvent& press_event = static_cast<const KeyPressEvent&>(inEvent);
			if (press_event.mKey == nap::EKeyCode::KEY_TAB) {
				int findIndex = 0;
				EntityInstance::ChildList canvasGroupChildren = getEntityInstance()->getChildren();
				for (EntityInstance* canvasEntity : canvasGroupChildren) {
					if (canvasEntity == mSelectedCanvas) {
						mSelectedCanvas = canvasGroupChildren[(findIndex + 1) % canvasGroupChildren.size()]; //findIndex + 1 because we skip to next one because of tab press
						mSelectedCanvasPass = mSelectedCanvas->getComponent<CanvasPassComponentInstance*>();
						break;
					}
					findIndex += 1;
				}
			}
			if (press_event.mKey == nap::EKeyCode::KEY_g) {
				nap::Logger::info("Switched canvas keyboard controls to MOVE");
				mCurrentKeyboardControlMode = KEYBOARD_CANVAS_CONTROL::TRANSLATE;
			}
			else if (press_event.mKey == nap::EKeyCode::KEY_s) {
				nap::Logger::info("Switched canvas keyboard controls to SCALE");
				mCurrentKeyboardControlMode = KEYBOARD_CANVAS_CONTROL::SCALE;
			}
			else if (press_event.mKey == nap::EKeyCode::KEY_c) {
				nap::Logger::info("Switched canvas keyboard controls to CORNER");
				mCurrentKeyboardControlMode = KEYBOARD_CANVAS_CONTROL::CORNER;
			}
			else if (mCurrentKeyboardControlMode == KEYBOARD_CANVAS_CONTROL::SCALE) {
				glm::vec3 scaleCurrent = canvas_transform_comp.getScale();
				if (press_event.mKey == nap::EKeyCode::KEY_LEFT) {
					scaleCurrent.x -= stepSize;
				}
				else if (press_event.mKey == nap::EKeyCode::KEY_RIGHT) {
					scaleCurrent.x += stepSize;
				}
				else if (press_event.mKey == nap::EKeyCode::KEY_UP) {
					scaleCurrent.y += stepSize;
				}
				else if (press_event.mKey == nap::EKeyCode::KEY_DOWN) {
					scaleCurrent.y -= stepSize;
				}
				canvas_transform_comp.setScale(scaleCurrent);
			}
			else if (mCurrentKeyboardControlMode == KEYBOARD_CANVAS_CONTROL::TRANSLATE) {
				glm::vec3 translateCurrent = canvas_transform_comp.getTranslate();
				if (press_event.mKey == nap::EKeyCode::KEY_LEFT) {
					translateCurrent.x -= stepSize;
				}
				else if (press_event.mKey == nap::EKeyCode::KEY_RIGHT) {
					translateCurrent.x += stepSize;
				}
				else if (press_event.mKey == nap::EKeyCode::KEY_UP) {
					translateCurrent.y += stepSize;
				}
				else if (press_event.mKey == nap::EKeyCode::KEY_DOWN) {
					translateCurrent.y -= stepSize;
				}
				canvas_transform_comp.setTranslate(translateCurrent);
			}
			else if (mCurrentKeyboardControlMode == KEYBOARD_CANVAS_CONTROL::CORNER) {
				if (press_event.mKey == nap::EKeyCode::KEY_1) {
					mCurrentCanvasCornerKeyboardControl = 0;
				}
				else if (press_event.mKey == nap::EKeyCode::KEY_2) {
					mCurrentCanvasCornerKeyboardControl = 1;
				}
				else if (press_event.mKey == nap::EKeyCode::KEY_3) {
					mCurrentCanvasCornerKeyboardControl = 2;
				}
				else if (press_event.mKey == nap::EKeyCode::KEY_4) {
					mCurrentCanvasCornerKeyboardControl = 3;
				}

				else if (press_event.mKey == nap::EKeyCode::KEY_LEFT) {
					std::vector<glm::vec2> offsets = canvas_comp.getCornerOffsets();
					offsets[mCurrentCanvasCornerKeyboardControl].x -= stepSize;
					canvas_comp.setCornerOffsets(offsets);
				}
				else if (press_event.mKey == nap::EKeyCode::KEY_RIGHT) {
					std::vector<glm::vec2> offsets = canvas_comp.getCornerOffsets();
					offsets[mCurrentCanvasCornerKeyboardControl].x += stepSize;
					canvas_comp.setCornerOffsets(offsets);
				}
				else if (press_event.mKey == nap::EKeyCode::KEY_UP) {
					std::vector<glm::vec2> offsets = canvas_comp.getCornerOffsets();
					offsets[mCurrentCanvasCornerKeyboardControl].y -= stepSize;
					canvas_comp.setCornerOffsets(offsets);
				}
				else if (press_event.mKey == nap::EKeyCode::KEY_DOWN) {
					std::vector<glm::vec2> offsets = canvas_comp.getCornerOffsets();
					offsets[mCurrentCanvasCornerKeyboardControl].y += stepSize;
					canvas_comp.setCornerOffsets(offsets);
				}

			}
		}
		std::vector<glm::i16vec2> corners = calculateScreenSpacePosition(mSelectedCanvas);
		// Ensure it's a pointer event
		if (event_type == RTTI_OF(PointerPressEvent))
		{
			const PointerPressEvent& press_event = static_cast<const PointerPressEvent&>(inEvent);
			nap::Logger::info("press event!!! upperLeft.x %i, upperLeft.y %i", corners[0].x, corners[0].y);
		}
		else if (event_type == RTTI_OF(PointerMoveEvent))
		{
			const PointerMoveEvent& move_event = static_cast<const PointerMoveEvent&>(inEvent);
			
			nap::Logger::info("move event!!!" + std::to_string(abs(corners[0].x - move_event.mX)));
			if (abs(corners[0].x - move_event.mX) < 10 && abs(corners[0].y - move_event.mY) < 10) {
				nap::Logger::info("touching upper left corner");
			}
		}
	}

	//UNFINISHED
	std::vector<glm::i16vec2> CanvasGroupComponentInstance::calculateScreenSpacePosition(EntityInstance* entity) {
		std::vector<glm::i16vec2> corners = std::vector<glm::i16vec2>(4);

		glm::vec3				translate = entity->getComponent<TransformComponentInstance>().getTranslate();
		glm::vec3				scale = entity->getComponent<TransformComponentInstance>().getScale();
		glm::quat				rotate = entity->getComponent<TransformComponentInstance>().getRotate();
		std::vector<glm::vec2>	cornerOffsets = entity->getComponent<RenderCanvasComponentInstance>().getCornerOffsets();
		//canvasSize and windowSize are in pixels
		glm::ivec2 canvasSize = entity->getComponent<RenderCanvasComponentInstance>().getFinalOutputTexture()->getSize();
		glm::ivec2 windowSize = entity->getCore()->getResourceManager()->findObject<RenderWindow>("ControlsWindow")->getSize();
		//scale canvasSize values to window
		if (canvasSize.x > canvasSize.y) {
			canvasSize = {windowSize.x, windowSize.x * canvasSize.y / canvasSize.x};
		}
		else {
			canvasSize = { windowSize.y * canvasSize.x / canvasSize.y, windowSize.y };
		}

		corners[0] = glm::i16vec2(((windowSize.x - canvasSize.x * scale.x) / 2 + windowSize.x * translate.x) + cornerOffsets[0].x * canvasSize.x, ((canvasSize.y * scale.y + windowSize.y) / 2 + windowSize.y * translate.y) - cornerOffsets[0].y * canvasSize.y);
		corners[1] = glm::i16vec2(((windowSize.x + canvasSize.x * scale.x) / 2 + windowSize.x * translate.x) - cornerOffsets[1].x * canvasSize.x, ((canvasSize.y * scale.y + windowSize.y) / 2 + windowSize.y * translate.y) - cornerOffsets[1].y * canvasSize.y);
		corners[2] = glm::i16vec2(((windowSize.x - canvasSize.x * scale.x) / 2 + windowSize.x * translate.x) + cornerOffsets[2].x * canvasSize.x, ((windowSize.y - canvasSize.y * scale.y) / 2 + windowSize.y * translate.y) + cornerOffsets[2].y * canvasSize.y);
		corners[3] = glm::i16vec2(((windowSize.x + canvasSize.x * scale.x) / 2 + windowSize.x * translate.x) - cornerOffsets[3].x * canvasSize.x, ((windowSize.y - canvasSize.y * scale.y) / 2 + windowSize.y * translate.y) + cornerOffsets[3].y * canvasSize.y);
		return corners;
	}

	void CanvasGroupComponentInstance::drawSequenceEditor() {
		mSequenceEditorGUI->show();
	}

	void CanvasGroupComponentInstance::setSequencePlayer() {
		if (mSelectedCanvas->hasComponent<SequenceCanvasComponent>()) {
			mSequenceEditor->mSequencePlayer = mSelectedCanvas->getComponent<SequenceCanvasComponentInstance>().mSequencePlayer;
			nap::utility::ErrorState error;
			mSequenceEditor->init(error);
			mSequenceEditorGUI->init(error);
		}
	}

	void CanvasGroupComponentInstance::setSelectedTextureControlOverlay(bool isControlWindowDraw) {
		auto& canvasComp = mSelectedCanvas->getComponent<RenderCanvasComponentInstance>();
		if (isControlWindowDraw) {
			canvasComp.setFinalSamplerTexture(mSelectedOverlayRenderTarget->mColorTexture.get());
		}
		else {
			canvasComp.setFinalSamplerTexture(canvasComp.getFinalOutputTexture().get());
		}
		
	}

	bool CanvasGroupComponentInstance::initSelectedRenderTarget()
	{
		mSelectedOverlayTexture = getEntityInstance()->getCore()->getResourceManager()->createObject<RenderTexture2D>();
		ResourcePtr<RenderTexture2D> outputTexRef = mSelectedCanvas->getComponent<RenderCanvasComponentInstance>().getFinalOutputTexture();
		mSelectedOverlayTexture->mWidth = outputTexRef->mWidth;
		mSelectedOverlayTexture->mHeight = outputTexRef->mHeight;
		mSelectedOverlayTexture->mColorFormat = outputTexRef->mColorFormat;
		mSelectedOverlayTexture->mUsage = nap::Texture::EUsage::Static;
		nap::utility::ErrorState error;
		if (!mSelectedOverlayTexture->init(error))
		{
			error.fail("%s: Failed to initialize selected output texture", mSelectedOverlayTexture->mID.c_str());
			return false;
		}
		mSelectedOverlayRenderTarget = getEntityInstance()->getCore()->getResourceManager()->createObject<RenderTarget>();
		mSelectedOverlayRenderTarget->mColorTexture = mSelectedOverlayTexture;
		mSelectedOverlayRenderTarget->mClearColor = RGBAColor8(255, 255, 255, 0).convert<RGBAColorFloat>();
		mSelectedOverlayRenderTarget->mSampleShading = false;
		mSelectedOverlayRenderTarget->mRequestedSamples = ERasterizationSamples::One;
		if (!mSelectedOverlayRenderTarget->init(error))
		{
			error.fail("%s: Failed to initialize internal render target for selected canvas overlay", mSelectedOverlayRenderTarget->mID.c_str());
			return false;
		}
	}

	void CanvasGroupComponentInstance::onMidiEventReceived(const MidiEvent& inEvent) {
		mMidiData->mReceivedEvents.emplace_back(inEvent.toString());
		if (mMidiData->mReceivedEvents.size() > 25)
		{
			mMidiData->mReceivedEvents.erase(mMidiData->mReceivedEvents.begin());
		}
		//pass to all canvas pass components that enabled midi events
		for (auto group_child : getEntityInstance()->getChildren())
		{
			std::vector<CanvasPassComponentInstance*> canvas_passes;
			group_child->getComponentsOfType(canvas_passes);
			for (CanvasPassComponentInstance* canvas_pass : canvas_passes) {

			}
			RenderCanvasComponentInstance& canvas_comp = group_child->getComponent<RenderCanvasComponentInstance>();
			if (canvas_comp.mCustomPostPass != nullptr) {
				UniformStructInstance* ubo = canvas_comp.mCustomPostPass->mUBO;
				if (inEvent.getChannel() == 0 && inEvent.getType() == MidiEvent::Type::controlChange) {
					UniformFloatInstance* uniform = ubo->findUniform<UniformFloatInstance>("midiKnob" + std::to_string(inEvent.getCCNumber() - 1));
					if (uniform != nullptr) {
						uniform->setValue(inEvent.getCCValue() / 128.0);
					}
				}
				else if (inEvent.getType() == MidiEvent::Type::pitchBend) {
					nap::Logger::info("pitch bend value: %f", inEvent.getPitchBendValue());
					UniformFloatInstance* uniform = ubo->findUniform<UniformFloatInstance>("midiPitchBend");
					if (uniform != nullptr) {
						uniform->setValue(inEvent.getPitchBendValue());
					}
					mMidiData->pitch = inEvent.getPitchBendValue();
				}
			}
		}
		
	}

	void CanvasGroupComponentInstance::handleTimeDependentAction(double deltatime) {
		if (mMidiData != nullptr) {
			mMidiData->pitchAccumulative = mMidiData->pitchAccumulative + mMidiData->pitch * deltatime;
			//pass to all canvas components
			for (auto group_child : getEntityInstance()->getChildren())
			{
				RenderCanvasComponentInstance& canvas_comp = group_child->getComponent<RenderCanvasComponentInstance>();
				if (canvas_comp.mCustomPostPass != nullptr) {
					UniformStructInstance* ubo = canvas_comp.mCustomPostPass->mUBO;
					ubo->findUniform<UniformFloatInstance>("midiPitchBendAcc")->setValue(mMidiData->pitchAccumulative);
				}
			}
		}
	}

	void CanvasGroupComponentInstance::drawAllHeadless()
	{
		std::vector<EntityInstance*> mCanvasEntities = getEntityInstance()->getChildren(); //TODO: maybe set in init() and update() when entityinstance children update call?
		for (EntityInstance* canvasEntity : mCanvasEntities) {
			canvasEntity->getComponent<RenderCanvasComponentInstance>().renderPasses();
		}
	}

	void CanvasGroupComponentInstance::drawSelectedInterface()
	{
		if (mSelectedCanvas != nullptr) {
			mSelectedCanvas->getComponent<RenderCanvasComponentInstance>().drawInterface(mSelectedOverlayRenderTarget);
		}
	}

	void CanvasGroupComponentInstance::selectCanvas(EntityInstance* newSelectedCanvas) {
		mSelectedCanvas = newSelectedCanvas;
		initSelectedRenderTarget();
	}

	void CanvasGroupComponentInstance::drawOutliner() {
		if (ImGui::Button("Toggle Backdrop")) {
			mDrawBackdrop = !mDrawBackdrop;
		}
		if (ImGui::RadioButton("T", mCurrentKeyboardControlMode == KEYBOARD_CANVAS_CONTROL::TRANSLATE)) {
			mCurrentKeyboardControlMode = KEYBOARD_CANVAS_CONTROL::TRANSLATE;
			nap::Logger::info("mCurrentKeyboardControlMode Set to translate from gui");
		};
		ImGui::SameLine();
		if (ImGui::RadioButton("S", mCurrentKeyboardControlMode == KEYBOARD_CANVAS_CONTROL::SCALE)) {
			mCurrentKeyboardControlMode = KEYBOARD_CANVAS_CONTROL::SCALE;
			nap::Logger::info("mCurrentKeyboardControlMode Set to translate from gui");
		};
		ImGui::SameLine();
		if (ImGui::RadioButton("C", mCurrentKeyboardControlMode == KEYBOARD_CANVAS_CONTROL::CORNER)) {
			mCurrentKeyboardControlMode = KEYBOARD_CANVAS_CONTROL::CORNER;
			nap::Logger::info("mCurrentKeyboardControlMode Set to corner from gui");
		};
		ImGui::SameLine();
		ImGui::DragFloat("Step Size", &mKeyboardControlStepSize, 0.01f, 0.f, 10.f, "%.2f", 1.f);
		for (EntityInstance* canvasEntity : getEntityInstance()->getChildren()) {
			ImGuiTreeNodeFlags node_flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
			if (mSelectedCanvas == canvasEntity) {
				node_flags |= ImGuiTreeNodeFlags_Selected;
			}
			ImGui::TreeNodeEx((EntityInstance*)canvasEntity, node_flags, canvasEntity->getEntity()->mID.c_str());
			if (ImGui::IsItemClicked())
			{
				mSelectedCanvas = canvasEntity;
				mSelectedCanvasPass = mSelectedCanvas->getComponent<CanvasPassComponentInstance*>();
				setSequencePlayer();
			}		
		}
		RenderCanvasComponentInstance& canvas_comp = mSelectedCanvas->getComponent<RenderCanvasComponentInstance>();
		TransformComponentInstance& canvas_transform_comp = mSelectedCanvas->getComponent<TransformComponentInstance>();
		
		ResourcePtr<RenderTexture2D> canvas_tex = canvas_comp.getFinalOutputTexture();
		float col_width = ImGui::GetContentRegionAvailWidth();
		float ratio_canvas_tex = static_cast<float>(canvas_tex->getWidth()) / static_cast<float>(canvas_tex->getHeight());
		if (ImGui::CollapsingHeader("Preview##final", ImGuiTreeNodeFlags_None))
		{
			ImGui::Image(*canvas_tex.get(), {col_width , col_width / ratio_canvas_tex});
		}
		utility::ErrorState errorState;
		/*
		if (canvas_comp.getVideoPlayer() != nullptr) {
			VideoPlayer* video_player = canvas_comp.getVideoPlayer();
			float current_time = canvas_comp.getVideoPlayer()->getCurrentTime();
			if (ImGui::SliderFloat("", &current_time, 0.0f, canvas_comp.getVideoPlayer()->getDuration(), "%.3fs", 1.0f))
				canvas_comp.getVideoPlayer()->seek(current_time);
			ImGui::Text("Total time: %fs", canvas_comp.getVideoPlayer()->getDuration());
			ImGui::BeginGroup();
			std::string mediaControlSymbol = video_player->isPlaying() ? "X" : "O";
			
			if (ImGui::ArrowButton("##left", ImGuiDir_Left)) {
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
			if (ImGui::Button(mediaControlSymbol.c_str())) {
				video_player->isPlaying() ? video_player->stopPlayback() : video_player->play();
			}
			ImGui::SameLine();
			if (ImGui::ArrowButton("##right", ImGuiDir_Right)) {
				video_player->selectVideo((video_player->getIndex() + 1) % video_player->getCount(), errorState);
				video_player->play();
			}
			ImGui::EndGroup();

		}
		*/
		ImGui::Text("Position");
		glm::vec3 translate = canvas_transform_comp.getTranslate();
		float tempXTransl = translate.x;
		float tempYTransl = translate.y;
		ImGui::DragFloat("X##position", &tempXTransl, 0.01f, -1.0f, 1.0f, "%.3f", 1.0f);
		ImGui::DragFloat("Y##position", &tempYTransl, 0.01f, -1.0f, 1.0f, "%.3f", 1.0f);
		if (translate.x != tempXTransl || translate.y != tempYTransl) {
			canvas_transform_comp.setTranslate(glm::vec3(tempXTransl, tempYTransl, translate.z));
		}
		ImGui::Text("Scale");
		glm::vec3 scale = canvas_transform_comp.getScale();
		float tempXScale = scale.x;
		float tempYScale = scale.y;
		ImGui::DragFloat("X##scale", &tempXScale, 0.01f, 0.0f, 1.0f, "%.3f", 1.0f);
		ImGui::DragFloat("Y##scale", &tempYScale, 0.01f, 0.0f, 1.0f, "%.3f", 1.0f);
		if (scale.x != tempXScale || scale.y != tempYScale) {
			canvas_transform_comp.setScale(glm::vec3(tempXScale, tempYScale, scale.z));
		}
		ImGui::Text("Rotation");
		glm::quat rotation = canvas_transform_comp.getRotate();
		float tempRot = rotation.z;
		ImGui::DragFloat("Degrees", &tempRot, 0.01f, 0.0f, 360.0f, "%.3f", 1.0f);
		if (rotation.z != tempRot) {
			canvas_transform_comp.setRotate(glm::quat(rotation.x, rotation.y, 1.0, rotation.w));
		}
		if (ImGui::CollapsingHeader("Corner Offsets", ImGuiTreeNodeFlags_None))
		{
			std::vector<glm::vec2> offsets = canvas_comp.getCornerOffsets();
			ImGui::DragFloat("Top Left X", &offsets[0].x, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Top Left Y", &offsets[0].y, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Top Right X", &offsets[1].x, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Top Right Y", &offsets[1].y, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Bottom Left X", &offsets[2].x, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Bottom Left Y", &offsets[2].y, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Bottom Right X", &offsets[3].x, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Bottom Right Y", &offsets[3].y, 0.01f, 0.0f, 1.0f);
			if (offsets != canvas_comp.getCornerOffsets()) {
				canvas_comp.setCornerOffsets(offsets);
			}
		}
		std::vector<CanvasPassComponentInstance*> passes;
		mSelectedCanvas->getComponentsOfType(passes);
		if (passes.size() > 0) {
			for (CanvasPassComponentInstance* pass : passes) {
				ImGuiTreeNodeFlags node_flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
				if (mSelectedCanvasPass == pass) {
					node_flags |= ImGuiTreeNodeFlags_Selected;
				}
				ImGui::TreeNodeEx((EntityInstance*)pass, node_flags, pass->mID.c_str());
				if (ImGui::IsItemClicked())
				{
					mSelectedCanvasPass = pass;
				}
			}
			ImGui::Text("%s: Overview", mSelectedCanvasPass->mID.c_str());
			if (ImGui::CollapsingHeader("Preview##pass", ImGuiTreeNodeFlags_None))
			{
				ImGui::Image(*mSelectedCanvasPass->mFinalTexture, { col_width , col_width / ratio_canvas_tex });
			}
		}
		else {
			ImGui::Text("No passes");
		}
		
		
		if (mSelectedCanvas->hasComponent<SequenceCanvasComponentInstance>()) {
			SequenceCanvasComponentInstance& seq_canvas_comp = mSelectedCanvas->getComponent<SequenceCanvasComponentInstance>();
			ResourcePtr<SequencePlayer> seq_player = seq_canvas_comp.getSequencePlayer();
			ImGui::Text("Sequence %s", seq_player->getSequenceFilename());
			float playbackSpeed = seq_player->getPlaybackSpeed();
			float tempPlaybackSpeed = playbackSpeed;
			ImGui::DragFloat("Sequence Speed", &playbackSpeed, 0.01f, 0.0f, 100.0f);
			if (tempPlaybackSpeed != playbackSpeed) {
				nap::Logger::info("set playback speed for sequence");
				seq_player->setPlaybackSpeed(playbackSpeed);
			}
		}
		/***
		if (canvas_comp.mCustomPostPass != nullptr) {
			UniformStructInstance* ubo = canvas_comp.mCustomPostPass->mUBO;
			ImGui::Text("Custom Post Pass");
			float tempPowerTo = ubo->findUniform<UniformFloatInstance>("power_to")->getValue();
			ImGui::DragFloat("Power to", &tempPowerTo, 0.01f, 0.0f, 1.0f);
			if (tempPowerTo != ubo->findUniform<UniformFloatInstance>("power_to")->getValue()) {
				ubo->findUniform<UniformFloatInstance>("power_to")->setValue(tempPowerTo);
			}
		}
		***/
		
	}

	void CanvasGroupComponentInstance::drawMidiInformation()
	{
		if (mMidiData == nullptr)
			return;

		ImGui::Begin("MIDI and OSC info");

		ImGui::Text("Pitch accumulated: %f", mMidiData->pitchAccumulative);
		// Get all received osc messages and convert into a single string
		std::string msg;
		for (const auto& message : mMidiData->mReceivedEvents)
			msg += (message + "\n");

		// Backup text
		char txt[256] = "No Midi Messages Received";

		// If there are no messages display that instead of the received messages
		char* display_msg = msg.empty() ? txt : &msg[0];
		size_t display_size = msg.empty() ? 256 : msg.size();

		// Display block of text
		ImGui::InputTextMultiline("Midi Messages", display_msg, display_size, ImVec2(-1.0f, ImGui::GetTextLineHeight() * 15), ImGuiInputTextFlags_ReadOnly);
	

		ImGui::End();
	}

	ResourcePtr<RenderWindow> CanvasGroupComponentInstance::getPresentationWindow() {
		return mPresentationWindow;
	}

}