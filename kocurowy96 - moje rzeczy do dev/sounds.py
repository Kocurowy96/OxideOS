"""
OxideOS - System Sounds Synthesizer (Complete Suite)
Author: Assistant & You
Description: Generates retro system sounds (Error, Warning, Info, Click, Logout,
             Notify, Bin, Window operations) as 16-bit PCM 44100Hz .wav files.
Dependencies: numpy (pip install numpy)
"""

import wave
import struct
import math
import random

SAMPLE_RATE = 44100

def save_wav(filename, samples):
    """Utility to save a list of float samples (-1.0 to 1.0) to a 16-bit stereo WAV file."""
    with wave.open(filename, 'w') as wav_file:
        wav_file.setparams((2, 2, SAMPLE_RATE, len(samples), "NONE", "not compressed"))
        for sample in samples:
            # Clamp values to -1.0 and 1.0 to prevent clipping noise
            scaled = int(max(-1.0, min(1.0, sample)) * 32767)
            wav_file.writeframes(struct.pack('<hh', scaled, scaled))
    print(f"[+] Generated: {filename}")

def generate_error_sound():
    """Generates a classic low, double-tone error/critical sound (thud-thud)."""
    duration = 0.35
    num_samples = int(SAMPLE_RATE * duration)
    samples = []

    for i in range(num_samples):
        t = i / SAMPLE_RATE
        env = math.exp(-6.0 * t)
        f1 = 110.0  # A2
        f2 = 116.54 # A#2
        val = 0.5 * (math.sin(2 * math.pi * f1 * t) + math.sin(2 * math.pi * f2 * t))
        if t > 0.1:
            t2 = t - 0.1
            env2 = math.exp(-8.0 * t2)
            val += 0.4 * math.sin(2 * math.pi * 82.41 * t2) * env2
        samples.append(val * env * 0.7)

    return samples

def generate_warning_sound():
    """Generates a polite, single medium-pitch warning tone."""
    duration = 0.25
    num_samples = int(SAMPLE_RATE * duration)
    samples = []

    for i in range(num_samples):
        t = i / SAMPLE_RATE
        env = math.exp(-5.0 * t)
        val = 0.6 * math.sin(2 * math.pi * 440.0 * t) + 0.3 * math.sin(2 * math.pi * 880.0 * t)
        samples.append(val * env * 0.5)

    return samples

def generate_info_sound():
    """Generates a crisp, uplifting two-tone chime for information/notifications."""
    duration = 0.3
    num_samples = int(SAMPLE_RATE * duration)
    samples = []

    for i in range(num_samples):
        t = i / SAMPLE_RATE
        env = math.exp(-6.0 * t)
        if t < 0.15:
            val = math.sin(2 * math.pi * 523.25 * t) # C5
        else:
            t_sub = t - 0.15
            val = math.sin(2 * math.pi * 783.99 * t_sub) # G5
        samples.append(val * env * 0.5)

    return samples

def generate_click_sound():
    """Generates a clean, distinctly audible UI click sound."""
    duration = 0.08  # Lengthened so players like VLC register it properly
    num_samples = int(SAMPLE_RATE * duration)
    samples = []

    for i in range(num_samples):
        t = i / SAMPLE_RATE
        env = math.exp(-15.0 * t)
        val = 0.5 * math.sin(2 * math.pi * 800.0 * t) + 0.3 * math.sin(2 * math.pi * 1600.0 * t)
        samples.append(val * env * 0.4)

    return samples

def generate_logout_sound():
    """Generates a descending, peaceful logout/shutdown chime (inverse of startup)."""
    duration = 3.0
    num_samples = int(SAMPLE_RATE * duration)
    samples = []

    NOTE_C5 = 523.25
    NOTE_G4 = 392.00
    NOTE_E4 = 329.63
    NOTE_C4 = 261.63

    for i in range(num_samples):
        t = i / SAMPLE_RATE
        if t > (duration - 0.4):
            global_env = (duration - t) / 0.4
        else:
            global_env = 1.0

        val = 0.0
        if t < 1.0:
            env1 = math.exp(-3.0 * t)
            val += 0.4 * math.sin(2 * math.pi * NOTE_C5 * t) * env1
        if t >= 0.4 and t < 1.8:
            t_sub = t - 0.4
            env2 = math.exp(-2.5 * t_sub)
            val += 0.35 * math.sin(2 * math.pi * NOTE_G4 * t_sub) * env2
        if t >= 0.9 and t < 2.5:
            t_sub = t - 0.9
            env3 = math.exp(-2.0 * t_sub)
            val += 0.3 * math.sin(2 * math.pi * NOTE_E4 * t_sub) * env3
        if t >= 1.4:
            t_sub = t - 1.4
            env4 = math.exp(-1.8 * t_sub)
            val += 0.4 * math.sin(2 * math.pi * NOTE_C4 * t_sub) * env4

        samples.append(val * global_env * 0.5)

    return samples

