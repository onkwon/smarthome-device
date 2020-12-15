#include "include/wifi.h"

#include <assert.h>
#include <string.h>
#include <pthread.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"

#include "libmcu/logging.h"

#define PASSWORD_MAXLEN			63

enum {
	EVENT_STARTED			= BIT0,
	EVENT_CONNECTED			= BIT1,
	EVENT_DISCONNECTED		= BIT2,
};

static struct {
	bool initialized;
	pthread_mutex_t lock;
	bool connected;
	EventGroupHandle_t event;
	wifiman_network_profile_t ap;
	uint8_t ip[WIFIMAN_IP4_MAXLEN];
	wifiman_event_handler_t event_handlers[WIFIMAN_EVENT_MAX];
} m;

static void wifi_event_handler(void *arg, int32_t event_id, void *event_data)
{
	system_event_sta_disconnected_t *disconnected_event;
	wifi_ap_record_t ap;
	wifiman_event_t etype = WIFIMAN_EVENT_UNKNOWN;

	switch (event_id) {
	case WIFI_EVENT_WIFI_READY:
		info("WiFi ready.");
		break;
	case WIFI_EVENT_STA_START:
		xEventGroupSetBits(m.event, EVENT_STARTED);
		info("WiFi station started.");
		break;
	case WIFI_EVENT_STA_STOP:
		info("WiFi station stopped.");
		break;
	case WIFI_EVENT_STA_CONNECTED:
		esp_wifi_sta_get_ap_info(&ap);
		memcpy(m.ap.bssid, ap.bssid, sizeof(m.ap.bssid));
		m.ap.rssi = ap.rssi;
		m.ap.channel = ap.primary;
		m.ap.security = ap.authmode + 1;
		info("WiFi station connected(RSSI: %d, country: %s, mode %d).",
				ap.rssi, ap.country.cc, ap.authmode);
		etype = WIFIMAN_EVENT_CONNECTED;
		break;
	case WIFI_EVENT_STA_DISCONNECTED:
		disconnected_event = (system_event_sta_disconnected_t *)event_data;
		m.connected = false;
		xEventGroupClearBits(m.event, EVENT_CONNECTED);
		xEventGroupSetBits(m.event, EVENT_DISCONNECTED);
		info("WiFi station disconnected: %x.", disconnected_event->reason);
		etype = WIFIMAN_EVENT_DISCONNECTED;
#if 0
		if (disconnected_event == WIFI_REASON_BASIC_RATE_NOT_SUPPORT) {
			esp_wifi_set_protocol(ESP_IF_WIFI_STA,
					WIFI_PROTOCOL_11B |
					WIFI_PROTOCOL_11G |
					WIFI_PROTOCOL_11N);
			esp_wifi_connect();
		}
#endif
		break;
	case WIFI_EVENT_SCAN_DONE:
		info("WiFi scanning done.");
		break;
	case WIFI_EVENT_STA_AUTHMODE_CHANGE:
		info("WiFi station autentication mode changed.");
		break;
	case WIFI_EVENT_AP_START:
	case WIFI_EVENT_AP_STOP:
	case WIFI_EVENT_AP_STACONNECTED:
	case WIFI_EVENT_AP_STADISCONNECTED:
	case WIFI_EVENT_AP_PROBEREQRECVED:
	default:
		error("Unknown event %x", event_id);
		break;
	}

	if (etype != WIFIMAN_EVENT_UNKNOWN && m.event_handlers[etype] != NULL) {
		m.event_handlers[etype](etype, NULL);
	}
}

static void ip_event_handler(void *arg, int32_t event_id, void *event_data)
{
	ip_event_got_ip_t *event;

	switch (event_id) {
	case IP_EVENT_STA_GOT_IP:
		m.connected = true;
		xEventGroupClearBits(m.event, EVENT_DISCONNECTED);
		xEventGroupSetBits(m.event, EVENT_CONNECTED);
		event = (ip_event_got_ip_t *)event_data;
		memcpy(m.ip, &event->ip_info.ip.addr, sizeof(m.ip));
		info("IP allocated: %s", ip4addr_ntoa(&event->ip_info.ip));
		break;
	default:
		error("Unknown event %x", event_id);
		break;
	}
}

static void event_handler(void *arg, esp_event_base_t event_base,
                                int32_t event_id, void *event_data)
{
	if (event_base == WIFI_EVENT) {
		wifi_event_handler(arg, event_id, event_data);
	} else if (event_base == IP_EVENT) {
		ip_event_handler(arg, event_id, event_data);
	} else {
		error("Unknown event: %s", event_base);
	}
}

