#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#include "led_interface.h"
#include "rte.h"

LOG_MODULE_REGISTER(spled_led, LOG_LEVEL_INF);

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static RGBColor previousLightValue;

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

	/* The board has a single on/off LED, so the colour is only logged. */
	gpio_pin_set_dt(&led, (lightValue.red || lightValue.green || lightValue.blue) ? 1 : 0);
	LOG_INF("light r=%u g=%u b=%u", lightValue.red, lightValue.green, lightValue.blue);
}
