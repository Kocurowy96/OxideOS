"""
OxideOS - Startup Chime Synthesizer (Classic Chime Motif)
Author: Assistant & You
Description: Generates a warm, clean, 16-bit PCM 44100Hz startup chime
             with a classic "dun... dyn, dyn, dyn" melodic progression.
Dependencies: numpy (pip install numpy)
"""

import wave
import struct
import math

# Audio configuration parameters
SAMPLE_RATE = 44100  # 44.1 kHz CD-quality clean audio
DURATION_SEC = 4.0   # Length of the startup chime
NUM_SAMPLES = int(SAMPLE_RATE * DURATION_SEC)

# Melodic frequencies for the "dun... dyn, dyn, dyn" motif (e.g., C Major chord progression)
# Note 1 (Dun...): Low, sustaining root note (C4)
# Notes 2-4 (dyn, dyn, dyn): Rising uplifting notes (E4 -> G4 -> C5)
NOTE_C4 = 261.63
NOTE_E4 = 329.63
NOTE_G4 = 392.00
NOTE_C5 = 523.25

def get_note_envelope(t, start_time, duration):
    """Calculates the envelope (volume shape) for an individual note at time t."""
    if t < start_time or t > (start_time + duration):
        return 0.0

    # Relative time inside the note
    rel_t = t - start_time

    # Quick attack, smooth exponential decay
    attack = 0.05
    if rel_t < attack:
        return rel_t / attack
    else:
        # Decay over the rest of the note duration
        decay_progress = (rel_t - attack) / (duration - attack)
        return max(0.0, math.exp(-3.0 * decay_progress))

def generate_sample(t):
    """Generates the audio sample value at time t (in seconds) for our motif."""
    # Master global envelope (fades out gracefully at the very end)
    if t < 0.05:
        global_env = t / 0.05
    elif t > (DURATION_SEC - 0.5):
        global_env = (DURATION_SEC - t) / 0.5
    else:
        global_env = 1.0

    wave_val = 0.0

    # 1. The "Dun..." (Long, deep foundational tone starting at 0.0s)
    env_dun = get_note_envelope(t, start_time=0.0, duration=2.5)
    wave_val += 0.5 * math.sin(2 * math.pi * NOTE_C4 * t) * env_dun
    wave_val += 0.25 * math.sin(2 * math.pi * (NOTE_C4 / 2) * t) * env_dun  # Sub octave warmth

    # 2. The "dyn, dyn, dyn" (Rapid succession of bright, bell-like chords rising up)
    # First dyn (E4) at 0.8s
    env_dyn1 = get_note_envelope(t, start_time=0.8, duration=1.2)
    wave_val += 0.4 * math.sin(2 * math.pi * NOTE_E4 * t) * env_dyn1

    # Second dyn (G4) at 1.2s
    env_dyn2 = get_note_envelope(t, start_time=1.2, duration=1.4)
    wave_val += 0.45 * math.sin(2 * math.pi * NOTE_G4 * t) * env_dyn2

    # Third dyn (C5 - high triumphant finish) at 1.7s sustaining to the end
    env_dyn3 = get_note_envelope(t, start_time=1.7, duration=2.3)
    wave_val += 0.5 * math.sin(2 * math.pi * NOTE_C5 * t) * env_dyn3
    wave_val += 0.25 * math.sin(2 * math.pi * NOTE_E4 * t) * env_dyn3

    # Apply global envelope, add a touch of warmth harmonics, and clamp
    final_val = wave_val * global_env * 0.6
    return max(-1.0, min(1.0, final_val))

print("[*] Synthesizing OxideOS Startup Chime ('dun... dyn, dyn, dyn')...")
audio_data = []

for i in range(NUM_SAMPLES):
    t = i / SAMPLE_RATE
    sample = generate_sample(t)

    # Convert float (-1.0 to 1.0) to 16-bit signed integer (-32768 to 32767)
    scaled_sample = int(sample * 32767)
    audio_data.append(scaled_sample)

output_filename = "oxide_startup.wav"
with wave.open(output_filename, 'w') as wav_file:
    n_channels = 2  # Stereo
    sampwidth = 2   # 16-bit = 2 bytes
    framerate = SAMPLE_RATE
    n_frames = NUM_SAMPLES
    comptype = "NONE"
    compname = "not compressed"

    wav_file.setparams((n_channels, sampwidth, framerate, n_frames, comptype, compname))

    # Write interleaved stereo frames
    for sample in audio_data:
        packed_sample = struct.pack('<hh', sample, sample)
        wav_file.writeframes(packed_sample)

print(f"[+] Success! Generated '{output_filename}' (16-bit PCM, {SAMPLE_RATE}Hz, Stereo).")
