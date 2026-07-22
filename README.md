# DemocracyGame

DemocracyGame is an Unreal Engine prototype for a hybrid political simulation and real-time strategy game.

## Current Prototype Scope

- Login and single-player state setup flow
- Local save/load, autosave, and protected backup save handling
- Interactive 3D office prototype with computer, phone, briefing folder, hallway, meeting room, and press room interactions
- Simulation systems for policies, advisors, events, demographics, economy, resources, departments, decision history, approval/stability, press releases, meetings, development, and fail-state risk
- Computer-driven simulation dashboard for managing the current state

## Project Layout

- `Source/Democracy/` - C++ gameplay, UI, save, office, and simulation prototype code
- `Config/` - Unreal project configuration and difficulty profile data
- `Content/` - Unreal assets used by the prototype
- `AssetsNeeded.txt` and `Assets_Needed.txt` - running asset requirement notes

## Git Notes

This repository uses Git LFS for Unreal binary assets. Install Git LFS before cloning or working with the project:

```powershell
git lfs install
```

Generated Unreal folders such as `Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/`, and local `Saves/` are intentionally ignored.