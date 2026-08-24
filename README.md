This is an edited forked copy of the original. It adds several new ways to put units into Aggressive Stance:

```ini
[SomeTechnoType]
AggressiveStance.Always=yes         ; make this techno type (infantry, vehicle, building, etc.) always aggressive-stanced
AggressiveStance.Veteran=yes        ; become aggressive once this unit reaches Veteran rank (or higher)
AggressiveStance.Elite=yes          ; become aggressive once this unit reaches Elite rank

[SomeCountry]
AggressiveStance=yes                ; every unit owned by a house of this country is always aggressive

[SomeTeamType]
AggressiveStance=yes                ; every member of a team of this type is aggressive while it belongs to the team

[SomeWarhead]                       ; "friendly Chaos Gas" - units the warhead hits become aggressive for a while
AggressiveStance=yes                ; enable the effect (equivalent to AggressiveStance.Duration=-1 on its own)
AggressiveStance.Duration=-1        ; game frames; -1 = indefinite, 0 = clear an existing grant, >0 = timed
AggressiveStance.Cumulative=no      ; yes = add Duration to the time remaining, no = overwrite it
AggressiveStance.AffectsHouses=all  ; comma list relative to the firer: owner, allies, enemies, neutral, all, none (default all)
```

# YRAggressiveStance
An engine extension that enables Aggressive Stance for Yuri's Revenge.

Also available at Nexus Mods: [https://www.nexusmods.com/commandandconquerredalert2/mods/143](https://www.nexusmods.com/commandandconquerredalert2/mods/143).

# FAQ

Q: How to use it?

A: Follow the steps.
1. Either download the source code and build a DLL, or download the built DLL from the release panel.
2. Put the DLL into the same location as `Syringe.exe` and `Ares.dll`. It is supposed to be detected by `Syringe.exe` and allows the use of Aggressive Stance.
3. After the game is launched, you are supposed to see Aggressive Stance in the hotkey configuration menu.
4. Select your armed units and/or buildings and press the configured hotkey. They will now passively engage unarmed enemy buildings.


Q: Is it safe to use it in multiplayer games?

A: This is multiplayer safe as long as every human player has `YRAggressiveStance.dll` installed. If **any** human player did not install `YRAggressiveStance.dll`, any attempt to make use of the Aggressive Stance will lead to a desync. **This extension CANNOT be used as a cheat.**


Q: Do you take feature requests?

A: Sorry, I don't.


Q: Can I make a modified build of `YRAggressiveStance.dll`?

A: Of course. Just make sure every human player has the identical `YRAggressiveStance.dll` installed, or the game will end in a desync. I am not responsible if someone tries to cheat in a multiplayer game using a modified build of `YRAggressiveStance.dll`, the same trick can be used on any open-source extension, even `Phobos.dll`.
