#!/usr/bin/env python3

import numpy as np
from scipy.optimize import curve_fit
import matplotlib.pyplot as plt

# Your data
frequencies = np.array([
                        130.813 ,
                        261.626 ,
                        523.251 ,
                        1046.5  ,
                        2093    ,
                        4186.01 ,
                        8372.02 ])
raw_values = np.array([
                       0.0138542    ,
                       0.00696405   ,
                       0.00348882   ,
                       0.00174439   ,
                       0.000864921  ,
                       0.000420362  ,
                       0.000186432 ])

# Normalized compensation (1 / raw_value)
compensation = 1.0 / raw_values

# Define the fitting function
def compensation_model(freq, a, b, c):
    log_freq = np.log(freq)
    log_f = a + b * log_freq + c * log_freq * log_freq
    return 1000 * np.exp(log_f)

# Fit the parameters
popt, pcov = curve_fit(compensation_model, frequencies, compensation)
a_fit, b_fit, c_fit = popt

print(f"Fitted coefficients:")
print(f"constexpr auto a = {a_fit:.8f};")
print(f"constexpr auto b = {b_fit:.8f};")
print(f"constexpr auto c = {c_fit:.8f};")

# Calculate residuals and R²
predicted = compensation_model(frequencies, *popt)
residuals = compensation - predicted
ss_res = np.sum(residuals**2)
ss_tot = np.sum((compensation - np.mean(compensation))**2)
r_squared = 1 - (ss_res / ss_tot)

print(f"\nFit quality:")
print(f"R² = {r_squared:.6f}")
print(f"Max error: {np.max(np.abs(residuals)):.6f}")

# Plot
plt.figure(figsize=(10, 6))
plt.semilogx(frequencies, compensation, 'o', label='Data', markersize=8)
freq_smooth = np.logspace(np.log10(frequencies[0]), np.log10(frequencies[-1]), 200)
plt.semilogx(freq_smooth, compensation_model(freq_smooth, *popt), '-', label='Fit')
plt.xlabel('Frequency (Hz)')
plt.ylabel('Compensation Factor')
plt.legend()
plt.grid(True, alpha=0.3)
plt.title(f'Compensation Fit (R² = {r_squared:.4f})')
plt.show()
