# SAM DAQ System

A Qt5/ROOT-based Data Acquisition system for SAMIDARE BOARD.

## Prerequisites

- [Pixi](https://pixi.sh/) package manager

## Quick Setup

### 1. Install Dependencies

```bash
# Install environment from pixi.toml
pixi install
```

### 2. Build the Project

```bash
# Build using the pixi task
pixi run build
```

### 3. Run the Application

```bash
# Activate pixi environment and run GUI
pixi shell
./build/SAMDAQ

# Or run CLI directly in environment
pixi run ./build/SAMDAQ --help
```


### Output Formats
- **ROOT TTree**: With timestamp branches
- **HTTP Server**: Real-time data serving
- **Waveform Display**: Live visualization


## Troubleshooting

### Build Issues

1. **Library conflicts**: Ensure you're in the pixi environment
   ```bash
   pixi shell
   which cmake  # Should point to pixi cmake
   ```

2. **Clean build**: Remove build directory and reconfigure
   ```bash
   rm -rf build && pixi run build
   ```

3. **Missing dependencies**: Reinstall environment
   ```bash
   pixi install --force
   ```

### Runtime Issues

1. **Qt display issues**: Check X11 forwarding if using SSH
2. **ROOT not found**: Verify `root-config` is accessible in pixi environment
3. **Permission errors**: Check hardware device permissions
