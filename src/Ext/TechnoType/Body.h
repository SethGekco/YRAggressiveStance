#pragma once

#include <TechnoTypeClass.h>
#include <map>

class TechnoTypeExt
{
public:
    static std::map<TechnoTypeClass*, bool> AggressiveStanceAlwaysMap;
    static bool IsAlwaysAggressiveStance(TechnoTypeClass* pType);
};
