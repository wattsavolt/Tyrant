

#include "Utility.h"

namespace tyr
{
	int Utility::NumDigits(int value)
	{
		if (!value)
		{
			return 1;
		}

		int count = 0;
		while (value)
		{
			value /= 10;
			count++;
		}

		return count;
	}
}
