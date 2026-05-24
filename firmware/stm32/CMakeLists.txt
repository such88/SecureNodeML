# firmware/stm32/CMakeLists.txt
#
# Zephyr application build file for SecureInferNode STM32F407.
#
# Build command (from repo root after west init + west update):
#   west build -p auto -b disco_f407vg firmware/stm32
#
# Flash command:
#   west flash --runner openocd
#
# Clean build:
#   west build -p always -b disco_f407vg firmware/stm32

cmake_minimum_required(VERSION 3.20.0)

# ── Must come BEFORE project() ─────────────────────────────────────────────
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(secureinfernode VERSION 0.1.0 LANGUAGES C CXX)

# ── Include paths ──────────────────────────────────────────────────────────
target_include_directories(app PRIVATE
  include                         # our own headers: model_verify.h etc.
  ${ZEPHYR_BASE}/../tflite-micro  # TFLite Micro headers
)

# ── Source files ───────────────────────────────────────────────────────────
# ADD every new .c or .cc file you create to this list.
# Forgetting this causes: "undefined reference to ..." linker error.
target_sources(app PRIVATE
  src/main.c
  src/model_verify.c
  src/version_check.c
  src/spdm_responder.c
  src/inference_thread.c
  src/ota_receiver.c

  # ── Generated file — uncomment after Day 7 ─────────────────────────────
  # Run: xxd -i models/converted/anomaly_int8.tflite > firmware/stm32/src/anomaly_model_data.cc
  src/anomaly_model_data.cc
)

# ── TFLite Micro library (added as a CMake subdirectory) ───────────────────
# Uncomment after running: west update  (which fetches tflite-micro)
# add_subdirectory(${ZEPHYR_BASE}/../tflite-micro tflite-micro-build)
# target_link_libraries(app PRIVATE tensorflow-lite-micro)
