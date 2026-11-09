#include "Body.h"

#include <CCINIClass.h>
#include <InfantryTypeClass.h>
#include <Helpers/Macro.h>
#include <Ext/HouseType/Body.h>
#include <Utilities/Debug.h>
#include <Fundamentals.h>

// ---------------------------------------------------------------------------
// Static member definitions
// ---------------------------------------------------------------------------

std::map<TechnoTypeClass*, bool> TechnoTypeExt::AggressiveStanceAlwaysMap;
std::map<TechnoTypeClass*, bool> TechnoTypeExt::AggressiveStanceExemptMap;
std::map<TechnoTypeClass*, int>  TechnoTypeExt::AggressiveStanceTogglableMap;
std::map<TechnoTypeClass*, bool> TechnoTypeExt::AggressiveStanceVeteranMap;
std::map<TechnoTypeClass*, bool> TechnoTypeExt::AggressiveStanceEliteMap;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static CCINIClass* GetRulesINI()
{
    return CCINIClass::INI_Rules;
}

static const char* const TagAlways  = "AggressiveStance.Always";
static const char* const TagExempt  = "AggressiveStance.Exempt";
static const char* const TagVeteran = "AggressiveStance.Veteran";
static const char* const TagElite   = "AggressiveStance.Elite";

static bool ReadBoolTag(TechnoTypeClass* pType, const char* tag)
{
    bool val = false;
    if (auto* pINI = GetRulesINI())
        if (pType->ID)
            val = pINI->ReadBool(pType->ID, tag, false);
    return val;
}

static int ReadTogglable(TechnoTypeClass* pType)
{
    auto* pINI = GetRulesINI();
    if (!pINI || !pType->ID)
        return -1;

    // Check if the tag is present at all
    char buf[8] = {};
    pINI->ReadString(pType->ID, "AggressiveStance.Togglable", "", buf, sizeof(buf));
    if (buf[0] == '\0')
        return -1;  // Tag absent — use auto-detect

    return pINI->ReadBool(pType->ID, "AggressiveStance.Togglable", true) ? 1 : 0;
}

// Read the map; on a miss (a type made after the last prewarm) read the INI
// and add it, as every lookup did before the prewarm.
template <typename T, typename Read>
static T Cached(std::map<TechnoTypeClass*, T>& map, TechnoTypeClass* pType, const char* what, Read read)
{
    if (PrewarmVerify::On)
        PrewarmVerify::Lookup();

    auto it = map.find(pType);
    if (it != map.end())
        return it->second;

    PrewarmVerify::Miss(what, pType->ID);
    T const val = read(pType);
    map[pType] = val;
    return val;
}

bool TechnoTypeExt::IsAlwaysAggressiveStance(TechnoTypeClass* pType)
{
    if (!pType) return false;
    return Cached(AggressiveStanceAlwaysMap, pType, TagAlways,
        [](TechnoTypeClass* p) { return ReadBoolTag(p, TagAlways); });
}

bool TechnoTypeExt::IsExemptFromAggressiveStance(TechnoTypeClass* pType)
{
    if (!pType) return false;
    return Cached(AggressiveStanceExemptMap, pType, TagExempt,
        [](TechnoTypeClass* p) { return ReadBoolTag(p, TagExempt); });
}

bool TechnoTypeExt::IsAggressiveWhenVeteran(TechnoTypeClass* pType)
{
    if (!pType) return false;
    return Cached(AggressiveStanceVeteranMap, pType, TagVeteran,
        [](TechnoTypeClass* p) { return ReadBoolTag(p, TagVeteran); });
}

bool TechnoTypeExt::IsAggressiveWhenElite(TechnoTypeClass* pType)
{
    if (!pType) return false;
    return Cached(AggressiveStanceEliteMap, pType, TagElite,
        [](TechnoTypeClass* p) { return ReadBoolTag(p, TagElite); });
}

int TechnoTypeExt::GetTogglable(TechnoTypeClass* pType)
{
    if (!pType) return -1;
    return Cached(AggressiveStanceTogglableMap, pType, "AggressiveStance.Togglable", ReadTogglable);
}

