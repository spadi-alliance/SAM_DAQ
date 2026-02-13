# SAM DAQ Scripting Guide

## Overview
The SAM DAQ CLI now supports script execution for automated data acquisition workflows. You can create script files containing a series of CLI commands and execute them using the `--script` command-line option.

## Usage
```bash
# Execute a script file
samdaq --script your_script.txt

# Combine with other options
samdaq --script script.txt --ip 192.168.1.200 --output-dir ./results
```

## Script File Format
Script files are plain text files with one command per line. Features include:

- **Comments**: Lines starting with `#` are ignored
- **Empty lines**: Ignored for readability
- **Command syntax**: Same as interactive CLI commands
- **Error handling**: Script stops on command errors

## Example Script
```bash
# Basic acquisition script
ip 192.168.1.100
output-dir ./data
output-file test_run

connect
power on
trigger self
start
stop
disconnect
quit
```

## Available Commands
All interactive CLI commands are supported in scripts:
- `connect`, `disconnect`, `ping`
- `power on|off`
- `trigger self|external|1khz|1mhz`
- `start`, `stop`
- `output-dir <path>`, `output-file <name>`
- `ip <address>`
- And many more...


## Troubleshooting
- Check file permissions on script files
- Verify file paths are correct
- Test individual commands interactively if script fails
- Use absolute paths for output directories when possible
