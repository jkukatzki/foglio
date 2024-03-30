// Local Includes
#include "foglioservice.h"
#include "foglio_driver.h"
#include "canvasgroupcomponent.h"

// External Includes
#include <nap/core.h>
#include <nap/resourcemanager.h>
#include <nap/logger.h>
#include <iostream>
#include <midiinputcomponent.h>
#include <scene.h>
#include <entity.h>

RTTI_BEGIN_CLASS_NO_DEFAULT_CONSTRUCTOR(nap::FoglioService)
	RTTI_CONSTRUCTOR(nap::ServiceConfiguration*)
RTTI_END_CLASS

namespace nap
{
	bool FoglioService::init(nap::utility::ErrorState& errorState)
	{
		//Logger::info("Initializing FoglioService");
		return true;
	}


	void FoglioService::update(double deltaTime)
	{
	}
	
	void FoglioService::postResourcesLoaded() {
		auto drivers = getCore().getResourceManager()->getObjects<FoglioDriver>();
		//if object has items in midi relation array, check if midi input component exists, if not create it
		for (auto driver : drivers) {
			if (driver->mMidiRelations.size() > 0 && getCore().getResourceManager()->getObjects<MidiInputComponent>().size() == 0) {
				// Create a new MidiInputComponent and add it to the scene
				/*ComponentInstance midiInputComponent = std::make_unique<MidiInputComponentInstance>();
				midiInputComponent->init(nap::utility::ErrorState());
				auto scene = getCore().getResourceManager()->findObject<Scene>("Scene");
				getCore().getResourceManager()->getObjects<CanvasGroupComponentInstance>()[0]->getEntityInstance()->addComponent(midiInputComponent);*/
			}
		}
		fileLoaded();
	}

	void FoglioService::getDependentServices(std::vector<rtti::TypeInfo>& dependencies)
	{
	}
	

	void FoglioService::shutdown()
	{
	}
}