void TechnoTypeExt::Prewarm()
{
    AggressiveStanceAlwaysMap.clear();
    AggressiveStanceExemptMap.clear();
    AggressiveStanceTogglableMap.clear();
    AggressiveStanceVeteranMap.clear();
    AggressiveStanceEliteMap.clear();

    for (int i = 0; i < TechnoTypeClass::Array->Count; i++)
    {
        TechnoTypeClass* pType = TechnoTypeClass::Array->Items[i];
        if (!pType)
            continue;

        AggressiveStanceAlwaysMap[pType] = ReadBoolTag(pType, TagAlways);
        AggressiveStanceExemptMap[pType] = ReadBoolTag(pType, TagExempt);
        AggressiveStanceTogglableMap[pType] = ReadTogglable(pType);
        AggressiveStanceVeteranMap[pType] = ReadBoolTag(pType, TagVeteran);
        AggressiveStanceEliteMap[pType] = ReadBoolTag(pType, TagElite);
    }
}

// ---------------------------------------------------------------------------
// PrewarmVerify
// ---------------------------------------------------------------------------

bool PrewarmVerify::On = false;

static unsigned VerifyLookups = 0;
static unsigned VerifyMisses = 0;
static unsigned VerifyDiffs = 0;

void PrewarmVerify::Start()
{
    char value[8] {};
    On = GetEnvironmentVariableA("YRAS_VERIFY_PREWARM", value, sizeof(value)) > 0 && value[0] == '1';
    VerifyLookups = 0;
}

template <typename T, typename Read>
static void Compare(std::map<TechnoTypeClass*, T>& map, TechnoTypeClass* pType, const char* what, Read read)
{
    auto it = map.find(pType);
    if (it == map.end())
        return;  // a miss; logged when it happened

    T const fresh = read(pType);
    if (it->second != fresh && VerifyDiffs++ < 20)
    {
        Debug::Log("verify prewarm: %s of %s is %d prewarmed, %d read now\n",
            what, pType->ID, static_cast<int>(it->second), static_cast<int>(fresh));
    }
}

void PrewarmVerify::Lookup()
{
    // Re-read every prewarmed tag at the 1st, 2nd, 4th, ... lookup after
    // the prewarm, as the old code read a tag at its type's first lookup.
    unsigned const n = ++VerifyLookups;
    if (n & (n - 1))
        return;

    unsigned const diffsBefore = VerifyDiffs;
    int types = 0;
    for (int i = 0; i < TechnoTypeClass::Array->Count; i++)
    {
        TechnoTypeClass* pType = TechnoTypeClass::Array->Items[i];
        if (!pType)
            continue;

        ++types;
        Compare(TechnoTypeExt::AggressiveStanceAlwaysMap, pType, TagAlways,
            [](TechnoTypeClass* p) { return ReadBoolTag(p, TagAlways); });
        Compare(TechnoTypeExt::AggressiveStanceExemptMap, pType, TagExempt,
            [](TechnoTypeClass* p) { return ReadBoolTag(p, TagExempt); });
        Compare(TechnoTypeExt::AggressiveStanceTogglableMap, pType, "AggressiveStance.Togglable", ReadTogglable);
        Compare(TechnoTypeExt::AggressiveStanceVeteranMap, pType, TagVeteran,
            [](TechnoTypeClass* p) { return ReadBoolTag(p, TagVeteran); });
        Compare(TechnoTypeExt::AggressiveStanceEliteMap, pType, TagElite,
            [](TechnoTypeClass* p) { return ReadBoolTag(p, TagElite); });
    }

    int countries = 0;
    for (int i = 0; i < HouseTypeClass::Array->Count; i++)
    {
        HouseTypeClass* pType = HouseTypeClass::Array->Items[i];
        if (!pType)
            continue;

        ++countries;
        auto it = HouseTypeExt::AggressiveStanceMap.find(pType);
        if (it == HouseTypeExt::AggressiveStanceMap.end())
            continue;

        bool const fresh = HouseTypeExt::ReadAggressiveStance(pType);
        if (it->second != fresh && VerifyDiffs++ < 20)
        {
            Debug::Log("verify prewarm: AggressiveStance of country %s is %d prewarmed, %d read now\n",
                pType->ID, static_cast<int>(it->second), static_cast<int>(fresh));
        }
    }

    Debug::Log("verify prewarm: lookup %u, frame %d: %d techno types and %d countries re-read, "
        "%u differ now, %u differences and %u misses since the start\n",
        n, static_cast<int>(Unsorted::CurrentFrame), types, countries, VerifyDiffs - diffsBefore, VerifyDiffs, VerifyMisses);
}

void PrewarmVerify::Miss(const char* what, const char* id)
{
    if (On && VerifyMisses++ < 20)
        Debug::Log("verify prewarm: %s of %s missed the cache (read now)\n", what, id ? id : "(no ID)");
}
