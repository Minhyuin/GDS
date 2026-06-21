import numpy as np

from ..protocol.constants import MAX_VOLTAGE, MIN_VOLTAGE, PERIOD_SAMPLE_COUNT


def generate_waveform(kind, points=PERIOD_SAMPLE_COUNT, center=1.65, amplitude=1.0, phase_deg=0.0):
    count = int(points)
    if count < 1:
        raise ValueError("points must be positive")
    phase = np.deg2rad(float(phase_deg))
    t = np.linspace(0.0, 1.0, count, endpoint=False)
    name = str(kind).strip().lower()

    if name == "sine":
        values = center + amplitude * np.sin(2.0 * np.pi * t + phase)
    elif name == "cosine":
        values = center + amplitude * np.cos(2.0 * np.pi * t + phase)
    elif name == "square":
        values = center + amplitude * np.where(np.sin(2.0 * np.pi * t + phase) >= 0.0, 1.0, -1.0)
    elif name == "triangle":
        shifted = (t + phase / (2.0 * np.pi)) % 1.0
        values = center + amplitude * (4.0 * np.abs(shifted - 0.5) - 1.0)
    elif name == "constant":
        values = np.full(count, float(center))
    else:
        raise ValueError(f"Unsupported waveform kind: {kind!r}")

    return np.clip(values, MIN_VOLTAGE, MAX_VOLTAGE).astype(float)
