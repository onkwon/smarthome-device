#include "httpsrv/route/register.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include "esp_http_server.h"

#include "url.h"
#include "libmcu/pubsub.h"
#include "sleep.h"

#define WIFI_SCAN_MAXLEN		10
#define WIFI_PASS_MAXLEN		63

#if !defined(MIN)
#define MIN(a, b)			(((a) > (b))? (b) : (a))
#endif

struct htmlobj {
	struct {
		const char *head;
		const char *body;
		const char *tail;
	} template;

	size_t page_size;
	char page[];
};

static void *htmlobj_new(size_t page_size)
{
	struct htmlobj *obj = calloc(1, page_size + sizeof(struct htmlobj));
	if (obj) {
		obj->page_size = page_size;
	}
	return obj;
}

static void htmlobj_delete(struct htmlobj *self)
{
	free(self);
}

static void htmlobj_set_template(struct htmlobj *self,
		const char *head, const char *body, const char *tail)
{
	self->template.head = head;
	self->template.body = body;
	self->template.tail = tail;
}

static void htmlobj_pack(struct htmlobj *self, const char *data)
{
	strcpy(self->page, self->template.head);
	size_t len = strlen(self->page);
	len += (size_t)snprintf(self->page+len, self->page_size-len-1, self->template.body, data);
	strncpy(self->page+len, self->template.tail, self->page_size - len - 1);
	self->page[self->page_size-1] = '\0';
}

const char *html_template_head = \
"<!DOCTYPE html>\
<html><head>\
	<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\
	<title>device onboarding</title>\
</head><body>";
const char *html_template_body = \
"<form action=\"\" method=\"post\">\
	<select name=\"ssid\">\
		<option value=\"\">Select SSID:</option>\
		%s\
	</select>\
	<input name=\"password\" type=\"password\" autocomplete=\"current-password\" placeholder=\"Password\" />\
	<button type=\"submit\">Register</button>";
const char *html_template_body_result = "%s";
const char *html_template_tail = "</body></html>";

#include "wifi.h"

static inline int convert_wifi_scanlist_to_html(char *buf,
		const wifiman_network_profile_t *scanlist, int n)
{
	int len = 0;
	for (int i = 0; i < n; i++) {
		len += sprintf(buf + len,
				"<option value=\"%s\">%s (%d)</option>",
				scanlist[i].ssid,
				scanlist[i].ssid,
				scanlist[i].rssi);
	}

	return len;
}

static void pack_wifi_scanlist(struct htmlobj *self)
{
	wifiman_network_profile_t scanlist[WIFI_SCAN_MAXLEN];
	int n = wifiman_scan(scanlist, WIFI_SCAN_MAXLEN);
	// keep the converted string at the end of the same result buffer
	// temporarily to avoid dynamic allocation
	size_t offset = self->page_size
		- WIFIMAN_SSID_MAXLEN * WIFI_SCAN_MAXLEN * 2 + 32 + 1;
	char *scanlist_html = self->page + offset;
	convert_wifi_scanlist_to_html(scanlist_html, scanlist, n);
	htmlobj_pack(self, scanlist_html);
}

static int handler_get(httpd_req_t *req)
{
	size_t pagesize = (WIFIMAN_SSID_MAXLEN * WIFI_SCAN_MAXLEN * 2
			+ 32/*option string*/ + 1/*null*/)
		+ strlen(html_template_head)
		+ strlen(html_template_body)
		+ strlen(html_template_tail);
	struct htmlobj *html = htmlobj_new(pagesize);
	if (html == NULL) {
		httpd_resp_send_500(req);
		return -1;
	}
	htmlobj_set_template(html, html_template_head,
			html_template_body, html_template_tail);

	pack_wifi_scanlist(html);
	httpd_resp_set_type(req, "text/html");
	httpd_resp_send(req, html->page, (ssize_t)strlen(html->page));

	htmlobj_delete(html);

	return 0;
}

