@echo off
setlocal enabledelayedexpansion

:: Get architecture
for /f "tokens=3" %%i in ('wmic os get osarchitecture /value ^| find "="') do set ARCH=%%i

if "%ARCH%"=="64-bit" (
    set DOCKER_ARCH=x86_64
    set DOCKER_IMAGE=valgrind-img-x86:latest
) else (
    echo Unsupported architecture: %ARCH%
    exit /b 1
)

echo Detected architecture: %ARCH%
echo Building Docker image: %DOCKER_IMAGE%

:: Get script directory
set SCRIPT_DIR=%~dp0

:: Build the Docker image
echo Building Docker image for %DOCKER_ARCH%...
docker build -f "%SCRIPT_DIR%%DOCKER_ARCH%\Dockerfile.valgrind" -t "%DOCKER_IMAGE%" "%SCRIPT_DIR%%DOCKER_ARCH%"

if %errorlevel% neq 0 (
    echo Failed to build Docker image
    exit /b 1
)

echo Docker image built successfully: %DOCKER_IMAGE%

:: Run the container
docker rm -f abacdsp

docker run -d -it --name abacdsp -v "%cd%\..:/root/project" %DOCKER_IMAGE% bash

docker ps -a

docker exec -it abacdsp /root/project/docker-unit-tests/build-tests.sh

docker exec -it abacdsp /root/project/docker-unit-tests/run_unit_tests.sh

echo Hints for debugging and checking logs:
echo # docker exec -it abacdsp bash
echo # - logs: cat /build/Testing/Temporary/Memory*
echo # - /root/project/docker-unit-tests/build-tests.sh
