#include <zephyr/drivers/led_strip.h>
#include <zephyr/logging/log.h>

#include "led_interface.h"
#include "rte.h"

LOG_MODULE_REGISTER(spled_led, LOG_LEVEL_INF);

/* The hardware counterpart of src/led_adapter.c. On native_sim the LED is a
 * GPIO that is only ever on or off, so the colour has to be drawn as text on a
 * second UART. Here the LED takes the RGB value directly and there is nothing
 * to draw: the board's WS2812 is the display. */
static const struct device *const strip = DEVICE_DT_GET(DT_ALIAS(led_strip));

static struct led_rgb pixel;
static RGBColor previousLightValue;

static void show(const RGBColor *value)
{
	pixel.r = value->red;
	pixel.g = value->green;
	pixel.b = value->blue;

	if (led_strip_update_rgb(strip, &pixel, 1) < 0) {
		LOG_ERR("LED strip update failed");
		return;
	}
	LOG_DBG("r=%3u g=%3u b=%3u", value->red, value->green, value->blue);
}

void ledInterface_init(void)
{
	previousLightValue.red = 0;
	previousLightValue.green = 0;
	previousLightValue.blue = 0;

	if (!device_is_ready(strip)) {
		LOG_ERR("LED strip device is not ready");
		return;
	}

	/* Show the powered-off LED right away: a WS2812 keeps whatever the last
	 * writer left in it across a reset, so an unwritten strip is not dark. */
	show(&previousLightValue);
}

void ledInterface(void)
{
	RGBColor lightValue;

	RteGetLightValue(&lightValue);

	if (lightValue.red == previousLightValue.red &&
	    lightValue.green == previousLightValue.green &&
	    lightValue.blue == previousLightValue.blue) {
		return;
	}
	previousLightValue = lightValue;

	show(&lightValue);
}
