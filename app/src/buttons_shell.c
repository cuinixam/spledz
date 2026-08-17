#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>

#include "autoconf.h"
#include "rte.h"

/* Nothing drives the emulated button pins on native_sim, so this is the port's
 * replacement for SPLed's pc_terminal keyboard: `spled power`, `spled up`,
 * `spled down`. The pins are the ones boards/native_sim_native_64.overlay
 * declares. On real hardware the buttons are real, so this file is not built. */

/* powerButton() needs POWER_BUTTON_PRESS_DEBOUNCE (10) consecutive polls at
 * CONFIG_OS_TASK_PERIOD ms to accept a press; hold well past that. */
#define PRESS_HOLD_MS (CONFIG_OS_TASK_PERIOD * 20)

static const struct gpio_dt_spec power_button = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);
static const struct gpio_dt_spec up_button = GPIO_DT_SPEC_GET(DT_ALIAS(sw1), gpios);
static const struct gpio_dt_spec down_button = GPIO_DT_SPEC_GET(DT_ALIAS(sw2), gpios);

static int press(const struct shell *sh, const struct gpio_dt_spec *button)
{
	/* The buttons are active low, so a press drives the pin to 0. */
	int ret = gpio_emul_input_set(button->port, button->pin, 0);

	if (ret) {
		shell_error(sh, "pin %u could not be driven low: %d", button->pin, ret);
		return ret;
	}
	k_msleep(PRESS_HOLD_MS);

	ret = gpio_emul_input_set(button->port, button->pin, 1);
	if (ret) {
		shell_error(sh, "pin %u could not be released: %d", button->pin, ret);
	}

	return ret;
}

static int cmd_power(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return press(sh, &power_button);
}

static int cmd_up(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return press(sh, &up_button);
}

static int cmd_down(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return press(sh, &down_button);
}

static int cmd_state(const struct shell *sh, size_t argc, char **argv)
{
	RGBColor light;

	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	RteGetLightValue(&light);
	shell_print(sh, "power=%s light=%u,%u,%u",
		    RteGetPowerState() == POWER_STATE_ON ? "on" : "off", light.red, light.green,
		    light.blue);
	shell_print(sh, "buttons power=%d up=%d down=%d (logical, 1 means pressed)",
		    gpio_pin_get_dt(&power_button), gpio_pin_get_dt(&up_button),
		    gpio_pin_get_dt(&down_button));

	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(spled_cmds,
			       SHELL_CMD(power, NULL, "Toggle power on or off.", cmd_power),
			       SHELL_CMD(up, NULL, "Press the up button.", cmd_up),
			       SHELL_CMD(down, NULL, "Press the down button.", cmd_down),
			       SHELL_CMD(state, NULL, "Show power, light and button state.", cmd_state),
			       SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(spled, &spled_cmds, "Press an SPLed button.", NULL);
