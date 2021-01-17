#include "provisioning.h"

#include <semaphore.h>

#include "libmcu/logging.h"
#include "libmcu/pubsub.h"
#include "libmcu/compiler.h"

#include "wifi.h"
#include "httpsrv/httpsrv.h"

#if !defined(DEFAULT_HOSTNAME)
#define DEFAULT_HOSTNAME			"smarthome"
#endif

static bool network_interface_init(void)
{
	if (!wifiman_on()) {
		return false;
	}
	if (!wifiman_start_ap()) {
		wifiman_off();
		return false;
	}
	if (!wifiman_set_hostname(DEFAULT_HOSTNAME)) {
		return false;
	}
	return true;
}

static void provisioning_done(void *context, const void *msg, size_t msglen)
{
	unused(msg);
	unused(msglen);

	sem_t *sem = (sem_t *)context;
	sem_post(sem);
}

bool provisioning_start(void)
{
	info("provisioning start");

	network_interface_init();
	httpsrv_start();

	sem_t provisioning_sema;
	if (sem_init(&provisioning_sema, 0, 0) != 0) {
		return false;
	}

	pubsub_subscribe_t done;
	pubsub_subscribe_static(&done, "wifi/provisioning",
			provisioning_done, &provisioning_sema);

	sem_wait(&provisioning_sema);

	return true;
}