static void extract_ap_info(httpd_req_t *req, char *ssid, char *pass)
{
	char *content = malloc(req->content_len+1);
	if (!content || httpd_req_recv(req, content, req->content_len) <= 0) {
		goto out;
	}
	content[req->content_len] = '\0';

	if (httpd_query_key_value(content, "ssid", ssid, WIFIMAN_SSID_MAXLEN)
			|| httpd_query_key_value(content, "password", pass, WIFI_PASS_MAXLEN)) {
		goto out;
	}

	url_decode(ssid, ssid, WIFIMAN_SSID_MAXLEN);
	url_decode(pass, pass, WIFI_PASS_MAXLEN);
	url_convert_plus_to_space(ssid);
	url_convert_plus_to_space(pass);
out:
	free(content);
}

static bool test_if_ap_valid(const char *ssid, const char *pass)
{
	uint8_t buf[sizeof(wifiman_network_profile_t)+WIFIMAN_PASS_MAXLEN] = {0,};
	wifiman_network_profile_t *profile = (void *)buf;

	strcpy(profile->ssid, ssid);
	strcpy(profile->password, pass);

	if (wifiman_connect(profile) != WIFIMAN_SUCCESS) {
		return false;
	}
	return true;
}

static bool save(const char *ssid, const char *pass)
{
	uint8_t buf[sizeof(wifiman_network_profile_t)+WIFIMAN_PASS_MAXLEN] = {0,};
	wifiman_network_profile_t *profile = (void *)buf;

	strncpy(profile->ssid, ssid, MIN(WIFIMAN_SSID_MAXLEN, strlen(ssid)));
	strncpy(profile->password, pass, MIN(WIFIMAN_PASS_MAXLEN, strlen(pass)));

	if (!wifiman_save_network(profile)) {
		return false;
	}

	return true;
}

static int handler_post(httpd_req_t *req)
{
	const char *result_string = "Failed associating with the AP";
	bool newly_saved = false;

	char *ssid, *pass;
	if (!(ssid = calloc(1, WIFIMAN_SSID_MAXLEN))) {
		httpd_resp_send_500(req);
		goto out;
	}
	if (!(pass = calloc(1, WIFI_PASS_MAXLEN))) {
		httpd_resp_send_500(req);
		goto out_free_ssid;
	}

	extract_ap_info(req, ssid, pass);
	if (test_if_ap_valid(ssid, pass) && save(ssid, pass)) {
		result_string = "Successfully registered!";
		newly_saved = true;
	}

	size_t pagesize = strlen(html_template_head)
		+ strlen(html_template_body_result) + 80// result_string
		+ strlen(html_template_tail) + 1;
	struct htmlobj *html = htmlobj_new(pagesize);
	if (html == NULL) {
		httpd_resp_send_500(req);
		goto out_free_pass;
	}
	htmlobj_set_template(html, html_template_head, html_template_body_result, html_template_tail);
	htmlobj_pack(html, result_string);
	httpd_resp_set_type(req, "text/html");
	httpd_resp_send(req, html->page, (ssize_t)strlen(html->page));

	htmlobj_delete(html);

out_free_pass:
	free(pass);
out_free_ssid:
	free(ssid);
out:
	if (newly_saved) {
		httpd_sess_trigger_close(req->handle, httpd_req_to_sockfd(req));
		sleep_ms(500); // TODO: wait until response complete

		pubsub_publish("wifi/provisioning", NULL, 0);
	}

	return 0;
}

const void *httpsrv_route_register_get(void)
{
	static const httpd_uri_t index = {
		.uri       = "/register",
		.method    = HTTP_GET,
		.handler   = handler_get,
	};
	return &index;
}

const void *httpsrv_route_register_post(void)
{
	static const httpd_uri_t index = {
		.uri       = "/register",
		.method    = HTTP_POST,
		.handler   = handler_post,
	};
	return &index;
}