static wifiman_error_t connect_internal(const wifiman_network_profile_t *conf,
		const char *pass)
{
	if (!m.initialized) {
		return WIFIMAN_NOT_INITIALIZED;
	}

	if (m.connected) {
		if (esp_wifi_disconnect() == ESP_OK) {
			xEventGroupWaitBits(m.event, EVENT_DISCONNECTED,
					pdTRUE, pdFALSE, portMAX_DELAY);
		}
	}

	wifi_mode_t mode;
	if (esp_wifi_get_mode(&mode) != ESP_OK) {
		return WIFIMAN_NOT_RESPONDING;
	}
	if (mode != WIFI_MODE_STA && mode != WIFI_MODE_APSTA) {
		return WIFIMAN_MODE_ERROR;
	}

	wifi_config_t esp_conf = { 0, };
	strncpy((char *)esp_conf.sta.ssid, conf->ssid, WIFIMAN_SSID_MAXLEN);
	strncpy((char *)esp_conf.sta.password, pass, PASSWORD_MAXLEN);
	if (esp_wifi_set_config(ESP_IF_WIFI_STA, &esp_conf) != ESP_OK) {
		return WIFIMAN_WRONG_SETTINGS;
	}

	memcpy(&m.ap, conf, sizeof(m.ap)); // keep the conncted ap information
	esp_wifi_connect();
	xEventGroupWaitBits(m.event, EVENT_CONNECTED | EVENT_DISCONNECTED,
					pdTRUE, pdFALSE, portMAX_DELAY);
	if (!m.connected) {
		return WIFIMAN_ERROR;
	}

	return WIFIMAN_SUCCESS;
}

static wifiman_error_t disconnect_internal(void)
{
	if (!m.connected) {
		return WIFIMAN_SUCCESS;
	}

	if (esp_wifi_disconnect() == ESP_OK) {
		xEventGroupWaitBits(m.event, EVENT_DISCONNECTED,
				pdTRUE, pdFALSE, portMAX_DELAY);
	}

	if (m.connected) {
		return WIFIMAN_ERROR;
	}

	return WIFIMAN_SUCCESS;
}

static bool turn_wifi_on_internal(void)
{
	tcpip_adapter_init();

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	if (esp_wifi_init(&cfg) != ESP_OK) {
		return false;
	}
	if (esp_wifi_set_storage(WIFI_STORAGE_RAM) != ESP_OK) {
		return false;
	}
	if (esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
				&event_handler, NULL) != ESP_OK) {
		return false;
	}
	if (esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
				&event_handler, NULL) != ESP_OK) {
		return false;
	}
	if (esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK) {
		return false;
	}
	if (esp_wifi_start() != ESP_OK) {
		return false;
	}
	return true;
}

static void turn_wifi_off_internal(void)
{
	disconnect_internal();
	esp_wifi_stop();
	esp_wifi_deinit();
}

bool wifiman_on(void)
{
	bool rc;

	if (!m.initialized) {
		pthread_mutex_init(&m.lock, NULL);
		m.event = xEventGroupCreate();
		assert(m.event != NULL);
		m.initialized = true;
	}

	pthread_mutex_lock(&m.lock);
	{
		rc = turn_wifi_on_internal();
	}
	pthread_mutex_unlock(&m.lock);


	return rc;
}

void wifiman_off(void)
{
	if (!m.initialized) {
		return;
	}

	pthread_mutex_lock(&m.lock);
	{
		turn_wifi_off_internal();
	}
	pthread_mutex_unlock(&m.lock);
}

bool wifiman_reset(void)
{
	if (!m.initialized) {
		return false;
	}

	bool rc;

	pthread_mutex_lock(&m.lock);
	{
		turn_wifi_off_internal();
		rc = turn_wifi_on_internal();
	}
	pthread_mutex_unlock(&m.lock);

	return rc;
}

wifiman_error_t wifiman_connect(const wifiman_network_profile_t *conf,
		const char *pass)
{
	if (conf == NULL) {
		return WIFIMAN_INVALID_PARAM;
	}
	if (pass == NULL && conf->security != WIFIMAN_SECURITY_OPEN) {
		return WIFIMAN_INVALID_PARAM;
	}

	size_t ssid_len = strnlen(conf->ssid, WIFIMAN_SSID_MAXLEN);
	if (ssid_len == 0 || ssid_len >= WIFIMAN_SSID_MAXLEN) {
		return WIFIMAN_INVALID_PARAM;
	}

	wifiman_error_t rc;

	pthread_mutex_lock(&m.lock);
	{
		rc = connect_internal(conf, pass);
	}
	pthread_mutex_unlock(&m.lock);

	return rc;
}

wifiman_error_t wifiman_disconnect(void)
{
	if (!m.initialized) {
		return WIFIMAN_NOT_INITIALIZED;
	}

	wifiman_error_t rc;

	pthread_mutex_lock(&m.lock);
	{
		rc = disconnect_internal();
	}
	pthread_mutex_unlock(&m.lock);

	return rc;
}

bool wifiman_is_connected(void)
{
	return m.connected;
}

bool wifiman_get_ip(uint8_t ip[WIFIMAN_IP4_MAXLEN])
{
	if (ip == NULL) {
		return false;
	}

	memset(ip, 0, WIFIMAN_IP4_MAXLEN);

	if (!m.connected) {
		return false;
	}

	memcpy(ip, m.ip, WIFIMAN_IP4_MAXLEN);
	return true;
}

