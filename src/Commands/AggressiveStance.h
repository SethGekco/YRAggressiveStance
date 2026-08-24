#pragma once

#include "Commands.h"
#include <ObjectClass.h>
#include <AbstractClass.h>
#include <TechnoClass.h>
#include <FootClass.h>
#include <map>

// Select next idle harvester
class AggressiveStanceClass : public CommandClass
{
public:
	static std::map<TechnoClass*, bool> AggressiveStanceMap;

	// Warhead-granted ("friendly Chaos Gas") Aggressive Stance. Value is the
	// game frame the grant expires on, or -1 for an indefinite grant. Separate
	// from AggressiveStanceMap so a warhead grant and the hotkey toggle don't
	// clobber each other.
	static std::map<TechnoClass*, int> GrantExpiry;

	// True if pTechno currently has an unexpired warhead grant. Expired entries
	// are pruned on read.
	static bool IsGrantActive(TechnoClass* pTechno);

	// Applies a warhead grant. duration: -1 = forever, 0 = clear/off, >0 = frames.
	// cumulative adds to the remaining duration instead of overwriting it.
	static void ApplyGrant(TechnoClass* pTechno, int duration, bool cumulative);

	// CommandClass
	virtual const char* GetName() const override;
	virtual const wchar_t* GetUIName() const override;
	virtual const wchar_t* GetUICategory() const override;
	virtual const wchar_t* GetUIDescription() const override;
	virtual void Execute(WWKey eInput) const override;
};
