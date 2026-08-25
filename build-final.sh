#!/bin/bash
set -e

if [ -z "$1" ]; then
  echo "Usage: ./build-final.sh /path/to/ns-3.42"
  exit 1
fi

NS3_DIR="$(cd "$1" && pwd)"
PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
TARGET="$NS3_DIR/scratch/ecn-final-modules"

echo "Installing final modular ECN project into:"
echo "$NS3_DIR"

rm -rf "$TARGET"
mkdir -p "$TARGET"

cp -R "$PROJECT_DIR/modules/." "$TARGET/"

# Copy the integration source as the scratch program.
cp "$PROJECT_DIR/simulation/main.cc" "$NS3_DIR/scratch/ecn-final.cc"

echo
echo "Building ns-3..."
cd "$NS3_DIR"
./ns3 build

echo
echo "Build completed."
echo
echo "Run:"
echo './ns3 run "scratch/ecn-final.cc --packetCount=100000 --simulationTime=30 --alpha=1.0"'
