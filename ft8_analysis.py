import numpy as np
import matplotlib.pyplot as plt
from scipy import signal
from scipy.fft import fft, fftfreq

def plot_ft8(data, fs):
    """Plot FT8 signal - time, frequency, and spectrogram"""
    t = np.arange(len(data)) / fs
    f = fftfreq(len(data), 1/fs)[:len(data)//2]
    fft_data = np.abs(fft(data))[:len(data)//2]
    
    fig, (ax1, ax2, ax3) = plt.subplots(3, 1, figsize=(10, 8))
    
    # Time domain
    ax1.plot(t, data)
    ax1.set_title('Time Domain')
    ax1.set_xlabel('Time (s)')
    
    # Frequency domain
    ax2.plot(f, 20*np.log10(fft_data + 1e-10))
    ax2.set_title('Frequency Domain')
    ax2.set_xlabel('Frequency (Hz)')
    ax2.set_ylabel('dB')
    
    # Spectrogram
    f_spec, t_spec, Sxx = signal.spectrogram(data, fs, nperseg=256)
    ax3.pcolormesh(t_spec, f_spec, 10*np.log10(Sxx + 1e-10))
    ax3.set_title('Spectrogram')
    ax3.set_xlabel('Time (s)')
    ax3.set_ylabel('Frequency (Hz)')
    
    plt.tight_layout()
    plt.show()

def check_ft8(data, fs):
    """Quick FT8 verification"""
    duration = len(data) / fs
    fft_data = np.abs(fft(data))
    peak_freq = fftfreq(len(data), 1/fs)[np.argmax(fft_data[:len(data)//2])]
    
    print(f"Duration: {duration:.2f}s (should be ~12.64s)")
    print(f"Peak freq: {peak_freq:.0f}Hz (should be 200-4000Hz)")
    print(f"Samples: {len(data)}")

# Load your data file
data = np.fromfile("ft8_output.dat", dtype=np.float32) 
sample_rate = 48000

# plot_ft8(data, sample_rate)
# check_ft8(data, sample_rate)

# Load the data
print(f"Loaded {len(data)} samples")
print(f"Data range: {data.min():.3f} to {data.max():.3f}")
print(f"Mean: {data.mean():.6f}")

# Check if it's all zeros
if np.all(data == 0):
    print("ERROR: All samples are zero!")
else:
    print("Data looks good - has non-zero values")

# Quick plot
plt.figure(figsize=(12, 4))
plt.plot(data[:10000])  # Plot first 10k samples
plt.title("First 10,000 samples")
plt.show()

# Now try the analysis with correct sample rate
# sample_rate = 48000
# plot_ft8(data, sample_rate)
# check_ft8(data, sample_rate)