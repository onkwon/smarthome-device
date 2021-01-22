#include "include/wifi.h"

#include <assert.h>
#include <string.h>
#include <pthread.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"

#include "libmcu/logging.h"
#include "libmcu/compiler.h"
#include "nvs_kvstore.h"

#define WIFI_KVSTORE_NAMESPACE		"wifi"
#define WIFI_KVSTORE_REGISTRY		"registry"

#if !defined(DEFAULT_WIFI_SSID)
#define DEFAULT_WIFI_SSID		"smarthome"
#endif
#if !defined(DEFAULT_WIFI_PASS)
#define DEFAULT_WIFI_PASS		"smarthome"
#endif
#if !defined(DEFAULT_WIFI_MAX_CONNECTIONS)
#define DEFAULT_WIFI_MAX_CONNECTIONS	1
#endif

#define PASSWORD_MAXLEN			63

enum {
	EVENT_STARTED			= BIT0,
	EVENT_CONNECTED			= BIT1,
	EVENT_DISCONNECTED		= BIT2,
};

struct network_registry {
	uint8_t count;
	struct {
		bool used;
	} index[WIFIMAN_MAX_NETWORK_PROFILES];
};

static struct {
	bool initialized;
	pthread_mutex_t lock;
	bool connected;
	EventGroupHandle_t event;
	wifiman_network_profile_t ap;
	uint8_t ip[WIFIMAN_IP4_MAXLEN];
	wifiman_event_handler_t event_handlers[WIFIMAN_EVENT_MAX];

	esp_netif_t *netif_sta;
	esp_netif_t *netif_ap;
} m;