def generate_notify_sound():
    """Generates a soft, pleasant two-tone 'plum-plum' notification."""
    duration = 0.4
    num_samples = int(SAMPLE_RATE * duration)
    samples = []

    for i in range(num_samples):
        t = i / SAMPLE_RATE
        env = math.exp(-4.0 * t)
        if t < 0.1:
            val = math.sin(2 * math.pi * 783.99 * t) # G5
        else:
            t_sub = t - 0.1
            val = math.sin(2 * math.pi * 1046.50 * t_sub) # C6
        samples.append(val * env * 0.3)

    return samples

def generate_add_bin_sound():
    """Generates a short, crunchy sound simulating throwing paper away."""
    duration = 0.25
    num_samples = int(SAMPLE_RATE * duration)
    samples = []

    for i in range(num_samples):
        t = i / SAMPLE_RATE
        env = math.exp(-8.0 * t)
        # White noise burst
        noise = random.uniform(-1.0, 1.0)
        # Modulated to sound "chunky/crunchy"
        crunch = math.sin(2 * math.pi * 30.0 * t)
        val = noise * abs(crunch)
        samples.append(val * env * 0.25)

    return samples

def generate_empty_bin_sound():
    """Generates a longer digital swoosh/crunch for emptying the recycle bin."""
    duration = 0.5
    num_samples = int(SAMPLE_RATE * duration)
    samples = []

    for i in range(num_samples):
        t = i / SAMPLE_RATE
        env = math.exp(-4.0 * t)
        noise = random.uniform(-1.0, 1.0)
        # Sweeping the crunch frequency downwards
        freq = 50.0 - (40.0 * (t / duration))
        crunch = math.sin(2 * math.pi * freq * t)
        val = noise * abs(crunch)
        samples.append(val * env * 0.3)

    return samples

def generate_maximize_sound():
    """Generates a quick ascending 'swish' for maximizing windows."""
    duration = 0.15
    num_samples = int(SAMPLE_RATE * duration)
    samples = []

    for i in range(num_samples):
        t = i / SAMPLE_RATE
        env = math.exp(-4.0 * t)
        # Frequency sweeps up from 300Hz to 900Hz
        freq = 300.0 + (600.0 * (t / duration))
        val = math.sin(2 * math.pi * freq * t)
        samples.append(val * env * 0.2)

    return samples

def generate_minimize_sound():
    """Generates a quick descending 'swish' for minimizing windows."""
    duration = 0.15
    num_samples = int(SAMPLE_RATE * duration)
    samples = []

    for i in range(num_samples):
        t = i / SAMPLE_RATE
        env = math.exp(-4.0 * t)
        # Frequency sweeps down from 900Hz to 300Hz
        freq = 900.0 - (600.0 * (t / duration))
        val = math.sin(2 * math.pi * freq * t)
        samples.append(val * env * 0.2)

    return samples

print("[*] Synthesizing OxideOS System Sounds Suite...")
save_wav("oxide_error.wav", generate_error_sound())
save_wav("oxide_warning.wav", generate_warning_sound())
save_wav("oxide_info.wav", generate_info_sound())
save_wav("oxide_click.wav", generate_click_sound())
save_wav("oxide_logout.wav", generate_logout_sound())
save_wav("oxide_notify.wav", generate_notify_sound())
save_wav("oxide_add_bin.wav", generate_add_bin_sound())
save_wav("oxide_empty_bin.wav", generate_empty_bin_sound())
save_wav("oxide_maximize.wav", generate_maximize_sound())
save_wav("oxide_minimize.wav", generate_minimize_sound())
print("[+] All system audio assets (including UI swishes and bin sounds) generated successfully!")
