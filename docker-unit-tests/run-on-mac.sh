#!/bin/bash

ARCH=$(uname -m)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Map architecture to Docker architecture and image name
if [[ "$ARCH" == "arm64" ]] || [[ "$ARCH" == "aarch64" ]]; then
    DOCKER_ARCH="aarch64"
    DOCKER_IMAGE="valgrind-img-arm64:latest"
elif [[ "$ARCH" == "x86_64" ]]; then
    DOCKER_ARCH="x86_64"
    DOCKER_IMAGE="valgrind-img-x86:latest"
else
    echo "Unsupported architecture: $ARCH"
    exit 1
fi

echo "Detected architecture: $ARCH"
echo "Building Docker image: $DOCKER_IMAGE"

# Build the appropriate Docker image
echo "Building Docker image for $DOCKER_ARCH..."
docker build -f "$SCRIPT_DIR/$DOCKER_ARCH/Dockerfile.valgrind" -t "$DOCKER_IMAGE" "$SCRIPT_DIR/$DOCKER_ARCH"

if [ $? -ne 0 ]; then
    echo "Failed to build Docker image"
    exit 1
fi

echo "Docker image built successfully: $DOCKER_IMAGE"

function runOnDocker() {
    docker rm -f abacdsp
    docker run -d -it --name abacdsp -v "$(pwd)"/..:/root/project "$DOCKER_IMAGE" bash
    docker ps -a
    docker exec -it abacdsp /root/project/docker-unit-tests/build-tests.sh
    docker exec -it abacdsp /root/project/docker-unit-tests/run-tests.sh
}

runOnDocker

echo "Hints for debugging and checking logs: "
echo "# docker exec -it abacdsp bash"
echo "# - logs: cat /build/Testing/Temporary/Memory*"
echo "# - /root/project/docker-unit-tests/build-tests.sh"
