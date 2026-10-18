# Super Smash Bros. for 3DS: plan (18 Oct 2026)

Goal: the game reaches its title screen with a picture. Today: boots, runs at speed 58, black screens, earlier an "extra data is corrupted" dialog.

## Known (✅ from the run log)
- Unimplemented or failing in the file system: archive `0x2345678A` (SavedataAndNcch) in `OpenFileDirectly`; 8 failed `OpenFile`, 1 failed `OpenDirectory`; NCCH archive `mediatype 1` (SD title, "no such title installed"); `AM::GetDLCTitleInfo`; `FS::GetThisSaveDataSecureValue` / `SetThisSaveDataSecureValue` are stubs.
- The `FS_DEBUG` prints (paths of failed opens) do not appear although `Module.ENV.FS_DEBUG` is set in the page.

## Steps (each ends with a check: re-run Smash and read the log)
1. **Make the paths visible.** Find why `FS_DEBUG` stays off (read the variable once at start and print its value). Check: the failed `OpenFile` lines show archive and path.
2. **Name the failing files.** Group the 8 `OpenFile` and the `OpenDirectory` by archive and path. Check: a table "archive / path / who asks".
3. **Look at working code (do not invent).** Read how Azahar/Citra implement archive `0x2345678A` (SaveDataAndContent), `GetDLCTitleInfo` and the secure value (3dbrew has the IPC layout: `0x086F0040` request `u32 slot`; response `found`, `from card`, `u64 value`). Check: notes with the exact semantics and file references.
4. **Implement the smallest version.** (a) SavedataAndNcch: forward opens to the title's own save data or romfs by path type; (b) secure value: remember `slot -> value` (persist like the saves), fix the response size (5 words); (c) `GetDLCTitleInfo`: empty answer; (d) ExtSaveData: create it when the game asks for it. Check: fewer failed opens; the "extra data" dialog does not come back.
5. **If the picture is still black:** compare the draw state (`GL_DEBUG_STATE`, `GL_DRAW_STATS`) and the service calls with a known-good emulator log; look for a `panic` / unknown service. Check: the first frame with 3D or UI.
6. **Regression run:** Mario 3D Land (route to World 1-1), Mario Kart 7 start, the three homebrew demos.
7. **Candidates for upstream** (separate branches from upstream master, one topic each, only after your "yes"): secure value store, archive 0x2345678A, `GetDLCTitleInfo`.

## Effort and risks
- Steps 1-3: about an hour. Step 4: a few hours, mostly in the core (`src/core/fs`, `services/fs.cpp`, `services/am.cpp`). Step 5 is open-ended.
- Risks: Smash may need more services (an unknown service stops the emulator with a `panic`); the 2 GB image is read lazily (works); the page can look fine while the game waits for something (watch the guest PC in the log).
- Done by me alone except looking at the screen after dialogs; the phone is not needed.
