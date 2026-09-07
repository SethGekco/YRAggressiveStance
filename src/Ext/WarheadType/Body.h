#pragma once

#include <WarheadTypeClass.h>
#include <TechnoClass.h>
#include <HouseClass.h>
#include <map>

// Which units caught by a warhead's blast are granted Aggressive Stance,
// judged relative to the firing house. This is a flag set parsed from a
// comma/space separated list (e.g. "allies,neutral"). Every target falls into
// exactly one category: neutral (civilian/passive house), owner (the firer),
// allies, or enemies.
//   owner              -> the firer's own units
//   allies             -> the firer's allied units
//   enemies            -> the firer's (non-neutral) enemies
//   neutral            -> civilian / MultiplayPassive houses
//   all                -> everything the warhead hits (default)
//   none               -> nothing
namespace ASHouse
{
    enum : unsigned char
    {
        None    = 0x0,
        Owner   = 0x1,
        Allies  = 0x2,
        Enemies = 0x4,
        Neutral = 0x8,
        All     = Owner | Allies | Enemies | Neutral,
    };
}

// Extends WarheadTypeClass with the AggressiveStance grant tags, read from INI
// once per warhead and cached (mirrors the lazy caching used by TechnoTypeExt).
class WarheadTypeExt
{
public:
    struct GrantInfo
    {
        bool          Has { false };          // AggressiveStance.Duration present?
        int           Duration { 0 };         // frames; -1 = forever, 0 = clear/off
        bool          Cumulative { false };   // add to remaining vs overwrite
        unsigned char AffectsHouses { ASHouse::All };  // ASHouse flag set, default All
    };

    // Lazily reads + caches the grant tags for pWH.
    static const GrantInfo& GetAggressiveStance(WarheadTypeClass* pWH);

    // True if a unit owned by pTarget's house should be granted the stance when
    // the warhead was fired by pFirerHouse, under the given ASHouse flag set.
    static bool PassesHouseFilter(unsigned char mask, TechnoClass* pTarget, HouseClass* pFirerHouse);

private:
    static std::map<WarheadTypeClass*, GrantInfo> Cache;
};
