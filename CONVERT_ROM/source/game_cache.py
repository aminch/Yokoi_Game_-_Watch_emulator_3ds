from __future__ import annotations

import json
import os
import time
from pathlib import Path

from .manufacturer_ids import MANUFACTURER_NINTENDO, normalize_manufacturer_id


_enabled: bool = False
_cache_dir: Path = Path(r".\\tmp\\cache")
_target_name: str = "unknown"

# Context needed to determine cache validity and find outputs.
_context: dict = {
    "export_dpi": 0,
    "size_scale": 1,
    "resolution_up": [0, 0],
    "resolution_down": [0, 0],
    "console_size": [0, 0],
    "console_atlas_size": [0, 0],
    "tex3ds_enabled": True,
    "texture_path_prefix": "",
    "texture_path_ext": "",
    "destination_game_file": "",
    "destination_graphique_file": "",
    "default_console": r".\\rom\\default.png",
    "default_alpha_bright": 1.7,
    "default_fond_bright": 1.35,
    "default_rotate": False,
}


def configure(
    *,
    enabled: bool,
    target_name: str,
    cache_dir: str | Path = Path(r".\\tmp\\cache"),
    export_dpi: int,
    size_scale: int,
    resolution_up: list[int],
    resolution_down: list[int],
    console_size: list[int],
    console_atlas_size: list[int],
    tex3ds_enabled: bool,
    texture_path_prefix: str,
    texture_path_ext: str,
    destination_game_file: str,
    destination_graphique_file: str,
    default_console: str,
    default_alpha_bright: float,
    default_fond_bright: float,
    default_rotate: bool,
) -> None:
    """Configure cache module for the current run.

    convert_3ds.py calls this once after it has applied the target profile.
    """

    global _enabled, _cache_dir, _target_name

    _enabled = bool(enabled)
    _cache_dir = Path(cache_dir)
    _target_name = str(target_name)

    _context.update({
        "export_dpi": int(export_dpi),
        "size_scale": int(size_scale),
        "resolution_up": list(resolution_up),
        "resolution_down": list(resolution_down),
        "console_size": list(console_size),
        "console_atlas_size": list(console_atlas_size),
        "tex3ds_enabled": bool(tex3ds_enabled),
        "texture_path_prefix": str(texture_path_prefix),
        "texture_path_ext": str(texture_path_ext),
        "destination_game_file": str(destination_game_file),
        "destination_graphique_file": str(destination_graphique_file),
        "default_console": str(default_console),
        "default_alpha_bright": float(default_alpha_bright),
        "default_fond_bright": float(default_fond_bright),
        "default_rotate": bool(default_rotate),
    })


def enabled() -> bool:
    return bool(_enabled)


def ensure_cache_dir() -> None:
    if not _enabled:
        return
    try:
        _cache_dir.mkdir(parents=True, exist_ok=True)
    except Exception:
        # Cache must never break builds.
        return


def invalidate_game(key: str) -> None:
    """Remove a game's cache file (best-effort)."""

    try:
        p = _cache_file_for_game(str(key))
        if p.exists():
            p.unlink()
    except Exception:
        return


def try_load_pack_meta(key: str, game_data: dict, *, clean_mode: bool) -> dict | None:
    """Try to load cached pack_meta for a game.

    Prints reasons when cache is enabled but can't be used.
    Returns cached pack_meta dict when valid, else None.
    """

    if not _enabled:
        return None
    if clean_mode:
        return None

    cache_path = _cache_file_for_game(key)
    if not cache_path.exists():
        return None

    try:
        with open(cache_path, "r", encoding="utf-8") as f:
            payload = json.load(f)
    except Exception as e:
        print(f"(cache) Rebuild {key}: invalid cache file ({e})")
        return None

    cached_sig = payload.get("signature")
    cached_meta = payload.get("pack_meta")
    if not isinstance(cached_sig, dict) or not isinstance(cached_meta, dict):
        print(f"(cache) Rebuild {key}: cache missing required fields")
        return None

    current_sig = _build_game_signature(key, game_data)
    if cached_sig != current_sig:
        reasons = _describe_cache_mismatch(cached_sig, current_sig)
        if reasons:
            print(f"(cache) Rebuild {key}:")
            for r in reasons:
                print(f"  - {r}")
        else:
            print(f"(cache) Rebuild {key}: signature mismatch")
        return None

    if not _expected_outputs_exist_for_game(key, game_data):
        missing = _describe_missing_outputs_for_game(key, game_data)
        print(f"(cache) Rebuild {key}: expected outputs missing")
        for r in missing:
            print(f"  - {r}")
        return None

    return cached_meta


