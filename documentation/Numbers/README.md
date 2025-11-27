# Sollya Polynomial Approximation Setup Guide

Generate optimal polynomial approximations using Sollya for functions like `sin(x*π/2)` or
cos(x) .

## Prerequisites

### macOS
- [Homebrew](https://brew.sh/) package manager
- Command line tools: `xcode-select --install`

### Linux (Ubuntu/Debian)
- Package manager access (`apt`)
- Build essentials: `sudo apt update && sudo apt install build-essential`

### Windows
- Windows 10 2004+ or Windows 11
- PowerShell with Administrator access

## Installation

### macOS
```bash
# Install Sollya via Homebrew
brew install sollya

# Verify installation
sollya --version
```

### Linux (Ubuntu/Debian)
```bash
# Update package list
sudo apt update

# Install Sollya
sudo apt install sollya

# Verify installation  
sollya --version
```

### Windows (via WSL)
```powershell
# 1. Install WSL (run as Administrator in PowerShell)
wsl --install

# 2. Restart your computer when prompted
# 3. Set up Ubuntu username/password when prompted
# 4. Install Sollya inside WSL
sudo apt update
sudo apt install sollya
```

## Usage

### Simple Script

Check the various scripts for functionality


### Interactive Usage
```bash
# Start Sollya interactive session
sollya

# In Sollya prompt:
f = sin(x * pi/2);
p = remez(f, 3, [-1, 1]);
p;
dirtyinfnorm(f - p, [-1, 1]);
quit;
```

### Custom Functions
```bash
# Different functions and degrees
sollya -c "
f = exp(x);                    # Exponential
p = remez(f, 4, [0, 1]);      # Degree 4 on [0,1]
write(p); 
quit;
"
```
