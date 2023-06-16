// Local Includes
#include "foglioservice.h"

// External Includes
#include <nap/core.h>
#include <nap/resourcemanager.h>
#include <nap/logger.h>
#include <iostream>

RTTI_BEGIN_CLASS_NO_DEFAULT_CONSTRUCTOR(nap::foglioService)
	RTTI_CONSTRUCTOR(nap::ServiceConfiguration*)
RTTI_END_CLASS

namespace nap
{
	bool foglioService::init(nap::utility::ErrorState& errorState)
	{
		//Logger::info("Initializing foglioService");
		return true;
	}


	void foglioService::update(double deltaTime)
	{
	}
	

	void foglioService::getDependentServices(std::vector<rtti::TypeInfo>& dependencies)
	{
	}
	

	void foglioService::shutdown()
	{
	}
}
