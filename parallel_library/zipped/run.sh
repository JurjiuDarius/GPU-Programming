#!/bin/bash
# =============================================================================
# Run script for User-Level Thread Library
# =============================================================================
#
# This script builds and runs the thread library in a Docker container
# because ucontext functions don't work reliably on macOS (especially Apple Silicon)
#
# USAGE:
#   ./run.sh          - Build and run tests
#   ./run.sh shell    - Open interactive shell in container
#   ./run.sh clean    - Remove Docker image
#
# =============================================================================

IMAGE_NAME="ult-threads"

case "$1" in
    shell)
        echo "Opening interactive shell in container..."
        docker build -t $IMAGE_NAME . && docker run -it --rm $IMAGE_NAME /bin/bash
        ;;
    clean)
        echo "Removing Docker image..."
        docker rmi $IMAGE_NAME 2>/dev/null || true
        echo "Done."
        ;;
    *)
        echo "Building and running thread library tests..."
        echo ""
        docker build -t $IMAGE_NAME . && docker run --rm $IMAGE_NAME
        ;;
esac
