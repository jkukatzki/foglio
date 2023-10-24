
#include "canvasgroupcomponent.h"
#include "rendercanvascomponent.h"
#include "inputcomponent.h"

#include <sequencecanvascomponent.h>
#include <entity.h>
#include <nap/core.h>
#include <glm/gtc/type_ptr.hpp>
#include <imgui/imgui.h>
#include <imguiutils.h>

// nap::rendercanvascomponent run time class definition
RTTI_BEGIN_CLASS(nap::CanvasGroupComponent)
	RTTI_PROPERTY("SequencePlayerEditor", &nap::CanvasGroupComponent::mSequencePlayerEditor, nap::rtti::EPropertyMetaData::Required)
	RTTI_PROPERTY("SequencePlayerEditorGUI", &nap::CanvasGroupComponent::mSequencePlayerEditorGUI, nap::rtti::EPropertyMetaData::Required)
RTTI_END_CLASS

RTTI_BEGIN_CLASS_NO_DEFAULT_CONSTRUCTOR(nap::CanvasGroupComponentInstance)
	RTTI_CONSTRUCTOR(nap::EntityInstance&, nap::Component&)
RTTI_END_CLASS

namespace nap
{
	
	CanvasGroupComponentInstance::CanvasGroupComponentInstance(EntityInstance& entity, Component& resource) :
		InputComponentInstance(entity, resource)
		{ }

	

	bool CanvasGroupComponentInstance::init(utility::ErrorState& errorState)
	{
		if (!errorState.check(InputComponentInstance::init(errorState), "unable to init canvas group component: %s", getEntityInstance()->mID.c_str()))
			return false;
		// Get resource
		CanvasGroupComponent* resource = getComponent<CanvasGroupComponent>();
		
		mSelected = getEntityInstance()->getChildren()[0];
		if (!initSelectedRenderTarget()) {
			return false;
		}
		mSequenceEditor = resource->mSequencePlayerEditor.get();
		if (!errorState.check(mSequenceEditor->init(errorState), "%s: unable to init sequence editor", resource->mID.c_str()))
			return false;
		mSequenceEditorGUI = resource->mSequencePlayerEditorGUI.get();
		if (!errorState.check(mSequenceEditorGUI->init(errorState), "%s: unable to init sequence editor GUI", resource->mID.c_str()))
			return false;
		//setSequencePlayer();
	}

	void CanvasGroupComponentInstance::trigger(const nap::InputEvent& inEvent) {
		// Ensure it's a pointer event
		rtti::TypeInfo event_type = inEvent.get_type().get_raw_type();
		if (!event_type.is_derived_from(RTTI_OF(nap::PointerEvent)))
			return;

		if (event_type == RTTI_OF(PointerPressEvent))
		{
			const PointerPressEvent& press_event = static_cast<const PointerPressEvent&>(inEvent);
			nap::Logger::info("press event!!!" + std::to_string(press_event.mX));
		}

	}

	void CanvasGroupComponentInstance::drawSequenceEditor() {
		mSequenceEditorGUI->show();
	}

	void CanvasGroupComponentInstance::setSequencePlayer() {
		if (mSelected->hasComponent<SequenceCanvasComponent>()) {
			mSequenceEditor->mSequencePlayer = mSelected->getComponent<SequenceCanvasComponentInstance>().mSequencePlayer;
			nap::utility::ErrorState error;
			mSequenceEditor->init(error);
			mSequenceEditorGUI->init(error);
		}
	}

