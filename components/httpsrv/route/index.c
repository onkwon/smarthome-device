#include "httpsrv/route/index.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "esp_http_server.h"

const char *html = "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\
		    <a href=register>Register</a>";

static int handler_get(httpd_req_t *req)
{
	httpd_resp_set_type(req, "text/html");
	httpd_resp_send(req, html, strlen(html));

	return 0;
}

const void *httpsrv_route_index_get(void)
{
	static const httpd_uri_t index = {
		.uri       = "/",
		.method    = HTTP_GET,
		.handler   = handler_get,
	};
	return &index;
}
