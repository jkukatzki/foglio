#pragma once

 // External includes
#include <rtti/typeinfo.h>
#include <nap/resource.h>

namespace nap
{
	/**
	 * Resource parsing string and connecting to needed signals coming from midi / sequences / parameters
	 */
	class FoglioDriver : public Resource
	{
		RTTI_ENABLE(Resource)
	public:
		virtual bool init(utility::ErrorState& errorState) override;

		std::string mExpression; ///< Property: 'Expression' string to parse

		std::vector<std::string> mMidiRelations = {};

	};
}