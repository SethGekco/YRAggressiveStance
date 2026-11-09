#pragma once

#include <TechnoTypeClass.h>
#include <map>

// Extends TechnoTypeClass with per-type INI tags that cannot be stored in
// the fixed vanilla struct.
class TechnoTypeExt
{
public:
    // Cache maps — keyed on TechnoTypeClass*, filled for every type by
    // Prewarm(); a type made after it is read on first encounter. Absence
    // from a map means the default applies.

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

    // Clears the maps and reads every tag above for every type in
    // TechnoTypeClass::Array. Called at the end of each RulesClass::Process
    // (0x668F6A), after the map's own rules, so the lookups during a game
    // only read the maps: PerfExt's lane 3 may run the target checks that
    // make them on several threads at once.
    static void Prewarm();
};

// YRAS_VERIFY_PREWARM=1 in the game's environment (test runs only): the 1st,
// 2nd, 4th, ... cached lookup after a prewarm re-reads every prewarmed tag
// from INI_Rules, as the old code read a tag at its type's first lookup, and
// logs any difference; a lookup that misses the cache is logged too.
namespace PrewarmVerify
{
    extern bool On;
    void Start();  // at each prewarm: reads the switch, restarts the count
    void Lookup();  // call only when On
    void Miss(const char* what, const char* id);
}
