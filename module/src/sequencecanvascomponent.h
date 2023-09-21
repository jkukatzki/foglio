#pragma once
#include "rendercanvascomponent.h"

#include <component.h>
#include <rendercomponent.h>
#include <nap/resourceptr.h>
#include <rendertexture2d.h>
#include <planemesh.h>
#include <rendertarget.h>
#include <materialinstance.h>
#include <renderablemesh.h>
#include <videoplayer.h>
#include <imagefromfile.h>
#include <foglioservice.h>
#include <transformcomponent.h>
#include <material.h>
#include <nap/resourceptr.h>

#include <sequenceplayer.h>
#include <sequenceevent.h>
#include <sequenceplayereventoutput.h>

namespace nap
{
	// Forward declares
	class SequenceCanvasComponentInstance;

	class NAPAPI SequenceCanvasComponent : public Component
	{
		RTTI_ENABLE(Component)
			DECLARE_COMPONENT(SequenceCanvasComponent, SequenceCanvasComponentInstance)

	public:
		ResourcePtr<SequencePlayer>		mSequencePlayer = nullptr;
	};

	class NAPAPI SequenceCanvasComponentInstance : public ComponentInstance
	{
		RTTI_ENABLE(ComponentInstance)
	public:
		SequenceCanvasComponentInstance(EntityInstance& entity, Component& resource);

		virtual bool init(utility::ErrorState& errorState) override;

	private:
		RenderCanvasComponentInstance* mRenderCanvasComponent;
		ResourcePtr<Canvas> mCanvas = nullptr;
		void selectVideo(const SequenceEventBase& sequenceEvent);
		nap::Slot<const SequenceEventBase&>		mSelectVideoSlot = { this, &SequenceCanvasComponentInstance::selectVideo };
	};
}