from .diagnostics import Diagnostics

def _diag(diag: Diagnostics | None, op: str, inputs, result, label: str = "", strict: bool = False) -> None:
    if diag is not None:
        diag.record(op, inputs, result, label, strict)

def u8(value: int, diagnostics: Diagnostics | None = None, label: str = "") -> int:
    result = int(value) & 0xFF
    if result != value:
        _diag(diagnostics, "u8_wrap", (value,), result, label)
    return result

def u16(value: int, diagnostics: Diagnostics | None = None, label: str = "") -> int:
    result = int(value) & 0xFFFF
    if result != value:
        _diag(diagnostics, "u16_wrap", (value,), result, label)
    return result

def s8(value: int, diagnostics: Diagnostics | None = None, label: str = "") -> int:
    raw = int(value) & 0xFF
    result = raw - 256 if raw & 0x80 else raw
    if result != value:
        _diag(diagnostics, "signed_wrap", (value,), result, label)
    return result

def s16(value: int, diagnostics: Diagnostics | None = None, label: str = "") -> int:
    raw = int(value) & 0xFFFF
    result = raw - 65536 if raw & 0x8000 else raw
    if result != value:
        _diag(diagnostics, "signed_wrap", (value,), result, label)
    return result

def qadd8(a: int, b: int, diagnostics: Diagnostics | None = None, label: str = "") -> int:
    a8 = u8(a, diagnostics, f"{label}: a" if label else "qadd8: a")
    b8 = u8(b, diagnostics, f"{label}: b" if label else "qadd8: b")
    total = a8 + b8
    result = 255 if total > 255 else total
    if total > 255:
        _diag(diagnostics, "saturating_add", (a8, b8), result, label)
    return result

def qsub8(a: int, b: int, diagnostics: Diagnostics | None = None, label: str = "") -> int:
    a8 = u8(a, diagnostics, f"{label}: a" if label else "qsub8: a")
    b8 = u8(b, diagnostics, f"{label}: b" if label else "qsub8: b")
    result = 0 if b8 > a8 else a8 - b8
    if b8 > a8:
        _diag(diagnostics, "saturating_sub", (a8, b8), result, label)
    return result

def clamp_u8(value: int, diagnostics: Diagnostics | None = None, label: str = "") -> int:
    result = max(0, min(255, int(value)))
    if result != value:
        _diag(diagnostics, "clamped_value", (value,), result, label, False)
    return result

def clamp_u16(value: int, diagnostics: Diagnostics | None = None, label: str = "") -> int:
    result = max(0, min(65535, int(value)))
    if result != value:
        _diag(diagnostics, "clamped_value", (value,), result, label, False)
    return result

def require_u8(value: int, diagnostics: Diagnostics | None = None, label: str = "") -> int:
    valid = isinstance(value, int) and 0 <= value <= 255
    if not valid:
        _diag(diagnostics, "invalid_u8", (value,), None, label, True)
        return clamp_u8(int(value) if isinstance(value, int) else 0, diagnostics, label)
    return value

def require_rgb(rgb, diagnostics: Diagnostics | None = None, label: str = "") -> tuple[int, int, int]:
    valid = isinstance(rgb, tuple) and len(rgb) == 3 and all(isinstance(c, int) and 0 <= c <= 255 for c in rgb)
    if not valid:
        _diag(diagnostics, "invalid_rgb_write", (rgb,), None, label, True)
        if not isinstance(rgb, tuple) or len(rgb) != 3:
            return (0, 0, 0)
        return tuple(clamp_u8(c if isinstance(c, int) else 0, diagnostics, label) for c in rgb)  # type: ignore[return-value]
    return rgb

def div_trunc(n: int, d: int, diagnostics: Diagnostics | None = None, label: str = "") -> int:
    if d == 0:
        raise ZeroDivisionError(label or "div_trunc")
    result = abs(n) // abs(d)
    if (n < 0) ^ (d < 0):
        result = -result
    if n != result * d:
        _diag(diagnostics, "truncating_division", (n, d), result, label)
    return result

def scale8(value: int, scale: int, diagnostics: Diagnostics | None = None, label: str = "") -> int:
    """FastLED fixed scale8 semantics (FASTLED_SCALE8_FIXED=1): (value*scale + value) >> 8."""
    value8 = u8(value, diagnostics, f"{label}: value" if label else "scale8: value")
    scale8_value = u8(scale, diagnostics, f"{label}: scale" if label else "scale8: scale")
    return (value8 * scale8_value + value8) >> 8

def lerp8(a: int, b: int, amount: int, diagnostics: Diagnostics | None = None, label: str = "") -> int:
    """Simulator helper with uint8 inputs and C++-style truncation; not a claimed FastLED byte-for-byte port."""
    a8 = u8(a, diagnostics, f"{label}: a" if label else "lerp8: a")
    b8 = u8(b, diagnostics, f"{label}: b" if label else "lerp8: b")
    amount8 = u8(amount, diagnostics, f"{label}: amount" if label else "lerp8: amount")
    return clamp_u8(a8 + div_trunc((b8 - a8) * amount8, 255, diagnostics, label or "lerp8"), diagnostics, label)
