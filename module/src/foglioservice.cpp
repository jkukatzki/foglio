// Local Includes
#include "foglioservice.h"

// External Includes
#include <nap/core.h>
#include <nap/resourcemanager.h>
#include <nap/logger.h>
#include <iostream>

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
	

	void FoglioService::getDependentServices(std::vector<rtti::TypeInfo>& dependencies)
	{
	}
	

	void FoglioService::shutdown()
	{
	}
}
