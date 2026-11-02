/*
 * LVGL configuration for the desktop simulator.
 *
 * Only settings that differ from LVGL's defaults are listed; lv_conf_internal.h
 * fills in the rest. On the device the same choices are made through
 * sdkconfig (Kconfig) instead -- keep the two in step: colour depth, heap size
 * and enabled widgets must match, or the simulator stops being representative.
 */

#ifndef LV_CONF_H
#define LV_CONF_H

/* RGB565, as on the HX8394 panel (ADR-0007). */
#define LV_COLOR_DEPTH 16

/* LVGL's own heap. 256 KB, as §5.7.9 budgets for the device. */
#define LV_MEM_SIZE (256 * 1024U)

/* The simulator is single-threaded: SDL events and LVGL run in main(). */
#define LV_USE_OS LV_OS_NONE

#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 1

/* Catch misuse (null objects, bad arguments) loudly during development. */
#define LV_USE_ASSERT_NULL 1
#define LV_USE_ASSERT_MALLOC 1
#define LV_USE_ASSERT_OBJ 1

/* The simulator drives its own SDL2 window (sim/sdl_display.cpp) rather than
 * LVGL's SDL driver, which cannot open a high-DPI window. */
#define LV_USE_SDL 0

/* Characters a wrapped line may break after. LVGL's default includes '.',
 * which splits numbers such as "0.90" across lines. */
#define LV_TXT_BREAK_CHARS " ,;:_)]}"

/* Lets the simulator save the screen to an image (--snapshot). */
#define LV_USE_SNAPSHOT 1

/* LVGL's bundled examples and demos are not built (see sim/CMakeLists.txt). */
#define LV_BUILD_EXAMPLES 0

#endif /* LV_CONF_H */
