// Local Includes
#include "foglio_driver.h"

// External Includes
#include <utility/fileutils.h>

RTTI_BEGIN_CLASS(nap::FoglioDriver)
RTTI_PROPERTY("Expression", &nap::FoglioDriver::mExpression, nap::rtti::EPropertyMetaData::Required)
RTTI_END_CLASS

namespace nap
{
	bool FoglioDriver::init(utility::ErrorState& errorState)
	{
		//find midi relations
		std::vector<size_t> positions; // holds all the positions that sub occurs within str
		size_t pos = mExpression.find("m_", 0);
		while (pos != std::string::npos)
		{
			positions.push_back(pos);
			pos = mExpression.find("m_", pos + 1);
		}
		
		return true;
	}
}