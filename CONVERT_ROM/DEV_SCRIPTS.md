# CONVERT_ROM dev scripts

This folder includes a few helper scripts that are **not required** for normal conversion/build flows, but can be useful for development and maintenance.

## Configuration

Some dev scripts call external tools. Configure their paths in `external_apps.py` (this file is git-ignored).

1. Copy `external_apps_template.py` to `external_apps.py`
2. Edit the paths for your system

### MAME

`extract_games_from_mame.py` uses MAME to generate an XML database via `mame.exe -listxml`.

Minimum supported version: **MAME 0.284** (or newer).

- Configure `MAME_PATH` in `external_apps.py` (the directory that contains `mame.exe`)
- Default template value is `C:\MAME`

## Scripts

### extract_games_from_mame.py

Purpose: Generates a Markdown table of MAME machines that use target LCD CPU families (SM5A/SM510/SM511).

Usage (from this folder):

```powershell
cd CONVERT_ROM
python extract_games_from_mame.py
```

Notes:
- Writes output to `EXTRACTED_LIST.md` in this folder.
- If MAME cannot be found, it will print an error showing the resolved `mame.exe` path.
