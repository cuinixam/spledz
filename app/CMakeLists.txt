cmake_minimum_required(VERSION 3.20.0)

# SPLed is a plain west project. It knows nothing about Zephyr, so nothing in
# this port is mirrored on the SPLed side.
if(NOT DEFINED SPLED_ROOT)
  get_filename_component(SPLED_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../deps/spled ABSOLUTE)
endif()
if(NOT EXISTS ${SPLED_ROOT}/KConfig)
  message(FATAL_ERROR
    "SPLed sources not found at ${SPLED_ROOT}. "
    "Run `west update`, or configure with -DSPLED_ROOT=<path>.")
endif()

# app/Kconfig sources the SPLed feature model through this variable, so it must
# be set before find_package(Zephyr) runs Kconfig.
set(ENV{SPLED_ROOT} ${SPLED_ROOT})

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(spledz C)

set(SPLED_COMPONENTS ${SPLED_ROOT}/components)

target_sources(app PRIVATE
  src/main.c
  src/button_adapter.c
  ${SPLED_COMPONENTS}/rte/src/rte.c
  ${SPLED_COMPONENTS}/spled/src/spled.c
  ${SPLED_COMPONENTS}/power_button/src/power_button.c
  ${SPLED_COMPONENTS}/power_signal_processing/src/power_signal_processing.c
  ${SPLED_COMPONENTS}/light_controller/src/light_controller.c
  ${SPLED_COMPONENTS}/main_control_knob/src/main_control_knob.c
)

# The platform adapters, picked by what the board's devicetree offers rather
# than by the board name: an RGB LED on a data line goes through led_strip, a
# plain on/off LED through gpio.
target_sources_ifdef(CONFIG_LED_STRIP app PRIVATE src/led_adapter_ws2812.c)
target_sources_ifndef(CONFIG_LED_STRIP app PRIVATE src/led_adapter.c)
# Both platforms: on native_sim it is the only way to press a button, on the
# board it is the one that needs no wiring.
target_sources_ifdef(CONFIG_SHELL app PRIVATE src/buttons_shell.c)

# In SPLed each variant lists the components it is built from. Zephyr has no
# variant to carry that list, so the selection is re-derived from a feature
# symbol here.
target_sources_ifdef(CONFIG_BRIGHTNESS_ADJUSTMENT_ENABLED app PRIVATE
  ${SPLED_COMPONENTS}/brightness_controller/src/brightness_controller.c
)

# SPLed's platform toolchain files supply this: `static` in a product build,
# empty in a test build so the tests can reach internal functions. Zephyr has no
# platform layer to carry it, so it is repeated here.
target_compile_definitions(app PRIVATE SPLE_TESTABLE_STATIC=static)

target_include_directories(app PRIVATE
  ${SPLED_ROOT}/platforms
  ${SPLED_COMPONENTS}/rte/src
  ${SPLED_COMPONENTS}/spled/src
  ${SPLED_COMPONENTS}/led_interface
  ${SPLED_COMPONENTS}/button_interface
  ${SPLED_COMPONENTS}/power_button/src
  ${SPLED_COMPONENTS}/power_signal_processing/src
  ${SPLED_COMPONENTS}/light_controller/src
  ${SPLED_COMPONENTS}/main_control_knob/src
  ${SPLED_COMPONENTS}/brightness_controller/src
  # SPLed components include "autoconf.h" by name; Zephyr generates it here.
  ${PROJECT_BINARY_DIR}/include/generated/zephyr
)
