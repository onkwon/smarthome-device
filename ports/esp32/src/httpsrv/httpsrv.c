#include "httpsrv/httpsrv.h"

#include <string.h>
#include "libmcu/logging.h"

#include "esp_http_server.h"

#include "httpsrv/route/index.h"
#include "httpsrv/route/register.h"

struct http_server {
	httpd_handle_t handle;
};

static void register_default_route_handlers(httpd_handle_t server)
{
	httpd_register_uri_handler(server, httpsrv_route_index_get());
	httpd_register_uri_handler(server, httpsrv_route_register_get());
	httpd_register_uri_handler(server, httpsrv_route_register_post());
}

httpsrv_t *httpsrv_start(void)
{
	httpd_handle_t server = NULL;
	httpd_config_t config = HTTPD_DEFAULT_CONFIG();

	esp_err_t err = httpd_start(&server, &config);
	if (err != ESP_OK) {
		error("fail to start httpsrv: %d", err);
		return NULL;
	}

	register_default_route_handlers(server);
	info("Web server started");

	return (httpsrv_t *)server;
}
