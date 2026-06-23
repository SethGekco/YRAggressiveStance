#pragma once

#include <TechnoTypeClass.h>
#include <map>

// Stores per-TechnoType data read from INI that cannot be stored in the
// vanilla TechnoTypeClass struct (which is fixed in the binary).
class TechnoTypeExt
{
public:
    // True if units of this type should always be in Aggressive Stance,
    // regardless of the player hotkey.  Corresponds to the INI tag:
    //   AggressiveStance.Always=yes   (default: no)
    static std::map<TechnoTypeClass*, bool> AggressiveStanceAlwaysMap;

    // Returns true if the given type has AggressiveStance.Always=yes.
    static bool IsAlwaysAggressiveStance(TechnoTypeClass* pType);
};
