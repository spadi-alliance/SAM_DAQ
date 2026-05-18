unset ROOTSYS ROOT_INCLUDE_PATH ROOT_LIBRARY_PATH
export LD_LIBRARY_PATH=$(echo "$LD_LIBRARY_PATH" | tr ':' '\n' | grep -v '/home/daq/artemis' | paste -sd: -)
