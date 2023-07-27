#pragma once

#include "canvas.h"
#include "rendercanvascomponent.h"

#include <component.h>
#include <inputcomponent.h>
#include <componentptr.h>

#include <renderservice.h>


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
		
	};

	class NAPAPI CanvasGroupComponentInstance : public InputComponentInstance
	{
		RTTI_ENABLE(InputComponentInstance)
	public:
		CanvasGroupComponentInstance(EntityInstance& entity, Component& resource);

		virtual bool init(utility::ErrorState& errorState) override;
		
		void drawAllHeadless();

	protected:
		virtual void trigger(const nap::InputEvent& inEvent) override;

	private:
		RenderService*								mRenderService = nullptr;
		ComponentPtr<RenderCanvasComponent>			mSelectedCanvas = nullptr;
		std::vector<RenderCanvasComponentInstance*> mCanvases;
	};
}