#include "mdns.h"

void mdns_test(void);
void mdns_test(void)
{
	mdns_init();
	mdns_hostname_set("smarthome");
#if 0
	debug("instance: %d", mdns_instance_name_set("smarthome_instance"));

	mdns_txt_item_t serviceTxtData[3] = {
		{"board","esp8266"},
		{"u","user"},
		{"p","password"}
	};
	mdns_service_add("smarthome-webserver", "_http", "_tcp", 80, serviceTxtData, 3);
	mdns_service_txt_item_set("_http", "_tcp", "path", "/register");
	mdns_service_txt_item_set("_http", "_tcp", "u", "admin");
#endif
}