def write_game_cache(key: str, game_data: dict, pack_meta: dict) -> None:
    if not _enabled:
        return

    try:
        _cache_dir.mkdir(parents=True, exist_ok=True)
        payload = {
            "signature": _build_game_signature(key, game_data),
            "pack_meta": pack_meta,
            "written_at": int(time.time()),
        }
        with open(_cache_file_for_game(key), "w", encoding="utf-8") as f:
            json.dump(payload, f, ensure_ascii=False, separators=(",", ":"))
    except Exception:
        return


def _cache_file_for_game(key: str) -> Path:
    safe_key = str(key)
    return _cache_dir / f"gamecache_{_target_name}_{safe_key}.json"


def _safe_stat_signature(path: str) -> dict:
    try:
        st = os.stat(path)
        return {
            "path": str(path),
            "exists": True,
            "mtime": float(st.st_mtime),
            "size": int(st.st_size),
        }
    except OSError:
        return {"path": str(path), "exists": False, "mtime": 0.0, "size": 0}


def _manufacturer_id(value) -> int:
    return normalize_manufacturer_id(value, default=MANUFACTURER_NINTENDO)


def _build_game_signature(key: str, game_data: dict) -> dict:
    signature: dict = {
        "version": 1,
        "key": str(key),
        "target": str(_target_name),
        "export_dpi": int(_context.get("export_dpi", 0)),
        "size_scale": int(_context.get("size_scale", 1)),
        "resolution_up": list(_context.get("resolution_up", [0, 0])),
        "resolution_down": list(_context.get("resolution_down", [0, 0])),
        "console_size": list(_context.get("console_size", [0, 0])),
        "console_atlas_size": list(_context.get("console_atlas_size", [0, 0])),
        "tex3ds_enabled": bool(_context.get("tex3ds_enabled", True)),
        "texture_path_prefix": str(_context.get("texture_path_prefix", "")),
        "texture_path_ext": str(_context.get("texture_path_ext", "")),
        "game": {},
        "inputs": [],
    }

    game_opts = {
        "ref": str(game_data.get("ref", "")),
        "display_name": str(game_data.get("display_name", key)),
        "date": str(game_data.get("date", "")),
        "rotate": bool(game_data.get("rotate", _context.get("default_rotate", False))),
        "mask": bool(game_data.get("mask", False)),
        "color_segment": bool(game_data.get("color_segment", False)),
        "two_in_one_screen": bool(game_data.get("2_in_one_screen", False)),
        "alpha_bright": float(game_data.get("alpha_bright", _context.get("default_alpha_bright", 1.7))),
        "fond_bright": float(game_data.get("fond_bright", _context.get("default_fond_bright", 1.35))),
        "shadow": bool(game_data.get("shadow", True)),
        "background_in_front": bool(game_data.get("background_in_front", False)),
        "camera": bool(game_data.get("camera", False)),
        "transform_visual": game_data.get("transform_visual", []),
        "size_visual": game_data.get("size_visual", [signature["resolution_up"], signature["resolution_down"]]),
        "manufacturer": int(_manufacturer_id(game_data.get("manufacturer", 0))),
    }
    signature["game"] = game_opts

    rom_path = str(game_data.get("Rom", ""))
    melody_path = str(game_data.get("Melody_Rom", ""))
    visuals = list(game_data.get("Visual", []) or [])
    backgrounds = list(game_data.get("Background", []) or [])
    console_path = str(game_data.get("console", _context.get("default_console", r".\\rom\\default.png")))

    paths: list[str] = []
    if rom_path:
        paths.append(rom_path)
    if melody_path:
        paths.append(melody_path)
    paths.extend([str(p) for p in visuals if p])
    paths.extend([str(p) for p in backgrounds if p])
    if console_path:
        paths.append(console_path)

    signature["inputs"] = [_safe_stat_signature(p) for p in paths]
    return signature


