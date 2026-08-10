#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#include "button_interface.h"

LOG_MODULE_REGISTER(spled_button, LOG_LEVEL_INF);

enum ButtonIndex {
	BUTTON_POWER = 0,
	BUTTON_UP,
	BUTTON_DOWN,
	BUTTON_COUNT,
};

static const struct gpio_dt_spec buttons[BUTTON_COUNT] = {
	[BUTTON_POWER] = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios),
	[BUTTON_UP] = GPIO_DT_SPEC_GET(DT_ALIAS(sw1), gpios),
	[BUTTON_DOWN] = GPIO_DT_SPEC_GET(DT_ALIAS(sw2), gpios),
};

static boolean pressed[BUTTON_COUNT];

void buttonInterface_init(void)
{
	for (int i = 0; i < BUTTON_COUNT; i++) {
		if (!gpio_is_ready_dt(&buttons[i])) {
			LOG_ERR("button %d is not ready", i);
			continue;
		}
		gpio_pin_configure_dt(&buttons[i], GPIO_INPUT);
	}
}

void buttonInterface_update(void)
{
	for (int i = 0; i < BUTTON_COUNT; i++) {
		pressed[i] = (gpio_pin_get_dt(&buttons[i]) > 0) ? TRUE : FALSE;
	}
}

boolean ButtonInterfaceIsButtonPressed(KeyCodes key)
{
	switch (key) {
	case POWER_BUTTON_KEY:
		return pressed[BUTTON_POWER];
	case KEY_UP:
		return pressed[BUTTON_UP];
	case KEY_DOWN:
		return pressed[BUTTON_DOWN];
	default:
		return FALSE;
	}
}
