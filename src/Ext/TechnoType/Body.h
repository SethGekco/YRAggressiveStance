#pragma once

#include <TechnoTypeClass.h>
#include <map>

// Extends TechnoTypeClass with per-type INI tags that cannot be stored in
// the fixed vanilla struct.
class TechnoTypeExt
{
public:
    // Cache maps — keyed on TechnoTypeClass*, populated lazily on first
    // encounter during gameplay. Absence from a map means the default applies.

    // AggressiveStance.Always=yes  (on attacker type)
    // Units of this type are permanently in aggressive stance regardless of
    // the player hotkey or team membership.
    static std::map<TechnoTypeClass*, bool> AggressiveStanceAlwaysMap;

    // AggressiveStance.Exempt=yes  (on TARGET type)
    // Units/buildings of this type cannot be targeted by aggressive stance
    // attackers via the ThreatPosed=0 bypass. Vanilla targeting still applies.
    static std::map<TechnoTypeClass*, bool> AggressiveStanceExemptMap;

    // AggressiveStance.Togglable=yes/no  (on attacker type)
    // Controls whether the hotkey can toggle this unit into/out of aggressive
    // stance. Default: auto-detected (armed/open-topped = yes, engineer = no).
    // -1 = not set (use auto-detect), 0 = forced off, 1 = forced on.
    static std::map<TechnoTypeClass*, int> AggressiveStanceTogglableMap;

    // AggressiveStance.Veteran=yes / AggressiveStance.Elite=yes  (on attacker type)
    // Units of this type become aggressive once they reach the given rank:
    // Veteran = veteran or elite, Elite = elite only.
    static std::map<TechnoTypeClass*, bool> AggressiveStanceVeteranMap;
    static std::map<TechnoTypeClass*, bool> AggressiveStanceEliteMap;

    static bool IsAlwaysAggressiveStance(TechnoTypeClass* pType);
    static bool IsExemptFromAggressiveStance(TechnoTypeClass* pType);

    // Returns: -1 = not set (auto), 0 = not togglable, 1 = togglable
    static int  GetTogglable(TechnoTypeClass* pType);

    static bool IsAggressiveWhenVeteran(TechnoTypeClass* pType);
    static bool IsAggressiveWhenElite(TechnoTypeClass* pType);
};