	bool CanvasGroupComponentInstance::initSelectedRenderTarget()
	{
		mSelectedOutputTexture = getEntityInstance()->getCore()->getResourceManager()->createObject<RenderTexture2D>();
		ResourcePtr<RenderTexture2D> outputTexRef = mSelected->getComponent<RenderCanvasComponentInstance>().getOutputTexture();
		mSelectedOutputTexture->mWidth = outputTexRef->mWidth;
		mSelectedOutputTexture->mHeight = outputTexRef->mHeight;
		mSelectedOutputTexture->mFormat = outputTexRef->mFormat;
		mSelectedOutputTexture->mUsage = ETextureUsage::Static;
		nap::utility::ErrorState error;
		if (!mSelectedOutputTexture->init(error))
		{
			error.fail("%s: Failed to initialize selected output texture", mSelectedOutputTexture->mID.c_str());
			return false;
		}
		mSelectedRenderTarget = getEntityInstance()->getCore()->getResourceManager()->createObject<RenderTarget>();
		mSelectedRenderTarget->mColorTexture = mSelectedOutputTexture;
		mSelectedRenderTarget->mClearColor = RGBAColor8(255, 255, 255, 0).convert<RGBAColorFloat>();
		mSelectedRenderTarget->mSampleShading = false;
		mSelectedRenderTarget->mRequestedSamples = ERasterizationSamples::One;
		if (!mSelectedRenderTarget->init(error))
		{
			error.fail("%s: Failed to initialize internal render target", mSelectedRenderTarget->mID.c_str());
			return false;
		}
	}

	void CanvasGroupComponentInstance::drawAllHeadless()
	{
		std::vector<EntityInstance*> mCanvasEntities = getEntityInstance()->getChildren(); //TODO: maybe set in init() and update() when entityinstance children update call?
		for (EntityInstance* canvasEntity : mCanvasEntities) {
			canvasEntity->getComponent<RenderCanvasComponentInstance>().drawAllHeadlessPasses();
		}
	}

	void CanvasGroupComponentInstance::drawSelectedInterface()
	{
		if (mSelected != nullptr) {
			mSelected->getComponent<RenderCanvasComponentInstance>().drawInterface(mSelectedRenderTarget);
		}
		
	}


	void CanvasGroupComponentInstance::drawOutliner() {
		if (ImGui::Button("Toggle Backdrop")) {
			mDrawBackdrop = !mDrawBackdrop;
		}
		for (EntityInstance* canvasEntity : getEntityInstance()->getChildren()) {
			ImGuiTreeNodeFlags node_flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
			if (mSelected == canvasEntity) {
				node_flags |= ImGuiTreeNodeFlags_Selected;
			}
			ImGui::TreeNodeEx((EntityInstance*)canvasEntity, node_flags, canvasEntity->getEntity()->mID.c_str());
			if (ImGui::IsItemClicked())
			{
				mSelected->getComponent<RenderCanvasComponentInstance>().setFinalSampler(false);
				mSelected = canvasEntity;
				setSequencePlayer();
			}
				
		}
		RenderCanvasComponentInstance& canvas_comp = mSelected->getComponent<RenderCanvasComponentInstance>();
		TransformComponentInstance& canvas_transform_comp = mSelected->getComponent<TransformComponentInstance>();
		
		ResourcePtr<RenderTexture2D> canvas_tex = canvas_comp.getOutputTexture();
		float col_width = ImGui::GetContentRegionAvailWidth();
		float ratio_canvas_tex = static_cast<float>(canvas_tex->getWidth()) / static_cast<float>(canvas_tex->getHeight());
		if (ImGui::CollapsingHeader("Preview", ImGuiTreeNodeFlags_None))
		{
			ImGui::Image(*canvas_tex.get(), {col_width , col_width / ratio_canvas_tex});
		}
		utility::ErrorState errorState;
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
		
		if (mSelected->hasComponent<SequenceCanvasComponentInstance>()) {
			SequenceCanvasComponentInstance& seq_canvas_comp = mSelected->getComponent<SequenceCanvasComponentInstance>();
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
		
		if (canvas_comp.mCustomPostPass != nullptr) {
			UniformStructInstance* ubo = canvas_comp.mCustomPostPass->mUBO;
			ImGui::Text("Custom Post Pass");
			float tempPowerTo = ubo->findUniform<UniformFloatInstance>("power_to")->getValue();
			ImGui::DragFloat("Power to", &tempPowerTo, 0.01f, 0.0f, 1.0f);
			if (tempPowerTo != ubo->findUniform<UniformFloatInstance>("power_to")->getValue()) {
				ubo->findUniform<UniformFloatInstance>("power_to")->setValue(tempPowerTo);
			}
		}
		
		
	}	

}