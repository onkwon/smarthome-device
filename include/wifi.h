#ifndef WIFIMAN_H
#define WIFIMAN_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#define WIFIMAN_BSSID_MAXLEN			6
#define WIFIMAN_SSID_MAXLEN			32
#define WIFIMAN_IP4_MAXLEN			4
#define WIFIMAN_MAC_MAXLEN			6

typedef enum {
	WIFIMAN_SUCCESS				= 0,
	WIFIMAN_ERROR,
	WIFIMAN_INVALID_PARAM,
	WIFIMAN_NOT_RESPONDING,
	WIFIMAN_NOT_INITIALIZED,
	WIFIMAN_MODE_ERROR,
	WIFIMAN_WRONG_SETTINGS,
} wifiman_error_t;

typedef enum {
	WIFIMAN_EVENT_UNKNOWN			= 0,
	WIFIMAN_EVENT_READY,
	WIFIMAN_EVENT_CONNECTED,
	WIFIMAN_EVENT_DISCONNECTED,
	WIFIMAN_EVENT_IP_ALLOCATED,
	WIFIMAN_EVENT_SCAN_COMPLETED,
	WIFIMAN_EVENT_AP_STATE_CHANGED,
	WIFIMAN_EVENT_AP_STATION_CONNECTED,
	WIFIMAN_EVENT_AP_STATION_DISCONNECTED,
	WIFIMAN_EVENT_MAX,
} wifiman_event_t;

typedef enum {
	WIFIMAN_SECURITY_AUTO			= 0,
	WIFIMAN_SECURITY_OPEN,
	WIFIMAN_SECURITY_WEP,
	WIFIMAN_SECURITY_WPA,
	WIFIMAN_SECURITY_WPA2,
	WIFIMAN_SECURITY_WPA2_ENTERPRISE,
	WIFIMAN_SECURITY_WPA3,
	WIFIMAN_SECURITY_MAX,
} wifiman_security_t;

typedef struct {
	wifiman_security_t security;
	uint8_t bssid[WIFIMAN_BSSID_MAXLEN];
	char ssid[WIFIMAN_SSID_MAXLEN + 1];
	uint8_t channel;
	int8_t rssi;
} wifiman_network_profile_t;

typedef void (*wifiman_event_handler_t)(wifiman_event_t event, void *context);

bool wifiman_on(void);
void wifiman_off(void);
bool wifiman_reset(void);

wifiman_error_t wifiman_connect(const wifiman_network_profile_t *conf,
		const char *pass);
wifiman_error_t wifiman_disconnect(void);
bool wifiman_is_connected(void);
bool wifiman_get_ip(uint8_t ip[WIFIMAN_IP4_MAXLEN]);
wifiman_error_t wifiman_get_mac(uint8_t mac[WIFIMAN_MAC_MAXLEN]);
int8_t wifiman_get_rssi(void);

int wifiman_scan(wifiman_network_profile_t *scanlist, int maxlen);

wifiman_error_t wifiman_register_event_handler(wifiman_event_t event,
		wifiman_event_handler_t handler);

#if 0 // TODO: implement
	set_mac()

	set_mode()
	get_mode()

	get_country()
	set_country()

	start_ap()
	stop_ap()
	congifure_ap()

	ping(ip, interval_ms, count)

	set_power_mode()
	get_power_mode()
#endif

#if defined(__cplusplus)
}
#endif

#endif /* WIFIMAN_H */