static void wifi_event_handler(void *arg, int32_t event_id, void *event_data)
{
	unused(arg);
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
	unused(arg);
	ip_event_got_ip_t *event;

	switch (event_id) {
	case IP_EVENT_STA_GOT_IP:
		m.connected = true;
		xEventGroupClearBits(m.event, EVENT_DISCONNECTED);
		xEventGroupSetBits(m.event, EVENT_CONNECTED);
		event = (ip_event_got_ip_t *)event_data;
		memcpy(m.ip, &event->ip_info.ip.addr, sizeof(m.ip));
		info("IP allocated: " IPSTR, IP2STR(&event->ip_info.ip));
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

static wifiman_error_t connect_internal(const wifiman_network_profile_t *info)
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
	strncpy((char *)esp_conf.sta.ssid, info->ssid, WIFIMAN_SSID_MAXLEN);
	strncpy((char *)esp_conf.sta.password, info->password, PASSWORD_MAXLEN);

	memcpy(esp_conf.sta.bssid, info->bssid, sizeof(esp_conf.sta.bssid));
	if (info->bssid[0] != 0 && info->bssid[1] != 0 &&
			info->bssid[2] != 0 && info->bssid[3] != 0 &&
			info->bssid[4] != 0 && info->bssid[5] != 0) {
		esp_conf.sta.bssid_set = true;
	}

	if (esp_wifi_set_config(ESP_IF_WIFI_STA, &esp_conf) != ESP_OK) {
		return WIFIMAN_WRONG_SETTINGS;
	}

	memcpy(&m.ap, info, sizeof(m.ap)); // keep the conncted ap information
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

static bool initialize_network_interfaces(void)
{
	if (esp_netif_init() != ESP_OK) {
		return false;
	}
	if ((m.netif_ap = esp_netif_create_default_wifi_ap()) == NULL) {
		return false;
	}
	if ((m.netif_sta = esp_netif_create_default_wifi_sta()) == NULL) {
		return false;
	}
	return true;
}

static bool turn_wifi_on_internal(void)
{
	if (!initialize_network_interfaces()) {
		return false;
	}

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
	return true;
}

static void deinitialize_network_interfaces(void)
{
	esp_netif_destroy(m.netif_ap), m.netif_ap = NULL;
	esp_netif_destroy(m.netif_sta), m.netif_sta = NULL;
	esp_netif_deinit();
}

static void turn_wifi_off_internal(void)
{
	deinitialize_network_interfaces();
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

bool wifiman_start_station(void)
{
	if (esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK) {
		return false;
	}
	if (esp_wifi_start() != ESP_OK) {
		return false;
	}
	return true;
}

bool wifiman_stop_station(void)
{
	disconnect_internal();
	esp_wifi_stop();
	return true;
}

bool wifiman_start_ap(void)
{
	wifi_config_t wifi_config = {
		.ap = {
			.ssid = DEFAULT_WIFI_SSID,
			.ssid_len = (uint8_t)strlen(DEFAULT_WIFI_SSID),
			.password = DEFAULT_WIFI_PASS,
			.max_connection = DEFAULT_WIFI_MAX_CONNECTIONS,
			.authmode = WIFI_AUTH_WPA_WPA2_PSK
		},
	};
	if (strlen(DEFAULT_WIFI_PASS) == 0) {
		wifi_config.ap.authmode = WIFI_AUTH_OPEN;
	}

	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
	ESP_ERROR_CHECK(esp_wifi_start());

	return true;
}

bool wifiman_stop_ap(void)
{
	esp_wifi_stop();
	return true;
}

static esp_netif_t *get_interface_type_from_mode(wifi_mode_t mode)
{
	switch (mode) {
	case WIFI_MODE_STA:
		return m.netif_sta;
	case WIFI_MODE_AP:
	case WIFI_MODE_APSTA:
		return m.netif_ap;
	default:
		break;
	}
	return NULL;
}

bool wifiman_set_hostname(const char *hostname)
{
	esp_netif_t *interface;
	wifi_mode_t mode;
	if (esp_wifi_get_mode(&mode) != ESP_OK) {
		return false;
	}
	if ((interface = get_interface_type_from_mode(mode)) == NULL) {
		return false;
	}
	return esp_netif_set_hostname(interface, hostname) == ESP_OK;
}

bool wifiman_get_hostname(const char **hostname)
{
	esp_netif_t *interface;
	wifi_mode_t mode;
	if (esp_wifi_get_mode(&mode) != ESP_OK) {
		return false;
	}
	if ((interface = get_interface_type_from_mode(mode)) == NULL) {
		return false;
	}
	return esp_netif_get_hostname(interface, hostname) == ESP_OK;
}

wifiman_error_t wifiman_connect(const wifiman_network_profile_t *info)
{
	if (info == NULL) {
		return WIFIMAN_INVALID_PARAM;
	}

	size_t len = strnlen(info->ssid, WIFIMAN_SSID_MAXLEN);
	if (len == 0 || len >= WIFIMAN_SSID_MAXLEN) {
		return WIFIMAN_INVALID_PARAM;
	}
	len = strnlen(info->password, WIFIMAN_PASS_MAXLEN);
	if (len == 0 || len >= WIFIMAN_PASS_MAXLEN) {
		return WIFIMAN_INVALID_PARAM;
	}

	wifiman_error_t rc;

	pthread_mutex_lock(&m.lock);
	{
		rc = connect_internal(info);
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

static void load_network_registry(kvstore_t *kv,
		struct network_registry *registry)
{
	if (kvstore_read(kv, WIFI_KVSTORE_REGISTRY, registry, sizeof(*registry))
			!= sizeof(*registry)) {
		memset(registry, 0, sizeof(*registry));
	}
}

static size_t count_saved_networks_internal(void)
{
	size_t count = 0;

	kvstore_t *kv = nvs_kvstore_open(WIFI_KVSTORE_NAMESPACE);
	if (kv == NULL) {
		error("cannot open %s kvstore", WIFI_KVSTORE_NAMESPACE);
		return 0;
	}

	struct network_registry registry;
	load_network_registry(kv, &registry);
	count = registry.count;

	nvs_kvstore_close(kv);

	return count;
}

static uint8_t get_empty_network_slot_index(const struct network_registry *p)
{
	for (uint8_t i = 0; i < WIFIMAN_MAX_NETWORK_PROFILES; i++) {
		if (!p->index[i].used) {
			return i;
		}
	}

	return WIFIMAN_MAX_NETWORK_PROFILES;
}

static bool update_network_registry(const struct network_registry *registry,
		kvstore_t *kv)
{
	if (kvstore_write(kv, WIFI_KVSTORE_REGISTRY, registry, sizeof(*registry))
			!= sizeof(*registry)) {
		return false;
	}
	return true;
}

static bool write_network_profile(const wifiman_network_profile_t *profile,
		kvstore_t *kv, uint8_t index)
{
	char index_string[4] = { 0, };
	snprintf(index_string, 3, "%u", index);

	size_t len = sizeof(*profile) + strlen(profile->password);
	if (kvstore_write(kv, index_string, profile, len) != len) {
		error("cannot write");
		return false;
	}

	return true;
}

static bool read_network_profile(wifiman_network_profile_t *profile,
		kvstore_t *kv, uint8_t index)
{
	char index_string[4] = { 0, };
	snprintf(index_string, 3, "%u", index);

	size_t len = sizeof(*profile) + WIFIMAN_PASS_MAXLEN;
	if (kvstore_read(kv, index_string, profile, len) == 0) {
		error("cannot read");
		return false;
	}

	return true;
}

static bool save_network_internal(const wifiman_network_profile_t *profile)
{
	bool rc = false;

	kvstore_t *kv = nvs_kvstore_open(WIFI_KVSTORE_NAMESPACE);
	if (kv == NULL) {
		error("cannot open %s kvstore", WIFI_KVSTORE_NAMESPACE);
		return false;
	}

	struct network_registry registry;
	load_network_registry(kv, &registry);

	uint8_t index = get_empty_network_slot_index(&registry);

	if (index >= WIFIMAN_MAX_NETWORK_PROFILES) {
		error("no space for additional network profile(max: %u).",
				WIFIMAN_MAX_NETWORK_PROFILES);
		goto out;
	}

	if (!write_network_profile(profile, kv, index)) {
		goto out;
	}

	registry.index[index].used = true;
	registry.count = (uint8_t)(registry.count + 1);

	if (!update_network_registry(&registry, kv)) {
		goto out;
	}

	rc = true;
out:
	nvs_kvstore_close(kv);
	return rc;
}

static bool get_network_internal(wifiman_network_profile_t *profile,
		uint8_t index)
{
	struct network_registry registry;
	bool rc = false;

	kvstore_t *kv = nvs_kvstore_open(WIFI_KVSTORE_NAMESPACE);
	if (kv == NULL) {
		error("cannot open %s kvstore", WIFI_KVSTORE_NAMESPACE);
		return false;
	}

	load_network_registry(kv, &registry);

	if (!registry.index[index].used) {
		goto out;
	}
	if (!read_network_profile(profile, kv, index)) {
		goto out;
	}

	rc = true;
out:
	nvs_kvstore_close(kv);
	return rc;
}

static bool clear_networks_internal(void)
{
	struct network_registry registry = { 0, };
	bool rc = false;

	kvstore_t *kv = nvs_kvstore_open(WIFI_KVSTORE_NAMESPACE);
	if (kv == NULL) {
		error("cannot open %s kvstore", WIFI_KVSTORE_NAMESPACE);
		return false;
	}

	if (kvstore_write(kv, WIFI_KVSTORE_REGISTRY, &registry, sizeof(registry))
			!= sizeof(registry)) {
		goto out;
	}

	rc = true;
out:
	nvs_kvstore_close(kv);
	return rc;
}

static bool delete_network(const wifiman_network_profile_t *profile)
{
	struct network_registry registry;
	bool rc = false;

	kvstore_t *kv = nvs_kvstore_open(WIFI_KVSTORE_NAMESPACE);
	if (kv == NULL) {
		error("cannot open %s kvstore", WIFI_KVSTORE_NAMESPACE);
		return false;
	}

	load_network_registry(kv, &registry);

	for (uint8_t i = 0; i < WIFIMAN_MAX_NETWORK_PROFILES; i++) {
		if (!registry.index[i].used) {
			continue;
		}

		uint8_t buf[sizeof(wifiman_network_profile_t)
			+ WIFIMAN_PASS_MAXLEN] = { 0, };
		wifiman_network_profile_t *saved = (void *)buf;

		if (!read_network_profile(saved, kv, i)) {
			continue;
		}
		if (strcmp(profile->ssid, saved->ssid) == 0 &&
				memcmp(profile->bssid, saved->bssid,
					WIFIMAN_BSSID_MAXLEN) == 0) {
			registry.index[i].used = false;
			if (update_network_registry(&registry, kv)) {
				rc = true;
			}
			break;
		}
	}

	nvs_kvstore_close(kv);
	return rc;
}

size_t wifiman_count_networks(void)
{
	size_t count;

	pthread_mutex_lock(&m.lock);
	{
		count = count_saved_networks_internal();
	}
	pthread_mutex_unlock(&m.lock);

	return count;
}

bool wifiman_save_network(const wifiman_network_profile_t *profile)
{
	bool rc;

	pthread_mutex_lock(&m.lock);
	{
		rc = save_network_internal(profile);
	}
	pthread_mutex_unlock(&m.lock);

	return rc;
}

bool wifiman_get_network(wifiman_network_profile_t *profile, uint8_t index)
{
	bool rc;

	pthread_mutex_lock(&m.lock);
	{
		rc = get_network_internal(profile, index);
	}
	pthread_mutex_unlock(&m.lock);

	return rc;
}

bool wifiman_clear_networks(void)
{
	bool rc;

	pthread_mutex_lock(&m.lock);
	{
		rc = clear_networks_internal();
	}
	pthread_mutex_unlock(&m.lock);

	return rc;
}

bool wifiman_delete_network(const wifiman_network_profile_t *profile)
{
	bool rc;

	pthread_mutex_lock(&m.lock);
	{
		rc = delete_network(profile);
	}
	pthread_mutex_unlock(&m.lock);

	return rc;
}