def _expected_outputs_exist_for_game(key: str, game_data: dict) -> bool:
    out_cpp = os.path.join(str(_context.get("destination_game_file", "")), f"{key}.cpp")
    out_h = os.path.join(str(_context.get("destination_game_file", "")), f"{key}.h")
    if not (os.path.exists(out_cpp) and os.path.exists(out_h)):
        return False

    gfx_dir = str(_context.get("destination_graphique_file", ""))
    seg_png = os.path.join(gfx_dir, f"segment_{key}.png")
    seg_t3s = os.path.join(gfx_dir, f"segment_{key}.t3s")
    con_png = os.path.join(gfx_dir, f"console_{key}.png")
    con_t3s = os.path.join(gfx_dir, f"console_{key}.t3s")
    if not (os.path.exists(seg_png) and os.path.exists(seg_t3s) and os.path.exists(con_png) and os.path.exists(con_t3s)):
        return False

    mask = bool(game_data.get("mask", False))
    bg_paths = list(game_data.get("Background", []) or [])
    has_background = bool(bg_paths) and (str(bg_paths[0]) != "") and (not mask)
    if has_background:
        bg_png = os.path.join(gfx_dir, f"background_{key}.png")
        bg_t3s = os.path.join(gfx_dir, f"background_{key}.t3s")
        if not (os.path.exists(bg_png) and os.path.exists(bg_t3s)):
            return False

    return True


def _describe_cache_mismatch(old_sig: dict, new_sig: dict) -> list[str]:
    reasons: list[str] = []

    def add(label: str, a, b) -> None:
        if a != b:
            reasons.append(f"{label}: {a!r} -> {b!r}")

    for k in (
        "target",
        "export_dpi",
        "size_scale",
        "resolution_up",
        "resolution_down",
        "console_size",
        "console_atlas_size",
        "tex3ds_enabled",
        "texture_path_prefix",
        "texture_path_ext",
    ):
        add(k, old_sig.get(k), new_sig.get(k))

    old_game = old_sig.get("game") if isinstance(old_sig.get("game"), dict) else {}
    new_game = new_sig.get("game") if isinstance(new_sig.get("game"), dict) else {}
    for k in sorted(set(old_game.keys()) | set(new_game.keys())):
        add(f"game.{k}", old_game.get(k), new_game.get(k))

    old_inputs = old_sig.get("inputs") if isinstance(old_sig.get("inputs"), list) else []
    new_inputs = new_sig.get("inputs") if isinstance(new_sig.get("inputs"), list) else []
    if len(old_inputs) != len(new_inputs):
        reasons.append(f"inputs.count: {len(old_inputs)} -> {len(new_inputs)}")
    for i in range(min(len(old_inputs), len(new_inputs))):
        o = old_inputs[i] if isinstance(old_inputs[i], dict) else {}
        n = new_inputs[i] if isinstance(new_inputs[i], dict) else {}
        path = n.get("path") or o.get("path") or f"input[{i}]"
        if o.get("path") != n.get("path"):
            reasons.append(f"inputs[{i}].path: {o.get('path')!r} -> {n.get('path')!r}")
            continue
        if (o.get("exists"), o.get("size"), o.get("mtime")) != (n.get("exists"), n.get("size"), n.get("mtime")):
            reasons.append(
                f"inputs[{i}] changed: {path} (exists/size/mtime {o.get('exists')}/{o.get('size')}/{o.get('mtime')} -> {n.get('exists')}/{n.get('size')}/{n.get('mtime')})"
            )

    return reasons[:25]


def _describe_missing_outputs_for_game(key: str, game_data: dict) -> list[str]:
    missing: list[str] = []

    def check(p: str) -> None:
        if not os.path.exists(p):
            missing.append(p)

    out_dir = str(_context.get("destination_game_file", ""))
    gfx_dir = str(_context.get("destination_graphique_file", ""))

    check(os.path.join(out_dir, f"{key}.cpp"))
    check(os.path.join(out_dir, f"{key}.h"))

    check(os.path.join(gfx_dir, f"segment_{key}.png"))
    check(os.path.join(gfx_dir, f"segment_{key}.t3s"))
    check(os.path.join(gfx_dir, f"console_{key}.png"))
    check(os.path.join(gfx_dir, f"console_{key}.t3s"))

    mask = bool(game_data.get("mask", False))
    bg_paths = list(game_data.get("Background", []) or [])
    has_background = bool(bg_paths) and (str(bg_paths[0]) != "") and (not mask)
    if has_background:
        check(os.path.join(gfx_dir, f"background_{key}.png"))
        check(os.path.join(gfx_dir, f"background_{key}.t3s"))

    return missing
