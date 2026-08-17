#include <zephyr/drivers/gpio.h>
#ifdef CONFIG_GPIO_EMUL
#include <zephyr/drivers/gpio/gpio_emul.h>
#endif
#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>

#include "autoconf.h"
#include "rte.h"

/* SPLed's pc_terminal platform is also a keyboard simulator; this is the port's
 * replacement for it: `spled power`, `spled up`, `spled down`. On native_sim
 * nothing drives the emulated pins, so without this there is no way in at all.
 * On the board it is a convenience, and it coexists with the soldered buttons:
 * both ways of pressing end up as the same low level on the same pin, so the
 * adapter above cannot tell them apart and does not have to. */

/* powerButton() needs POWER_BUTTON_PRESS_DEBOUNCE (10) consecutive polls at
 * CONFIG_OS_TASK_PERIOD ms to accept a press; hold well past that. */
#define PRESS_HOLD_MS (CONFIG_OS_TASK_PERIOD * 20)

static const struct gpio_dt_spec power_button = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);
static const struct gpio_dt_spec up_button = GPIO_DT_SPEC_GET(DT_ALIAS(sw1), gpios);
static const struct gpio_dt_spec down_button = GPIO_DT_SPEC_GET(DT_ALIAS(sw2), gpios);

/* Hold the button, then let it go. The pin polarity lives in the devicetree, so
 * neither half names a level. */
static int hold(const struct gpio_dt_spec *button)
{
#ifdef CONFIG_GPIO_EMUL
	/* An emulated input has a value of its own, separate from what the pin
	 * would be driven to, and only this call can set it. */
	return gpio_emul_input_set(button->port, button->pin, button->dt_flags & GPIO_ACTIVE_LOW ? 0 : 1);
#else
	/* A real input cannot be told what to read, so drive it instead, and keep
	 * the input buffer on so the adapter still reads the pin. Driving the
	 * pressed level is what a wired button does, so one held at the same time
	 * pulls the same way rather than fighting the driver. */
	return gpio_pin_configure_dt(button, GPIO_INPUT | GPIO_OUTPUT_ACTIVE);
#endif
}

static int release(const struct gpio_dt_spec *button)
{
#ifdef CONFIG_GPIO_EMUL
	return gpio_emul_input_set(button->port, button->pin, button->dt_flags & GPIO_ACTIVE_LOW ? 1 : 0);
#else
	/* Back to a plain input. The pull-up from the devicetree takes the pin to
	 * the released level, and a finger on the button still overrides it. */
	return gpio_pin_configure_dt(button, GPIO_INPUT);
#endif
}

static int press(const struct shell *sh, const struct gpio_dt_spec *button)
{
	int ret = hold(button);

	if (ret) {
		shell_error(sh, "pin %u could not be pressed: %d", button->pin, ret);
		return ret;
	}
	k_msleep(PRESS_HOLD_MS);

	ret = release(button);
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
