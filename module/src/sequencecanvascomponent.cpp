#include "sequencecanvascomponent.h"
#include "rendercanvascomponent.h"

#include <entity.h>
#include <nap/core.h>
#include <nap/resourceptr.h>
#include <renderglobals.h>
#include <nap/resourceptr.h>
#include <rtti/objectptr.h>
#include <nap/resourceptr.h>


// nap::rendercanvascomponent run time class definition
RTTI_BEGIN_CLASS(nap::SequenceCanvasComponent)
	RTTI_PROPERTY("Sequence", &nap::SequenceCanvasComponent::mSequencePlayer, nap::rtti::EPropertyMetaData::Default)
RTTI_END_CLASS

RTTI_BEGIN_CLASS_NO_DEFAULT_CONSTRUCTOR(nap::SequenceCanvasComponentInstance)
RTTI_CONSTRUCTOR(nap::EntityInstance&, nap::Component&)
RTTI_END_CLASS

namespace nap
{

	SequenceCanvasComponentInstance::SequenceCanvasComponentInstance(EntityInstance& entity, Component& resource) :
		ComponentInstance(entity, resource)
	{ }






	bool SequenceCanvasComponentInstance::init(utility::ErrorState& errorState)
	{
		// Fetch the resource manager
		SequenceCanvasComponent* resource = getComponent<SequenceCanvasComponent>();
		//mapping output by hardcoded number
		const ResourcePtr<SequencePlayerEventOutput> canvasEventOutput = resource->mSequencePlayer->mOutputs[0];
		if (!errorState.check(canvasEventOutput != nullptr, "unable to find CanvasSequenceEventReceiver with index: %s", 0))
			return false;

		canvasEventOutput->mSignal.connect(mSelectVideoSlot);
		return true;

	}

	void SequenceCanvasComponentInstance::selectVideo(const SequenceEventBase& sequenceEvent) {
		VideoPlayer* player = getEntityInstance()->getComponent<RenderCanvasComponentInstance>().getVideoPlayer();
		utility::ErrorState error;
		error.check(player != nullptr, "unable to find player in rendercanvascomponent:");
		const SequenceEventInt& eventInt = static_cast<const SequenceEventInt&>(sequenceEvent);
		nap::Logger::info("Select Video Event from Sequence. SelectIndex = %i", (eventInt.getValue()) % player->getCount());
		player->selectVideo((eventInt.getValue()) % player->getCount(), error);
		player->play();
	}

}