#include <stdio.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>

#include "led_interface.h"
#include "rte.h"

LOG_MODULE_REGISTER(spled_led, LOG_LEVEL_INF);

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

/* The LED gets a UART of its own rather than sharing the console with Zephyr's
 * shell. On native_sim that is a second pseudo terminal, so this adapter owns
 * its screen the way SPLed's pc_terminal platform owns the invoking terminal,
 * and it can redraw in place without fighting anyone for the cursor. */
static const struct device *const display = DEVICE_DT_GET(DT_NODELABEL(uart1));

static RGBColor previousLightValue;

static void display_write(const char *text)
{
	if (!device_is_ready(display)) {
		return;
	}
	while (*text) {
		uart_poll_out(display, *text++);
	}
}

static void display_light(const RGBColor *value)
{
	char frame[64];

	/* The numbers are not decoration: a viewer that drops 24-bit colour (GNU
	 * screen, for one) renders every state as the same grey block, and the
	 * demo looks dead while it is working. */
	snprintf(frame, sizeof(frame), "\x1b[48;2;%u;%u;%um LED \x1b[0m r=%3u g=%3u b=%3u\r",
		 value->red, value->green, value->blue, value->red, value->green, value->blue);
	display_write(frame);
}

void ledInterface_init(void)
{
	previousLightValue.red = 0;
	previousLightValue.green = 0;
	previousLightValue.blue = 0;

	if (!gpio_is_ready_dt(&led)) {
		LOG_ERR("LED device is not ready");
		return;
	}
	gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);

	/* Hide the cursor and show the powered-off LED, so the display is there
	 * before anything happens rather than only after the first change. */
	display_write("\x1b[?25l");
	display_light(&previousLightValue);
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

	/* The emulated board LED is on/off only, so it just tracks "lit". */
	gpio_pin_set_dt(&led, (lightValue.red || lightValue.green || lightValue.blue) ? 1 : 0);

	display_light(&lightValue);
}
