# winfe-resurrect — 1993 Udanax Gold Frontend Visual Resurrection

Pure historical recreation of the **winfe** Windows frontend that shipped with the open-source Udanax Gold release.

This is **not** a working hypertext client. It is a faithful visual and structural resurrection of the 1993 Borland OWL-based UI so that the original look, menus, dialogs and form layouts can be experienced on modern systems.

## What is restored

**Step 3** (current):

- Main frame + original Document / Edit / Window menus
- Modal Login, Document Info, and Link Info dialogs
- Desktop / Home, Memo, Rate forms
- **Full Akord “General Liability Section” form** reconstructed from the original `pcaide/akord.pvt` resource (coverages, options, limits, producer/applicant, historical sample data and pre-checked boxes)
- Classic MS Sans Serif styling throughout

Fonts intentionally use **MS Sans Serif** to evoke the Windows 3.1 / early Windows 95 era.

## Building

### Nix (recommended on Linux / NixOS)

```bash
nix build          # → result/bin/winfe_resurrect.exe  (Windows PE)
nix run            # build + launch under Wine
nix develop        # shell with MinGW cross-compiler + Wine
```

### Windows

```bat
g++ -std=c++17 -O2 -mwindows winfe_resurrect.cpp -o winfe_resurrect.exe -lcomctl32 -lgdi32 -luser32
```

or with MSVC:

```bat
cl /EHsc /O2 /Fe:winfe_resurrect.exe winfe_resurrect.cpp user32.lib gdi32.lib comctl32.lib
```

### Manual MinGW cross-compile

```bash
make               # uses x86_64-w64-mingw32-g++ by default
wine winfe_resurrect.exe
```

## Source of truth

All menu strings, dialog captions, control labels and sample content were taken directly from the original `winfe/fe.rc`, `winfe/fe.h` and the `pcaide/` form sources in the Udanax Gold tree:

- https://github.com/xanatoo/udanax-gold

The original application used Borland ObjectWindows Library (OWL) + a custom `PVDialog` layer and was tightly bound to the Udanax mid-end object model. This recreation intentionally strips every non-visual dependency so the UI can stand alone for historical study.

## Limitations (by design)

- No FeBe / backend connection
- No real document model, versioning or transclusion
- MDI is emulated with ordinary overlapped windows rather than a true MDI client
- Some advanced OWL behaviours (exact focus handling, BWCC controls, etc.) are not reproduced
- Akord form is a faithful reconstruction from `akord.pvt` but not pixel-perfect to the original dialog units

## Extending the resurrection

The dialog layouts in `fe.rc` and the `.pvt` resources under `pcaide/` are the authoritative source. Additional forms (Attn, CProf, Tabs) can be expanded by the same CreateWindow pattern.

---

Historical effort only. The modern hypertext work lives elsewhere.
