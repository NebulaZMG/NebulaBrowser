#!/bin/bash
# Start Nebula Browser with GPU acceleration enabled on Linux
cd "$(dirname "$0")"
NEBULA_GPU_ALLOW_LINUX=1 npm start