static wifiman_error_t get_mac_internal(uint8_t mac[WIFIMAN_MAC_MAXLEN])
{
	wifi_mode_t mode;
	if (esp_wifi_get_mode(&mode) != ESP_OK) {
		return WIFIMAN_ERROR;
	}

	esp_interface_t interface;

	switch (mode) {
	case WIFI_MODE_AP:
		interface = ESP_IF_WIFI_AP;
		break;
	case WIFI_MODE_STA:
		interface = ESP_IF_WIFI_STA;
		break;
	default:
		warn("Not supported wifi mode %d", mode);
		return WIFIMAN_MODE_ERROR;
	}

	if (esp_wifi_get_mac(interface, mac) != ESP_OK) {
		return WIFIMAN_ERROR;
	}

	return WIFIMAN_SUCCESS;
}

wifiman_error_t wifiman_get_mac(uint8_t mac[WIFIMAN_MAC_MAXLEN])
{
	if (mac == NULL) {
		return WIFIMAN_INVALID_PARAM;
	}
	if (!m.initialized) {
		return WIFIMAN_NOT_INITIALIZED;
	}

	wifiman_error_t rc;

	pthread_mutex_lock(&m.lock);
	{
		rc = get_mac_internal(mac);
	}
	pthread_mutex_unlock(&m.lock);

	return rc;
}

static int scan_internal(wifi_ap_record_t *records,
		const wifi_scan_config_t *conf, uint16_t *n)
{
	if (esp_wifi_scan_start(conf, true) != ESP_OK) {
		return 0;
	}
	if (esp_wifi_scan_get_ap_records(n, records) != ESP_OK) {
		return 0;
	}
	return (int)*n;
}

static void convert_esp_record_to_profile(wifiman_network_profile_t *profile,
		const wifi_ap_record_t *record)
{
	memcpy(profile->bssid, record->bssid, sizeof(profile->bssid));
	memcpy(profile->ssid, record->ssid, sizeof(profile->ssid));
	profile->rssi = record->rssi;
	profile->channel = record->primary;
	switch (record->authmode) {
	case WIFI_AUTH_OPEN:
		profile->security = WIFIMAN_SECURITY_OPEN;
		break;
	case WIFI_AUTH_WEP:
		profile->security = WIFIMAN_SECURITY_WEP;
		break;
	case WIFI_AUTH_WPA_PSK:
		profile->security = WIFIMAN_SECURITY_WPA;
		break;
	case WIFI_AUTH_WPA2_PSK:
	case WIFI_AUTH_WPA_WPA2_PSK:
		profile->security = WIFIMAN_SECURITY_WPA2;
		break;
	case WIFI_AUTH_WPA2_ENTERPRISE:
		profile->security = WIFIMAN_SECURITY_WPA2_ENTERPRISE;
		break;
	default:
		profile->security = WIFIMAN_SECURITY_MAX;
		break;
	}
}

int wifiman_scan(wifiman_network_profile_t *scanlist, int maxlen)
{
	if (scanlist == NULL || maxlen == 0 || !m.initialized) {
		return 0;
	}

	wifi_ap_record_t *ap_records;
	if ((ap_records = calloc(maxlen, sizeof(*ap_records))) == NULL) {
		error("Failed to allocate.");
		return 0;
	}

	int nr_scanned;

	pthread_mutex_lock(&m.lock);
	{
		nr_scanned = scan_internal(ap_records, NULL, (uint16_t *)&maxlen);
	}
	pthread_mutex_unlock(&m.lock);

	memset(scanlist, 0, sizeof(*scanlist) * maxlen);
	for (int i = 0; i < nr_scanned; i++) {
		convert_esp_record_to_profile(&scanlist[i], &ap_records[i]);
	}

	free(ap_records);

	return nr_scanned;
}

int8_t wifiman_get_rssi(void)
{
	if (!m.connected) {
		return 0;
	}

	wifi_scan_config_t conf = {
		.ssid = (uint8_t *)m.ap.ssid,
		.bssid = m.ap.bssid,
	};
	wifi_ap_record_t ap = { 0, };

	int n = 1;
	int nr_scanned;
	pthread_mutex_lock(&m.lock);
	{
		nr_scanned = scan_internal(&ap, &conf, (uint16_t *)&n);
	}
	pthread_mutex_unlock(&m.lock);
	assert(nr_scanned == 1);

	m.ap.rssi = ap.rssi;

	return ap.rssi;
}

wifiman_error_t wifiman_register_event_handler(wifiman_event_t event,
		wifiman_event_handler_t handler)
{
	switch (event) {
	case WIFIMAN_EVENT_CONNECTED:
	case WIFIMAN_EVENT_DISCONNECTED:
		m.event_handlers[event] = handler;
		break;
	default:
		warn("Unsupported type handler: %d.", event);
		return WIFIMAN_INVALID_PARAM;
	}

	return WIFIMAN_SUCCESS;
}
