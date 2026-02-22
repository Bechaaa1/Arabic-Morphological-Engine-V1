#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────────────
#  Build & Run script for Arabic Morphological Engine
# ─────────────────────────────────────────────────────────────────
set -e

BINARY="arabic-morpho"
MODE="${1:-server}"  # server | cli | demo | build-only

# Colors
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
CYAN='\033[0;36m'; NC='\033[0m'; BOLD='\033[1m'

echo -e "${CYAN}${BOLD}"
echo "  ╔══════════════════════════════════════════════════╗"
echo "  ║   Arabic Morphological Engine — Build Script    ║"
echo "  ╚══════════════════════════════════════════════════╝"
echo -e "${NC}"

# ── Try cmake first, fallback to g++ ──────────────────────────────
if command -v cmake &>/dev/null; then
    echo -e "${YELLOW}Building with CMake...${NC}"
    mkdir -p build
    cmake -B build -DCMAKE_BUILD_TYPE=Release -Wno-dev 2>/dev/null
    cmake --build build --parallel 2>/dev/null
    cp build/$BINARY . 2>/dev/null || true
else
    echo -e "${YELLOW}Building with g++...${NC}"
    g++ -std=c++17 -O2 -Iinclude \
        src/ArabicUtils.cpp \
        src/AVLTree.cpp \
        src/HashTable.cpp \
        src/MorphologyEngine.cpp \
        src/HttpServer.cpp \
        src/main.cpp \
        -o $BINARY
fi

echo -e "${GREEN}✓ Build successful: ./$BINARY${NC}\n"

if [[ "$MODE" == "build-only" ]]; then
    exit 0
fi

# ── Run ──────────────────────────────────────────────────────────
case "$MODE" in
    server)
        echo -e "${CYAN}Starting HTTP server on http://localhost:8080${NC}"
        echo -e "${CYAN}Open your browser and navigate to the URL above.${NC}"
        echo -e "${YELLOW}Press Ctrl+C to stop.${NC}\n"
        ./$BINARY
        ;;
    cli)
        echo -e "${CYAN}Starting interactive CLI mode...${NC}\n"
        ./$BINARY --cli
        ;;
    demo)
        echo -e "${CYAN}Running demonstration...${NC}\n"
        ./$BINARY --demo
        ;;
    *)
        echo -e "${RED}Unknown mode: $MODE${NC}"
        echo "Usage: ./build.sh [server|cli|demo|build-only]"
        exit 1
        ;;
esac
