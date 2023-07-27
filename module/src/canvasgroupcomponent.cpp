
#include "canvasgroupcomponent.h"
#include "rendercanvascomponent.h"
#include "canvasshader.h"
#include "inputcomponent.h"

#include <entity.h>


// nap::rendercanvascomponent run time class definition
RTTI_BEGIN_CLASS(nap::CanvasGroupComponent)
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

	}

	void CanvasGroupComponentInstance::trigger(const nap::InputEvent& inEvent) {
		// Ensure it's a pointer event
		/***
		/rtti::TypeInfo event_type = inEvent.get_type().get_raw_type();
		if (!event_type.is_derived_from(RTTI_OF(nap::PointerEvent)))
			return;

		if (event_type == RTTI_OF(PointerPressEvent))
		{
			const PointerPressEvent& press_event = static_cast<const PointerPressEvent&>(inEvent);
			nap::Logger::info("press event!!! "+press_event.mX);
		}***/
	}

	void CanvasGroupComponentInstance::drawAllHeadless()
	{
		std::vector<EntityInstance*> mCanvasEntities = getEntityInstance()->getChildren();
		for (EntityInstance* canvasEntity : mCanvasEntities) {
			mCanvases.emplace_back(&(canvasEntity->getComponent<RenderCanvasComponentInstance>()));
		}
		for (RenderCanvasComponentInstance* canvasComp : mCanvases) {
			canvasComp->draw();
		}
	}

}