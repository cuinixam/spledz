#include <zephyr/kernel.h>

#include "autoconf.h"
#include "button_interface.h"
#include "led_interface.h"
#include "spled.h"

int main(void)
{
	ledInterface_init();
	buttonInterface_init();
	Task_Init();

	while (1) {
		buttonInterface_update();
		spled();
		k_sleep(K_MSEC(CONFIG_OS_TASK_PERIOD));
	}

	return 0;
}
