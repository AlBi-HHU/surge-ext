#!/bin/bash

# Exit immediately if a command exits with a non-zero status
set -e

echo "------------------------------------------"
echo "  Surge-Ext Build & Deployment Manager    "
echo "------------------------------------------"

# Define the menu options
PS3='Please enter your choice (1, 2, or 3): '
options=("Local Development Build" "Docker Image Build" "Quit")

select opt in "${options[@]}"
do
    case $opt in
        "Local Development Build")
            echo -e "\n[!] Starting local development setup..."
            
            # Prepare and run setup
            if [ -f "./setup.sh" ]; then
                chmod +x setup.sh
                ./setup.sh
            else
                echo "Error: setup.sh not found in the current directory."
                exit 1
            fi
            
            # Run CMake configuration and build
            cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
            cmake --build build -- -j$(nproc)
            
            echo -e "\n[✔] Local build completed successfully."
            break
            ;;

        "Docker Image Build")
            echo -e "\n[!] Starting Docker build..."
            
            # Check if Docker is installed
            if ! command -v docker &> /dev/null; then
                echo "Error: Docker is not installed or not in your PATH."
                exit 1
            fi
            
            # Run the docker build command
            docker build -t surge-ext .
            
            echo -e "\n[✔] Docker image 'surge-ext' built successfully."
            break
            ;;

        "Quit")
            echo "Exiting."
            exit 0
            ;;

        *) 
            echo "Invalid option: $REPLY. Please choose 1, 2, or 3."
            ;;
    esac
done