#!/bin/bash

# Display a warning message
echo "THIS SCRIPT IS IN A PRE-ALPHA STAGE. RUNNING IT MAY DAMAGE YOUR OPERATING SYSTEM INSTALLATION. Co-created with ChatGPT."
read -p "Do you wish to proceed? (yes/no): " choice
if [[ "$choice" != "yes" ]]; then
  echo "Exiting script."
  exit 0
fi

# Function to display an error and exit
error_exit() {
  echo "$1" >&2
  exit 1
}

# Ensure CUDA is installed and up-to-date
CUDA_VERSION=$(nvcc --version | grep release | awk '{print $6}' | cut -d ',' -f 1)
REQUIRED_CUDA_VERSION="12.0"
if [[ -z "$CUDA_VERSION" ]]; then
  error_exit "CUDA is not installed or not in your PATH. Please install CUDA $REQUIRED_CUDA_VERSION or newer."
elif [[ $(echo "$CUDA_VERSION $REQUIRED_CUDA_VERSION" | awk '{print ($1 >= $2)}') -ne 1 ]]; then
  error_exit "CUDA version $CUDA_VERSION is outdated. Please update to version $REQUIRED_CUDA_VERSION or newer."
fi

# Ensure Docker and nvidia-docker are installed
DOCKER_VERSION=$(docker --version)
if [[ $? -ne 0 ]]; then
  error_exit "Docker is not installed. Please install Docker and ensure it is in your PATH."
fi

# Ensure GPU drivers are up-to-date
NVIDIA_DRIVER_VERSION=$(nvidia-smi --query-gpu=driver_version --format=csv,noheader)
REQUIRED_DRIVER_VERSION="520.56.06"
if [[ $(echo "$NVIDIA_DRIVER_VERSION $REQUIRED_DRIVER_VERSION" | awk '{print ($1 >= $2)}') -ne 1 ]]; then
  error_exit "Nvidia driver version $NVIDIA_DRIVER_VERSION is outdated. Please update to version $REQUIRED_DRIVER_VERSION or newer."
fi

# Ensure Xorg display is not running
XORG_RUNNING=$(nvidia-smi | grep "No running processes found")
if [[ -z "$XORG_RUNNING" ]]; then
  error_exit "Xorg display server is running on your Nvidia GPU. Please stop it and try again."
fi

# Ensure nvidia-drm is loaded with modeset=1
NVIDIA_DRM_MODESET=$(cat /sys/module/nvidia_drm/parameters/modeset)
if [[ "$NVIDIA_DRM_MODESET" -ne 1 ]]; then
  error_exit "nvidia-drm module is not loaded with modeset=1. Please ensure the module is loaded correctly."
fi

# Step 1: Navigate to the game directory
echo "Navigating to the game directory..."
cd "$HOME/.steam/steam/steamapps" || error_exit "Failed to change directory to $HOME/.steam/steam/steamapps"
ls -la .

# Step 2: Generate a session ID
echo "Generating a session ID..."
SESSION_ID=$(head /dev/urandom | LC_ALL=C tr -dc 'a-zA-Z0-9' | head -c 16)
echo "Session ID: $SESSION_ID"

# Step 3: Launch the Nestri server
echo "Launching Nestri server..."
docker run --gpus all --device=/dev/dri --name nestri -it --entrypoint /bin/bash -e SESSION_ID="$SESSION_ID" -v "$(pwd)":/game -p 8080:8080/udp --cap-add=SYS_NICE --cap-add=SYS_ADMIN ghcr.io/nestriness/nestri/server:nightly || error_exit "Failed to launch the Nestri server."

# Step 4: Configure the game within the container
echo "Configuring the game within the container..."
docker exec -it nestri bash -c "ls -la /game && /etc/startup.sh > /dev/null &" || error_exit "Failed to configure the game within the container."

# Step 5: Wait for the X11 directory and run the game
echo "Waiting for /tmp/.X11-unix directory to appear..."
while [ ! -d /tmp/.X11-unix ]; do
  sleep 1
done

# Running the game
echo "Running the game..."
read -p "Enter the name of your game executable (e.g., game.exe): " GAME_EXE
docker exec -it nestri bash -c "nestri-proton -pr $GAME_EXE" || error_exit "Failed to run the game."

# Step 6: Provide the play URL
echo "Your game is ready to play!"
echo "Navigate to the following URL in your browser:"
echo "https://nestri.io/play/$SESSION_ID"
