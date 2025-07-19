#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "sys/socket.h"
#include "errno.h"
#include "arpa/inet.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "esp_tls.h"
#include "esp_check.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_netif.h"
#include "register/soc/gpio_reg.h"
#include "register/soc/uart_reg.h"
#include "nvs_flash.h"
#include "esp_tls_crypto.h"
#include "esp_http_server.h"
#include "lwip/dhcp.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "dhcpserver/dhcpserver.h"
#include "dhcpserver/dhcpserver_options.h"
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "driver/uart.h"
#include "hungarian_wrappers.h"
#include "favicon.h"
#include "download_icon.h"
#include "eject_icon.h"
#include "insert_icon.h"
#include "mkdir_icon.h"
#include "new_image_icon.h"
#include "reload_icon.h"
#include "trash_icon.h"
#include "upload_icon.h"
#include "bootloader.h"
#include "bootloader_hs.h"
#include "driver/rmt_tx.h"
#include "led_strip_encoder.h"

#define ESP_MAXIMUM_RETRY			8
#define CONFIG_ESP_WPA3_SAE_PWE_HUNT_AND_PECK	1
#define CONFIG_ESP_WIFI_AUTH_WPA_WPA2_PSK	1

#if CONFIG_ESP_WPA3_SAE_PWE_HUNT_AND_PECK
#define ESP_WIFI_SAE_MODE	WPA3_SAE_PWE_HUNT_AND_PECK
#define EXAMPLE_H2E_IDENTIFIER	""
#elif CONFIG_ESP_WPA3_SAE_PWE_HASH_TO_ELEMENT
#define ESP_WIFI_SAE_MODE	WPA3_SAE_PWE_HASH_TO_ELEMENT
#define EXAMPLE_H2E_IDENTIFIER	CONFIG_ESP_WIFI_PW_ID
#elif CONFIG_ESP_WPA3_SAE_PWE_BOTH
#define ESP_WIFI_SAE_MODE	WPA3_SAE_PWE_BOTH
#define EXAMPLE_H2E_IDENTIFIER	CONFIG_ESP_WIFI_PW_ID
#endif

#if CONFIG_ESP_WIFI_AUTH_OPEN
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_OPEN
#elif CONFIG_ESP_WIFI_AUTH_WEP
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WEP
#elif CONFIG_ESP_WIFI_AUTH_WPA_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WAPI_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WAPI_PSK
#endif

#define WIFI_CONNECTED_BIT	BIT0
#define WIFI_FAIL_BIT		BIT1
#define WIFI_SCAN_DONE_BIT	BIT2
#define WIFI_DISCONNECTED_BIT	BIT3
#define WIFI_STA_STOP_BIT	BIT4
#define HTTP_QUERY_KEY_MAX_LEN	64

#define WIFI_SCAN_LIST_SIZE	10
#define CREDENTIALS_LIST_SIZE	15
#define YASIO_WIFI_MODE_STA	0x00
#define YASIO_WIFI_MODE_AP	0xff

#define AP_SSID			"tplink_859483"
#define AP_PASS			"tajne_859483"
#define AP_CHANNEL		1

#ifndef MIN
#define MIN(a, b)		(((a) < (b)) ? (a) : (b))
#endif

#define SETUP_SERVER_PORT	6777

#define DEBUG1_PIN		7
#define DEBUG2_PIN		43
#define SETUP_ENABLE_PIN	4

#define PIN_RMT_OUT		21
#define RESOLUTION_HZ		3333333

#define CMD_PIN	11
#define CLK_PIN	10
#define D0_PIN	9
#define D1_PIN	8
#define D2_PIN	13
#define D3_PIN	12

#define NOTIFICATION_CMD_LO		0
#define NOTIFICATION_CMD_HI		1
#define NOTIFICATION_ENABLE_SETUP	2
#define NOTIFICATION_TIMEOUT		3

#define SIO_UART_NUM		UART_NUM_1
#define SIO_CMD_PIN		1
#define SIO_UART_TXD_PIN	3
#define SIO_UART_RXD_PIN	2
#define SIO_BUFFER_SIZE		1024
#define ESP_INTR_FLAG_DEFAULT	0

#define F_OSC			1773447

#define DCMND_FORMAT_DISK		0x21
#define DCMND_FORMAT_MEDIUM		0x22
#define DCNMD_SEND_HIGH_SPEED_INDEX	0x3f
#define DCNMD_HAPPY_CONFIG		0x48
#define DCMND_READ_PERCOM		0x4e
#define DCMND_PUT_SECTOR		0x50
#define DCMND_READ_SECTOR		0x52
#define DCMND_READ_STATUS		0x53
#define DCMND_WRITE_SECTOR		0x57
#define DCMND_GET_IP_ADDRESS		0xf0
#define DCMND_GET_NETWORK		0xf1
#define DCMND_SELECT_NETWORK		0xf2
#define DCMND_GET_WIFI_MODE		0xf3
#define DCMND_SCAN_NETWORKS		0xf4
#define DCMND_GET_AP_PARAMETERS		0xf5
#define DCMND_SET_AP			0xf6
#define DCMND_GET_CHUNK			0xf8
#define DCMND_GET_NEXT_CHUNK		0xf9

#define DRIVE_TYPE_ATR		1
#define DRIVE_TYPE_XEX		2
#define ATR_HEADER_SIZE		16
#define BOOT_SECTOR_SIZE	128
#define BOOT_PATH_OFFSET	0x1c
#define BOOT_PATH_LENGTH	30
#define N_HS_INDICES		9

#define MOUNT_POINT		"/sdcard"
#define YASIO_CONFIG_FNAME	"yasio.conf"
#define YASIO_SETUP_FNAME	"yasio_setup.xex"

struct Drive {
	char path[256];
	uint8_t *buffer;
	int type;
	/* ATR */
	uint32_t total_paragraphs;
	uint32_t sector_size;
	int write_protected;
	uint32_t total_sectors;
	uint32_t last_read_sector;
	uint32_t total_tracks;
	uint32_t sectors_per_track;
	uint32_t size;
	/* COM/EXE/XEX */
	uint32_t chunk_offset;
	uint32_t prev_chunk_size;
};

struct ResponseContext {
	httpd_req_t *req;
	char *buf;
	int buf_size;
	int offset;
};

struct HSIDescriptor {
	int val;
	char *str;
	char *kbps;
};

struct Credentials {
	char ssid[64];
	char pass[64];
};

static const char *TAG = "yasio";

static esp_netif_t *sta_netif;
static esp_netif_t *ap_netif;
static EventGroupHandle_t s_wifi_event_group;
static uint16_t ap_number = WIFI_SCAN_LIST_SIZE;
static wifi_ap_record_t ap_info[WIFI_SCAN_LIST_SIZE];
static uint16_t ap_count = 0;
static struct Credentials credentials[CREDENTIALS_LIST_SIZE];
static char ap_ssid[64];
static char ap_password[64];
static char ap_ip[32];

static SemaphoreHandle_t connection_gate;
static int s_retry_num = 0;
static httpd_handle_t server = NULL;
static char ssid_connected[64];
static char ip_address[16];
static char setup_server_ip[16];

static TaskHandle_t sio_task_handle = NULL;
static struct Drive drives[4];
static esp_timer_handle_t timeout_timer;

static uint32_t high_speed_index = 40;	// default: 18866 bps

static uint8_t led_pixel[3];
static rmt_channel_handle_t led_chan = NULL;
static rmt_tx_channel_config_t tx_chan_config = {
	.clk_src = RMT_CLK_SRC_DEFAULT,
	.gpio_num = PIN_RMT_OUT,
	.mem_block_symbols = 64,
	.resolution_hz = RESOLUTION_HZ,
	.trans_queue_depth = 4,
	.flags.invert_out = false,
};
static rmt_encoder_handle_t led_encoder = NULL;
static rmt_transmit_config_t tx_config = {
	.loop_count = 0,
};

const char *main_view_header =
"<!DOCTYPE html>\n"
"<html>\n"
" <head>\n"
"  <style>.direntry{font-family:courier} .img-size img {width: 16px; height: 16px} .sep-col td {width: 60 px}</style>\n"
" </head>\n"
" <body>\n";
const char *main_view_full_drive_entry =
"  <div>\n"
"   <table>\n"
"    <tr class='direntry'>\n"
"     <td>\n"
"      <form method='post' action='$uri'>\n"
"       <button class='img-size'><img src='/eject_icon.png' alt='Eject'/></button>\n"
"       <input type='hidden' name='op' value='eject'/>\n"
"       <input type='hidden' name='drive' value='$1'/>\n"
"      </form>\n"
"     </td>\n"
"     <td>D$1:</td>\n"
"     <td>$2</td>\n"
"    </tr>\n"
"   </table>\n"
"  </div>";
const char *main_view_empty_drive_entry =
"  <div>\n"
"    <table>\n"
"      <tr class='direntry'>\n"
"       <td><button class='img-size' disabled><img src='/eject_icon.png' alt='Eject'/></button></td>\n"
"       <td>D$1:</td>\n"
"       <td>(empty)</td>\n"
"      </tr>\n"
"    </table>\n"
"  </div>\n";
const char *main_view_reload_setup =
"  <div>\n"
"   <form method='post' action='$uri'>\n"
"    <input type='hidden' name='op' value='reload'/>\n"
"    <button class='img-size'><img src='/reload_icon.png' alt='Reload'/></button>\n"
"   </form>\n"
"  </div>\n";
const char *main_view_save_state =
"  <div>\n"
"    <form method='post' action='$uri'>\n"
"      <input type='hidden' name='op' value='save'/>\n"
"      <button>Save state</button>\n"
"    </form>\n"
"  </div>\n";
const char *main_view_hsi_head =
"  <div>\n"
"   <form method='post' action='$uri'>High Speed Index:\n"
"    <select name='hsi' id='hsi'>\n";
const char *main_view_hsi_entry =
"     <option value='$1'$2>$1 ($3 kbps)</option>\n";
const char *main_view_hsi_tail =
"    </select>\n"
"    <input type='hidden' name='op' value='selhsi'/>\n"
"    <button type='submit'>Select</button>\n"
"   </form>\n"
"  </div>\n";
const char *main_view_mkdir =
"  <div>\n"
"   <form method='post' action='$uri'>\n"
"    <input type='text' placeholder='Directory name' id='dirname' name='dirname' required/>\n"
"    <input type='hidden' name='op' value='mkdir'/>\n"
"    <button class='img-size'><img src='/mkdir_icon.png' alt='Make dir'/></button>\n"
"   </form>\n"
"  </div>\n";
const char *main_view_upload_file =
"  <div>\n"
"   <form method='post' enctype='multipart/form-data' action='$uri'>\n"
"    <input type='file' id='upfile' name='write-file'/>\n"
"    <button class='img-size'><img src='/upload_icon.png' alt='Upload'/></button>\n"
"   </form>\n"
"  </div>\n";
const char *main_view_create_empty_disk =
"  <div>\n"
"   <form method='post' action='$uri'>\n"
"    New image:<input type='text' placeholder='File name' id='filename' name='filename' required/>\n"
"    <input type='hidden' name='op' value='mkimg'/>\n"
"    <select name='density' id='density'>\n"
"     <option value='SD'>SD (90k)</option>\n"
"     <option value='ED'>ED (130k)</option>\n"
"     <option value='DD'>DD (180k)</option>\n"
"     <option value='QD'>QD (360k)</option>\n"
"     <option value='HD'>HD (720k)</option>\n"
"    </select>\n"
"    <button class='img-size'><img src='/new_image_icon.png' alt='New image'/></button>\n"
"   </form>\n"
"  </div>\n";
const char *main_view_dirlist_head =
"  <table>\n";
const char *main_view_dirlist_parent =
"   <tr class='direntry'>\n"
"    <td></td>\n"
"    <td></td>\n"
"    <td></td>\n"
"    <td></td>\n"
"    <td></td>\n"
"    <td></td>\n"
"    <td></td>\n"
"    <td></td>\n"
"    <td><a href='$1'>Parent directory</a></td>\n"
"   </tr>";
const char *main_view_dirlist_dir =
"   <tr class='direntry'>\n"
"    <td></td>\n"
"    <td></td>\n"
"    <td></td>\n"
"    <td></td>\n"
"    <td></td>\n"
"    <td>\n"
"     <form method='post' action='$uri'>\n"
"      <button class='img-size'><img src='/trash_icon.png' alt='Delete'/></button>\n"
"      <input type='hidden' name='op' value='rmdir'/>\n"
"      <input type='hidden' name='dirname' value='$1'/>\n"
"     </form>\n"
"    </td>\n"
"    <td></td>\n"
"    <td class='sep-col'></td>\n"
"    <td class='text'><a href='$uri1/$1'>$1/</a></td>\n"
"   </tr>\n";
const char *main_view_dirlist_file =
"   <tr class='direntry'>\n"
"    <td>\n"
"     <form method='post' action='$uri1/$1'>\n"
"      <input type='hidden' name='op' value='insert'/>\n"
"      <input type='hidden' name='drive' value='1'/>\n"
"      <button class='img-size'><img src='/insert_icon.png' alt='Insert D1:'/></button>\n"
"     </form>\n"
"    </td>\n"
"    <td>\n"
"     <form method='post' action='$uri1/$1'>\n"
"      <input type='hidden' name='op' value='insert'/>\n"
"      <input type='hidden' name='drive' value='2'/>\n"
"      <button class='img-size'><img src='/insert_icon.png' alt='Insert D2:'/></button>\n"
"     </form>\n"
"    </td>\n"
"    <td>\n"
"     <form method='post' action='$uri1/$1'>\n"
"      <input type='hidden' name='op' value='insert'/>\n"
"      <input type='hidden' name='drive' value='3'/>\n"
"      <button class='img-size'><img src='/insert_icon.png' alt='Insert D3:'/></button>\n"
"     </form>\n"
"    </td>\n"
"    <td>\n"
"     <form method='post' action='$uri1/$1'>\n"
"      <input type='hidden' name='op' value='insert'/>\n"
"      <input type='hidden' name='drive' value='4'/>\n"
"      <button class='img-size'><img src='/insert_icon.png' alt='Insert D4:'/></button>\n"
"     </form>\n"
"    </td>\n"
"    <td>\n"
"     <a href='$uri1/$1?op=download' download='$1'><button class='img-size'><img src='/download_icon.png' alt='Download'/></button></a>\n"
"    </td>\n"
"    <td>\n"
"     <form method='post' action='$uri'>\n"
"      <input type='hidden' name='op' value='rmfile'/>\n"
"      <input type='hidden' name='filename' value='$1'/>\n"
"      <button class='img-size'><img src='/trash_icon.png' alt='Delete'/></button>\n"
"     </form>\n"
"    </td>\n"
"    <td class='text'>$2</td>\n"
"    <td class='sep-col'></td>\n"
"    <td class='text'>$1</td>\n"
"   </tr>\n";
const char *main_view_dirlist_tail =
"  </table>\n";
const char *main_view_tail =
" </body>\n"
"</html>";

const char *error_view =
"<!DOCTYPE html>\n"
"<html>\n"
" <head></head>\n"
" <body>\n"
"  <div>$1</div>\n"
"  <button onclick=\"location.href='$uri'\">Go back</button>\n"
" </body>\n"
"</html>";

const struct HSIDescriptor hsi_lut[N_HS_INDICES] = {
	{40, "40", "18.9"},
	{16, "16", "38.5"},
	{10, "10", "52.5"},
	{9, "9", "55.4"},
	{8, "8", "59.1"},
	{6, "6", "68.2"},
	{4, "4", "80.4"},
	{2, "2", "98.5"},
	{1, "1", "110.8"}
};

static void set_rgb(uint8_t r, uint8_t g, uint8_t b)
{
	led_pixel[0] = r;
	led_pixel[1] = g;
	led_pixel[2] = b;
	ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_pixel, sizeof(led_pixel), &tx_config));
}

static int load_atr(int drive, int size)
{
	uint8_t *header = drives[drive].buffer;

	if (header[0] != 0x96 || header[1] != 0x02) {
		return -1;
	}

	drives[drive].total_paragraphs = ((uint32_t) header[2] & 0xff) | (((uint32_t) header[3] & 0xff) << 8) | (((uint32_t) header[6] & 0xff) << 16);
	drives[drive].sector_size = ((uint32_t) header[4] & 0xff) | (((uint32_t) header[5] & 0xff) << 8);
	drives[drive].write_protected = (header[15] & 1) == 1;
	if (drives[drive].sector_size == 128) {
		drives[drive].total_sectors = 16*drives[drive].total_paragraphs / drives[drive].sector_size;
	} else if (drives[drive].sector_size == 512) {
		drives[drive].total_sectors = 16*drives[drive].total_paragraphs / drives[drive].sector_size;
	} else if (drives[drive].sector_size == 256) {
		if ((drives[drive].total_paragraphs & 0x0f) == 0x00) {
			drives[drive].total_sectors = 16*drives[drive].total_paragraphs / drives[drive].sector_size;
		} else if ((drives[drive].total_paragraphs & 0x0f) == 0x08) {
			drives[drive].total_sectors = 16*(drives[drive].total_paragraphs + 24) / drives[drive].sector_size;
		}
	}
	if (drives[drive].total_sectors % 80 == 0) {
		drives[drive].total_tracks = 80;
	} else {
		drives[drive].total_tracks = 40;
	}
	drives[drive].sectors_per_track = drives[drive].total_sectors / drives[drive].total_tracks;
	drives[drive].type = DRIVE_TYPE_ATR;

	drives[drive].size = size;

	return 0;
}

static int load_xex(uint32_t size)
{
	uint8_t *header = drives[0].buffer;
	int l = MIN(BOOT_PATH_LENGTH, strlen(drives[0].path));
	int i;

	if (header[0] != 0xff || header[1] != 0xff) {
		return -1;
	}

	for (i = 0; i < l; i++) {
		bootloader[BOOT_PATH_OFFSET+i] = drives[0].path[i];
		bootloader_hs[BOOT_PATH_OFFSET+i] = drives[0].path[i];
	}
	bootloader[BOOT_PATH_OFFSET+i] = 0x9b;
	bootloader_hs[BOOT_PATH_OFFSET+i] = 0x9b;

	drives[0].type = DRIVE_TYPE_XEX;
	drives[0].size = size;
	drives[0].chunk_offset = 2;
	drives[0].prev_chunk_size = 0;

	uart_set_baudrate(SIO_UART_NUM, (F_OSC/2) / (7 + 40));

	return 0;
}

static int ends_with(char *s, char *postfix)
{
	int i;

	for (i = 0; s[i]; i++);
	return i >= strlen(postfix) && !strcmp(postfix, s + i - strlen(postfix));
}

static esp_err_t insert_image(char *path, int drive)
{
	FILE *fp;
	char *f_buf;
	int f_buf_len;

	fp = fopen(path, "rb");
	if (fp == NULL) {
		ESP_LOGE(TAG, "fopen error '%s'", path);
		return ESP_FAIL;
	}
	fseek(fp, 0, SEEK_END);
	f_buf_len = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	f_buf = malloc(f_buf_len);
	if (f_buf == NULL) {
		ESP_LOGE(TAG, "malloc %d failed", f_buf_len);
		fclose(fp);
		return ESP_FAIL;
	}
	fread(f_buf, 1, f_buf_len, fp);
	fclose(fp);

	ESP_LOGI(TAG, "inserting file '%s' (%d) into D%d", path, f_buf_len, drive + 1);

	if (drive >= 0 && drive <= 3) {
		if (drives[drive].buffer != NULL) {
			free(drives[drive].buffer);
		}
		drives[drive].buffer = (uint8_t *) f_buf;
		strcpy(drives[drive].path, path + sizeof(MOUNT_POINT));
		if ((ends_with(path, ".atr") || ends_with(path, ".ATR")) && load_atr(drive, f_buf_len) == 0) {
			ESP_LOGI(TAG, "file inserted");
		} else if ((ends_with(path, ".xex") || ends_with(path, ".XEX") || ends_with(path, ".com") || ends_with(path, ".COM"))
				&& drive == 0 && load_xex(f_buf_len) == 0) {
			ESP_LOGI(TAG, "file inserted");
		} else {
			ESP_LOGE(TAG, "file insertion failed");
			free(drives[drive].buffer);
			drives[drive].path[0] = 0;
			// TODO: show error message
		}
		return ESP_OK;
	} else {
		return ESP_FAIL;
	}
}

static void sort_insert(char **names, char *name)
{
	int n;
	char *tmp;

	if (names[0] == NULL) {
		names[0] = name;
		names[1] = NULL;
		return;
	}
	for (n = 0; names[n] != NULL; n++);
	names[n+1] = NULL;
	names[n] = name;
	for (; strcmp(names[n], names[n-1]) < 0; n--) {
		tmp = names[n];
		names[n] = names[n-1];
		names[n-1] = tmp;
		if (n == 1) break;
	}
}

static void build_effective_path(httpd_req_t *req, char *name, char *path)
{
	char *dest;
	int i;

	strcpy(path, MOUNT_POINT);

	if (strcmp(req->uri, "/")) {
		strcat(path, req->uri);
	}

	dest = strcat(path, "/");

	for (dest = path; *dest; dest++);
	i = 0;
	while (name[i]) {
		if (name[i] == '+') {
			*dest++ = ' ';
			i++;
		} else if (!strncmp(name + i, "%2B", 3)) {
			*dest++ = '+';
			i += 3;
		} else if (!strncmp(name + i, "%2C", 3)) {
			*dest++ = ',';
			i += 3;
		} else if (!strncmp(name + i, "%28", 3)) {
			*dest++ = '(';
			i += 3;
		} else if (!strncmp(name + i, "%29", 3)) {
			*dest++ = ')';
			i += 3;
		} else {
			*dest++ = name[i++];
		}
	}
	*dest = 0;
}

static void add_response_data(struct ResponseContext *ctx, const char *data, ...)
{
	int i;
	char *src = (char *) data;
	char *src_stack = NULL;
	va_list ap;
	char *args[4];
	int n_args = 0;

	va_start(ap, data);

	while (*src || src_stack != NULL) {
		if (*src == 0) {
			src = src_stack;
			src_stack = NULL;
		}
		if (*src == '$') {
			if (!strncmp(src + 1, "uri1", 4)) {
				if (strcmp(ctx->req->uri, "/")) {
					src_stack = src + 5;
					src = (char *) ctx->req->uri;
				} else {
					src = src + 5;
				}
			} else if (!strncmp(src + 1, "uri", 3)) {
				src_stack = src + 4;
				src = (char *) ctx->req->uri;
			} else {
				for (i = src[1] - '0'; n_args < i; n_args++) {
					args[n_args] = va_arg(ap, char *);
				}
				src_stack = src + 2;
				src = args[i - 1];
			}
			if (*src == 0) {			/* zero-length argument => skip it immediately */
				src = src_stack;
				src_stack = NULL;
			}
		}
		ctx->buf[ctx->offset++] = *src++;
		if (ctx->offset == ctx->buf_size) {
			httpd_resp_send_chunk(ctx->req, ctx->buf, ctx->offset);
			ctx->offset = 0;
		}
	}

	va_end(ap);
}

static esp_err_t http_show_main_screen(httpd_req_t *req)
{
	struct ResponseContext ctx;
	char dirname[600];
	DIR *dir;
	struct dirent *entry;
	int offset = 0;
	char *data;
	char **dir_names;
	char **file_names;
	int n_dir_names = 0;
	int n_file_names = 0;
	char drive_num[2];
	char f_size[12];
	char path[256];
	struct stat sb;
	int n;

	sprintf(dirname, "%s%s", MOUNT_POINT, req->uri);
	dir = opendir(dirname);
	if (dir != NULL) {
		ESP_LOGI(TAG, "Opened directory %s", dirname);

		data = malloc(8192);
		if (data == NULL) {
			ESP_LOGE(TAG, "malloc 8192 failed (data)");
			return ESP_FAIL;
		}
		dir_names = malloc(64 * sizeof(char *));
		if (dir_names == NULL) {
			ESP_LOGE(TAG, "malloc %d failed", 64 * sizeof(char *));
			return ESP_FAIL;
		}
		file_names = malloc(256 * sizeof(char *));
		if (file_names == NULL) {
			ESP_LOGE(TAG, "malloc %d failed", 256 * sizeof(char *));
			return ESP_FAIL;
		}
		ESP_LOGI(TAG, "reading directory '%s'", dirname);
		dir_names[0] = NULL;
		file_names[0] = NULL;
		while ((entry = readdir(dir)) != NULL) {
			strcpy(data + offset, entry->d_name);
			if (entry->d_type == DT_DIR) {
				sort_insert(dir_names, data + offset);
				n_dir_names++;
			} else {
				sort_insert(file_names, data + offset);
				n_file_names++;
			}
			offset += strlen(entry->d_name) + 1;
		}
		closedir(dir);

		ctx.req = req;
		ctx.buf_size = 16384;
		ctx.buf = malloc(ctx.buf_size);
		ctx.offset = 0;
		offset = 0;

		if (ctx.buf == NULL) {
			ESP_LOGE(TAG, "response buffer malloc failed (%d)", ctx.buf_size);
			return ESP_FAIL;
		}

		add_response_data(&ctx, main_view_header);
		drive_num[1] = 0;
		for (n = 0; n < 4; n++) {
			drive_num[0] = '1' + n;
			if (drives[n].path[0]) {
				add_response_data(&ctx, main_view_full_drive_entry, drive_num, drives[n].path);
			} else {
				add_response_data(&ctx, main_view_empty_drive_entry, drive_num);
			}
		}
		add_response_data(&ctx, main_view_reload_setup);
		add_response_data(&ctx, main_view_save_state);
		add_response_data(&ctx, main_view_hsi_head);
		for (n = 0; n < N_HS_INDICES; n++) {
			add_response_data(&ctx, main_view_hsi_entry, hsi_lut[n].str, high_speed_index == hsi_lut[n].val ? " selected" : "", hsi_lut[n].kbps);
		}
		add_response_data(&ctx, main_view_hsi_tail);
		add_response_data(&ctx, main_view_mkdir);
		add_response_data(&ctx, main_view_upload_file);
		add_response_data(&ctx, main_view_create_empty_disk);
		add_response_data(&ctx, main_view_dirlist_head);
		if (strcmp(req->uri, "/")) {
			for (n = 0; req->uri[n] != 0; n++);
			while (req->uri[n] != '/') n--;
			if (n == 0) {
				strncpy(dirname, "/", 2);
			} else {
				strncpy(dirname, req->uri, n);
				dirname[n] = 0;
			}
			add_response_data(&ctx, main_view_dirlist_parent, dirname);
		}
		for (n = 0; n < n_dir_names; n++) {
			add_response_data(&ctx, main_view_dirlist_dir, dir_names[n]);
		}
		for (n = 0; n < n_file_names; n++) {
			build_effective_path(req, file_names[n], path);
			stat(path, &sb);
			sprintf(f_size, "%d", (int) sb.st_size);
			add_response_data(&ctx, main_view_dirlist_file, file_names[n], f_size);
		}
		add_response_data(&ctx, main_view_dirlist_tail);
		add_response_data(&ctx, main_view_tail);

		if (ctx.offset > 0) {
			httpd_resp_send_chunk(req, ctx.buf, ctx.offset);
		}
		httpd_resp_send_chunk(req, NULL, 0);

		free(ctx.buf);
		free(data);
		free(dir_names);
		free(file_names);

		return ESP_OK;
	} else {
		ESP_LOGE(TAG, "Failed to open dir %s", dirname);
		httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Directory not found");
		return ESP_FAIL;
	}
}

static void http_redirect(httpd_req_t *req, int strip_file)
{
	char buf[256];
	int i;

	for (i = 0; req->uri[i] != '?' && req->uri[i] != 0; i++) {
		buf[i] = req->uri[i];
	}
	if (strip_file) {
		while (buf[--i] != '/');
		if (i == 0) {
			buf[1] = 0;
		} else {
			buf[i] = 0;
		}
	} else {
		buf[i] = 0;
	}
	ESP_LOGI(TAG, "going to '%s'", buf);

	httpd_resp_set_status(req, "301");
	httpd_resp_set_hdr(req, "Location", buf);
	httpd_resp_send_chunk(req, NULL, 0);
}

static int atr_sector_coordinates(int drive, int sector_number, uint8_t **data, uint32_t *size, uint32_t *offset_out)
{
	uint32_t offset = 0;

	if (drives[drive].sector_size == 128 || drives[drive].sector_size == 512) {
		offset = ATR_HEADER_SIZE + (sector_number - 1) * drives[drive].sector_size;
		*size = drives[drive].sector_size;
	} else if (drives[drive].sector_size == 256) {
		if ((drives[drive].total_paragraphs & 0x0f) == 0x00) {
			if (sector_number < 4) {
				offset = ATR_HEADER_SIZE + (sector_number - 1) * 128;
				*size = 128;
			} else {
				offset = ATR_HEADER_SIZE + 768 + (sector_number - 4) * 256;
				*size = 256;
			}
		} else if ((drives[drive].total_paragraphs & 0x0f) == 0x08) {
			if (sector_number < 4) {
				offset = ATR_HEADER_SIZE + (sector_number - 1) * 128;
				*size = 128;
			} else {
				offset = ATR_HEADER_SIZE + 384 + (sector_number - 4) * 256;
				*size = 256;
			}
		}
	}
	if (offset + drives[drive].sector_size >= drives[drive].size) {
		return -1;
	}
	*data = drives[drive].buffer + offset;
	if (offset_out != NULL) {
		*offset_out = offset;
	}
	return 0;
}

static int xex_sector_coordinates(int sector_number, uint8_t **data, uint32_t *size)
{
	uint8_t *selected_bootloader = bootloader_hs;
	uint32_t selected_bootloader_size = 1024;
	if (sector_number - 1 < selected_bootloader_size / BOOT_SECTOR_SIZE) {
		*size = BOOT_SECTOR_SIZE;
		*data = selected_bootloader + BOOT_SECTOR_SIZE * (sector_number - 1);
		return 0;
	} else {
		*size = 0;
		*data = NULL;
		return -1;
	}
}

static esp_err_t http_get_handler(httpd_req_t *req)
{
	char *buf;
	size_t buf_len;
	char op[10];
	FILE *fp;
	char filename[128];
	int i;
	int j;

	ESP_LOGI(TAG, "GET URI: '%s'", req->uri);

	if (!strcmp(req->uri, "/favicon.ico")) {
		httpd_resp_set_type(req, "image/x-icon");
		httpd_resp_send(req, favicon, sizeof(favicon));
		return ESP_OK;
	}

	if (!strcmp(req->uri, "/download_icon.png")) {
		httpd_resp_set_type(req, "image/png");
		httpd_resp_send(req, download_icon, sizeof(download_icon));
		return ESP_OK;
	}

	if (!strcmp(req->uri, "/eject_icon.png")) {
		httpd_resp_set_type(req, "image/png");
		httpd_resp_send(req, eject_icon, sizeof(eject_icon));
		return ESP_OK;
	}

	if (!strcmp(req->uri, "/insert_icon.png")) {
		httpd_resp_set_type(req, "image/png");
		httpd_resp_send(req, insert_icon, sizeof(insert_icon));
		return ESP_OK;
	}

	if (!strcmp(req->uri, "/mkdir_icon.png")) {
		httpd_resp_set_type(req, "image/png");
		httpd_resp_send(req, mkdir_icon, sizeof(mkdir_icon));
		return ESP_OK;
	}

	if (!strcmp(req->uri, "/new_image_icon.png")) {
		httpd_resp_set_type(req, "image/png");
		httpd_resp_send(req, new_image_icon, sizeof(new_image_icon));
		return ESP_OK;
	}

	if (!strcmp(req->uri, "/reload_icon.png")) {
		httpd_resp_set_type(req, "image/png");
		httpd_resp_send(req, reload_icon, sizeof(reload_icon));
		return ESP_OK;
	}

	if (!strcmp(req->uri, "/trash_icon.png")) {
		httpd_resp_set_type(req, "image/png");
		httpd_resp_send(req, trash_icon, sizeof(trash_icon));
		return ESP_OK;
	}

	if (!strcmp(req->uri, "/upload_icon.png")) {
		httpd_resp_set_type(req, "image/png");
		httpd_resp_send(req, upload_icon, sizeof(upload_icon));
		return ESP_OK;
	}

	op[0] = 0;
	buf_len = httpd_req_get_url_query_len(req) + 1;
	if (buf_len > 1) {
		buf = malloc(buf_len);
		ESP_RETURN_ON_FALSE(buf, ESP_ERR_NO_MEM, TAG, "buffer alloc failed");
		if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
			if (httpd_query_key_value(buf, "op", op, sizeof(op)) == ESP_OK) {
				// nop
			}
		}
		free(buf);
	}

	if (op[0] == 0) {
		ESP_LOGI(TAG, "listing directory %s", req->uri);
		return http_show_main_screen(req);
	} else if (!strcmp(op, "download")) {
		strcpy(filename, MOUNT_POINT);
		for (i = 0; filename[i] != 0; i++);
		for (j = 0; req->uri[j] != '?'; j++) {
			if (!strncmp(req->uri + j, "%20", 3)) {
				j += 2;
				filename[i++] = ' ';
			} else {
				filename[i++] = req->uri[j];
			}
		}
		filename[i] = 0;
		fp = fopen(filename, "rb");
		if (fp == NULL) {
			ESP_LOGE(TAG, "fopen error '%s'", filename);
			return ESP_FAIL;
		}
		fseek(fp, 0, SEEK_END);
		buf_len = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		buf = malloc(buf_len);
		if (buf == NULL) {
			ESP_LOGE(TAG, "malloc %d failed", buf_len);
			fclose(fp);
			return ESP_FAIL;
		}
		fread(buf, 1, buf_len, fp);
		fclose(fp);
		ESP_LOGI(TAG, "downloading file '%s' (%d)", filename, buf_len);
		httpd_resp_set_type(req, "application/octet-stream");
		httpd_resp_send(req, buf, buf_len);
		free(buf);
		http_redirect(req, 1);
		return ESP_OK;
	}

	return ESP_ERR_NOT_FOUND;
}

static const httpd_uri_t root_get_uri = {
	.uri       = "/*",
	.method    = HTTP_GET,
	.handler   = http_get_handler,
	.user_ctx  = NULL
};

static void http_show_error(httpd_req_t *req, char *message)
{
	struct ResponseContext ctx;

	ctx.req = req;
	ctx.buf_size = 1024;
	ctx.buf = malloc(ctx.buf_size);
	ctx.offset = 0;
	httpd_resp_set_status(req, "403");
	add_response_data(&ctx, error_view, message);
	httpd_resp_send_chunk(req, ctx.buf, ctx.offset);
	httpd_resp_send_chunk(req, NULL, 0);
	free(ctx.buf);
}

#define STATE_NAME	0
#define STATE_VALUE	1
#define DENSITY_SD	0
#define DENSITY_ED	1
#define DENSITY_DD	2
#define DENSITY_QD	3
#define DENSITY_HD	4

static esp_err_t handle_post_urlencoded(httpd_req_t *req)
{
	char buffer[256];
	char path[540];
	int ret;
	char *op = NULL;
	char *name = NULL;
	int i = 0;
	int j;
	int drive = -1;
	FILE *fp;
	char *f_buf;
	int f_buf_len;
	int hsi = 0;
	int density = -1;
	uint32_t total_tracks;
	uint32_t sectors_per_track;
	uint32_t sector_size;
	uint32_t total_sectors;
	uint32_t size;
	uint32_t total_paragraphs;
	char *data;
	char *p;
	struct Drive *d;

	ESP_LOGI(TAG, "handle post urlencoded %s", req->uri);

	while (i < req->content_len) {
		if ((ret = httpd_req_recv(req, buffer + i, MIN(req->content_len - i, sizeof(buffer)))) <= 0) {
			if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
				continue;
			}
			return ESP_FAIL;
		}
		//ESP_LOGI(TAG, "========= received data:");
		//ESP_LOGI(TAG, "%.*s", ret, buffer);
		//ESP_LOGI(TAG, "=========");
		i += ret;
	}
	i = 0;
	while (i < req->content_len) {
		if (req->content_len - i >= 2 && !strncmp(buffer + i, "op", 2)) {
			i += 2;
			op = buffer + i + 1;
		} else if (req->content_len - i >= 7 && !strncmp(buffer + i, "dirname", 7)) {
			i += 7;
			name = buffer + i + 1;
		} else if (req->content_len - i >= 8 && !strncmp(buffer + i, "filename", 8)) {
			i += 8;
			name = buffer + i + 1;
		} else if (req->content_len - i >= 5 && !strncmp(buffer + i, "drive", 5)) {
			i += 5;
			drive = buffer[i + 1] - '1';
		} else if (req->content_len - i >= 3 && !strncmp(buffer + i, "hsi", 3)) {
			i += 3;
			for (j = i + 1, hsi = 0; j < req->content_len && buffer[j] != '&'; j++) {
				hsi = (10 * hsi) + (buffer[j] - '0');
			}
		} else if (req->content_len - i >= 7 && !strncmp(buffer + i, "density", 7)) {
			i += 7;
			if (!strncmp(buffer + i + 1, "SD", 2)) {
				density = DENSITY_SD;
			} else if (!strncmp(buffer + i + 1, "ED", 2)) {
				density = DENSITY_ED;
			} else if (!strncmp(buffer + i + 1, "DD", 2)) {
				density = DENSITY_DD;
			} else if (!strncmp(buffer + i + 1, "QD", 2)) {
				density = DENSITY_QD;
			} else if (!strncmp(buffer + i + 1, "HD", 2)) {
				density = DENSITY_HD;
			}
		} else {
			httpd_resp_set_status(req, "400");
			return ESP_FAIL;
		}
		if (req->content_len - i < 1 || buffer[i++] != '=') {
			//ESP_LOGE(TAG, "expected '=' at %d", i);
			httpd_resp_set_status(req, "400");
			return ESP_FAIL;
		}
		while (buffer[i] != '&' && i < req->content_len) {
			i++;
		}
		if (i == req->content_len || buffer[i] == '&') {
			buffer[i++] = '\0';
		} else {
			//ESP_LOGE(TAG, "expected EOF or '&' at %d", i);
			httpd_resp_set_status(req, "400");
			return ESP_FAIL;
		}
	}
	/*
	for (i = 0; i < req->content_len; i++) {
		ESP_LOGI(TAG, "[%d] %c", i, buffer[i]);
	}
	*/
	if (!strcmp(op, "mkdir")) {
		ESP_LOGI(TAG, "mkdir");
		build_effective_path(req, name, path);
		ESP_LOGI(TAG, "making directory '%s'", path);
		if (mkdir(path, 0700) == 0) {
			http_redirect(req, 0);
		} else {
			http_show_error(req, "Directory creation failed");
		}
	} else if (!strcmp(op, "rmdir")) {
		build_effective_path(req, name, path);
		ESP_LOGI(TAG, "removing directory %s", path);
		if (rmdir(path) == 0) {
			http_redirect(req, 0);
		} else {
			http_show_error(req, "Directory removal failed");
		}
	} else if (!strcmp(op, "rmfile")) {
		build_effective_path(req, name, path);
		ESP_LOGI(TAG, "removing file '%s'", path);
		for (i = 0; i < 4; i++) {
			if (drives[i].path[0] && !strcmp(drives[i].path, path + sizeof(MOUNT_POINT))) {
				break;
			}
		}
		if (i < 4) {									// file inserted in drive (i+1)
			sprintf(path, "file inserted in drive %d, cannot remove", i + 1);	// hack: reuse array to create error message
			ESP_LOGE(TAG, "%s", path);
			http_show_error(req, path);
		} else if (unlink(path) == 0) {
			http_redirect(req, 0);
		} else {
			http_show_error(req, "File removal failed");
		}
	} else if (!strcmp(op, "insert")) {
		strcpy(path, MOUNT_POINT);
		for (i = 0; path[i]; i++);
		for (j = 0; req->uri[j]; j++) {
			if (!strncmp(req->uri + j, "%20", 3)) {
				j += 2;
				path[i++] = ' ';
			} else {
				path[i++] = req->uri[j];
			}
		}
		path[i] = 0;
		if (insert_image(path, drive) != ESP_OK) {
			ESP_LOGE(TAG, "failed to insert '%s' into D%d", path, drive + 1);
		}
		http_redirect(req, 1);
	} else if (!strcmp(op, "eject")) {
		if (drives[drive].path[0] != 0) {
			drives[drive].path[0] = 0;
			free(drives[drive].buffer);
			drives[drive].buffer = NULL;
		}
		http_redirect(req, 0);
		return ESP_OK;
	} else if (!strcmp(op, "selhsi")) {
		ESP_LOGI(TAG, "setting high speed index %d", hsi);
		high_speed_index = hsi;
		http_redirect(req, 0);
		return ESP_OK;
	} else if (!strcmp(op, "mkimg")) {
		build_effective_path(req, name, path);
		ESP_LOGI(TAG, "creating image '%s' %d", path, density);
		if ((fp = fopen(path, "r")) != NULL) {
			fclose(fp);
			http_show_error(req, "file already exists");
			return ESP_OK;
		}
		switch (density) {
			case DENSITY_SD:
				total_tracks = 40;
				sectors_per_track = 18;
				sector_size = 128;
				break;

			case DENSITY_ED:
				total_tracks = 40;
				sectors_per_track = 26;
				sector_size = 128;
				break;

			case DENSITY_DD:
				total_tracks = 40;
				sectors_per_track = 18;
				sector_size = 256;
				break;

			case DENSITY_QD:
				total_tracks = 80;
				sectors_per_track = 18;
				sector_size = 256;
				break;

			case DENSITY_HD:
				total_tracks = 80;
				sectors_per_track = 36;
				sector_size = 256;
				break;

			default:
				total_tracks = 0;
				sectors_per_track = 0;
				sector_size = 0;
				break;
		}
		total_sectors = total_tracks * sectors_per_track;
		size = 3*128 + (total_sectors - 3) * sector_size;
		total_paragraphs = size >> 4;
		fp = fopen(path, "wb");
		if (fp != NULL) {
			f_buf_len = size + 16;
			f_buf = malloc(f_buf_len);
			if (f_buf == NULL) {
				ESP_LOGE(TAG, "malloc %d failed", f_buf_len);
				fclose(fp);
				http_show_error(req, "cannot allocate memory");
				return ESP_FAIL;
			}
			memset(f_buf, 0, f_buf_len);
			f_buf[0] = 0x96;
			f_buf[1] = 0x02;
			f_buf[2] = total_paragraphs & 0xff;
			f_buf[3] = (total_paragraphs >> 8) & 0xff;
			f_buf[4] = sector_size & 0xff;
			f_buf[5] = (sector_size >> 8) & 0xff;
			f_buf[6] = (total_paragraphs >> 16) & 0xff;
			fwrite(f_buf, 1, size + 16, fp);
			fclose(fp);
			http_redirect(req, 0);
		} else {
			http_show_error(req, "cannot create file");
			return ESP_FAIL;
		}
	} else if (!strcmp(op, "save")) {
		size = 8192;
		data = malloc(size);
		if (data == NULL) {
			return ESP_FAIL;
		}
		p = data;

		fp = fopen(MOUNT_POINT "/" YASIO_CONFIG_FNAME, "rt");
		if (fp == NULL) {
			free(data);
			http_show_error(req, "error saving state");
			return ESP_FAIL;
		}
		size = fread(data, 1, size, fp);
		fclose(fp);

		fp = fopen(MOUNT_POINT "/" YASIO_CONFIG_FNAME, "wt");
		i = 0;
		while (i < size) {
			p = data + i;
			if (p[0] == 'D' && '1' <= p[1] && p[1] <= '4' && p[2] == ':') {
				while (data[i++] != '\n');
				fwrite(p, 1, 3, fp);
				d = &drives[p[1] - '1'];
				if (d->path[0] != 0) {
					fwrite(d->path, 1, strlen(d->path), fp);
				}
				fputc('\n', fp);
			} else if (!strncmp(p, "ST:", 3) || !strncmp(p, "AP:", 3) || !strncmp(p, "MO:", 3)) {
				j = i;
				while (data[i++] != '\n');
				fwrite(p, 1, i - j, fp);
			}
		}
		fclose(fp);

		free(data);

		http_redirect(req, 0);
		return ESP_OK;
	} else if (!strcmp(op, "reload")) {
		ESP_LOGI(TAG, "reload");
		if (drives[0].path[0]) {
			drives[0].path[0] = 0;
			free(drives[0].buffer);
			drives[0].buffer = NULL;
		}

		struct sockaddr_in dest_addr;
		inet_pton(AF_INET, setup_server_ip, &dest_addr.sin_addr);
		dest_addr.sin_family = AF_INET;
		dest_addr.sin_port = htons(SETUP_SERVER_PORT);
		int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
		if (sock < 0) {
			ESP_LOGE(TAG, "unable to open socket %d", errno);
		}
		int err = connect(sock, (struct sockaddr *) &dest_addr, sizeof(dest_addr));
		if (err != 0) {
			ESP_LOGE(TAG, "unable to connect %d", errno);
		}
		f_buf_len = 0;
		recv(sock, (char *) &f_buf_len, 2, 0);
		f_buf = malloc(f_buf_len);
		size = 0;
		while (size < f_buf_len) {
			size += recv(sock, f_buf + size, f_buf_len - size, 0);
		}
		shutdown(sock, 0);
		close(sock);

		fp = fopen(MOUNT_POINT "/" YASIO_SETUP_FNAME, "wb");
		fwrite(f_buf, 1, f_buf_len, fp);
		fclose(fp);

		drives[0].buffer = (uint8_t *) f_buf;
		strcpy(drives[0].path, YASIO_SETUP_FNAME);
		ESP_LOGI(TAG, "loading xex '%s' %d %p", drives[0].path, f_buf_len, f_buf);
		load_xex(f_buf_len);

		http_redirect(req, 0);
		return ESP_OK;
	} else {
		http_redirect(req, 0);
	}

	return ESP_OK;
}

#define TOKEN_NONE			0
#define TOKEN_CRLF			1
#define TOKEN_CONTENT_DISPOSITION	2
#define TOKEN_CONTENT_TYPE		3
#define	TOKEN_FORM_DATA			4
#define TOKEN_COLON			5
#define TOKEN_SEMICOLON			6
#define TOKEN_EQU			7
#define TOKEN_NAME			8
#define TOKEN_FILENAME			9
#define TOKEN_STRING			10
#define TOKEN_MIME_TYPE			11
#define TOKEN_BOUNDARY			12
#define TOKEN_DOUBLE_MINUS		13

static int scan_token(char *buffer, int *offset, int content_length, char *boundary, char **start, int *length)
{
	if (content_length - *offset >= strlen(boundary) && !strncmp(buffer + *offset, boundary, strlen(boundary))) {
		*offset += strlen(boundary);
		return TOKEN_BOUNDARY;
	}
	if (content_length - *offset >= 2 && buffer[*offset] == '\r' && buffer[*offset + 1] == '\n') {
		*offset += 2;
		return TOKEN_CRLF;
	}
	while (buffer[*offset] == ' ') {
		*offset += 1;
	}
	if (content_length - *offset >= strlen("Content-Disposition") && !strncmp(buffer + *offset, "Content-Disposition", strlen("Content-Disposition"))) {
		*offset += strlen("Content-Disposition");
		return TOKEN_CONTENT_DISPOSITION;
	}
	if (content_length - *offset >= strlen("Content-Type") && !strncmp(buffer + *offset, "Content-Type", strlen("Content-Type"))) {
		*offset += strlen("Content-Type");
		return TOKEN_CONTENT_TYPE;
	}
	if (content_length - *offset >= strlen("form-data") && !strncmp(buffer + *offset, "form-data", strlen("form-data"))) {
		*offset += strlen("form-data");
		return TOKEN_FORM_DATA;
	}
	if (content_length - *offset >= strlen("name") && !strncmp(buffer + *offset, "name", strlen("name"))) {
		*offset += strlen("name");
		return TOKEN_NAME;
	}
	if (content_length - *offset >= strlen("filename") && !strncmp(buffer + *offset, "filename", strlen("filename"))) {
		*offset += strlen("filename");
		return TOKEN_FILENAME;
	}
	if (content_length - *offset >= 2 && buffer[*offset] == '-' && buffer[*offset + 1] == '-') {
		*offset += 2;
		return TOKEN_DOUBLE_MINUS;
	}
	if (content_length - *offset >= 1 && buffer[*offset] == ':') {
		*offset += 1;
		return TOKEN_COLON;
	}
	if (content_length - *offset >= 1 && buffer[*offset] == ';') {
		*offset += 1;
		return TOKEN_SEMICOLON;
	}
	if (content_length - *offset >= 1 && buffer[*offset] == '=') {
		*offset += 1;
		return TOKEN_EQU;
	}
	if (content_length - *offset >= 2 && buffer[*offset] == '"') {
		if (start != NULL) {
			*start = buffer + *offset + 1;
		}
		while (buffer[++*offset] != '"');
		if (length != NULL) {
			*length = buffer + *offset - *start;
		}
		*offset += 1;
		return TOKEN_STRING;
	}
	if (content_length - *offset >= 1 && buffer[*offset] >= 'a' && buffer[*offset] <= 'z') {
		if (start != NULL) {
			*start = buffer + *offset;
		}
		while ((buffer[*offset] >= 'a' && buffer[*offset] <= 'z') || buffer[*offset] == '/' || buffer[*offset] == '-') {
			*offset += 1;
		}
		if (length != NULL) {
			*length = buffer + *offset - *start;
		}
		return TOKEN_MIME_TYPE;
	}

	return TOKEN_NONE;
}

static int expect_token(int token, char *buffer, int *offset, int content_length, char *boundary, char **start, int *length)
{
	int save = *offset;
	if (token == scan_token(buffer, offset, content_length, boundary, start, length)) {
		return 1;
	} else {
		*offset = save;
		return 0;
	}
}

static int parse_content_disposition(char *buffer, int *offset, int content_length, char *boundary, char **filename, int *filename_len)
{
	return expect_token(TOKEN_COLON, buffer, offset, content_length, boundary, NULL, NULL)
		&& expect_token(TOKEN_FORM_DATA, buffer, offset, content_length, boundary, NULL, NULL)
		&& expect_token(TOKEN_SEMICOLON, buffer, offset, content_length, boundary, NULL, NULL)
		&& expect_token(TOKEN_NAME, buffer, offset, content_length, boundary, NULL, NULL)
		&& expect_token(TOKEN_EQU, buffer, offset, content_length, boundary, NULL, NULL)
		&& expect_token(TOKEN_STRING, buffer, offset, content_length, boundary, NULL, NULL)
		&& expect_token(TOKEN_SEMICOLON, buffer, offset, content_length, boundary, NULL, NULL)
		&& expect_token(TOKEN_FILENAME, buffer, offset, content_length, boundary, NULL, NULL)
		&& expect_token(TOKEN_EQU, buffer, offset, content_length, boundary, NULL, NULL)
		&& expect_token(TOKEN_STRING, buffer, offset, content_length, boundary, filename, filename_len)
		&& expect_token(TOKEN_CRLF, buffer, offset, content_length, boundary, NULL, NULL);
}

static int parse_content_type(char *buffer, int *offset, int content_length, char *boundary)
{
	return expect_token(TOKEN_COLON, buffer, offset, content_length, boundary, NULL, NULL)
		&& expect_token(TOKEN_MIME_TYPE, buffer, offset, content_length, boundary, NULL, NULL)
		&& expect_token(TOKEN_CRLF, buffer, offset, content_length, boundary, NULL, NULL);
}

static int parse_multipart_header(char *buffer, int *offset, int content_length, char *boundary, char **filename, int *filename_len)
{
	int t;

	for (;;) {
		t = scan_token(buffer, offset, content_length, boundary, filename, filename_len);
		if (t == TOKEN_CRLF) {
			return 1;
		} else if (t == TOKEN_CONTENT_DISPOSITION) {
			if (!parse_content_disposition(buffer, offset, content_length, boundary, filename, filename_len)) {
				ESP_LOGE(TAG, "failed to parse content disposition");
				return 0;
			}
		} else if (t == TOKEN_CONTENT_TYPE) {
			if (!parse_content_type(buffer, offset, content_length, boundary)) {
				ESP_LOGE(TAG, "failed to parse content type");
				return 0;
			}
		} else {
			return 0;
		}
	}
}

static esp_err_t handle_post_multipart(httpd_req_t *req, int content_length, char *boundary)
{
	char *buffer;
	char path[540];
	FILE *fp;
	int size = 0;
	int ret;
	int offset;
	char *filename;
	int filename_len;
	char *data_start;
	int data_len;
	int i;

	if ((buffer = malloc(content_length)) == NULL) {
		sprintf(path, "Memory allocation failure (%d bytes)", content_length);	// hack: reuse `path' to make error message
		http_show_error(req, path);
		return ESP_FAIL;
	}

	size = 0;
	while (size < content_length) {
		if ((ret = httpd_req_recv(req, buffer + size, content_length - size)) <= 0) {
			if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
				continue;
			}
			free(buffer);
			http_show_error(req, "Multipart post: data retrieval failure");
			return ESP_FAIL;
		}
		size += ret;
	}
	/*
	ESP_LOGI(TAG, "========= received data:");
	ESP_LOGI(TAG, "%.*s", size, buffer);
	ESP_LOGI(TAG, "=========");
	*/

	offset = 0;
	if (!expect_token(TOKEN_BOUNDARY, buffer, &offset, content_length, boundary, NULL, NULL)) {
		ESP_LOGE(TAG, "expected `boundary' at %d", offset);
		free(buffer);
		return ESP_FAIL;
	}
	if (!expect_token(TOKEN_CRLF, buffer, &offset, content_length, boundary, NULL, NULL)) {
		ESP_LOGE(TAG, "expected CRLF at %d", offset);
		free(buffer);
		return ESP_FAIL;
	}
	if (!parse_multipart_header(buffer, &offset, content_length, boundary, &filename, &filename_len)) {
		ESP_LOGE(TAG, "multipart header parsing error at %d [%20s]", offset, buffer + offset);
		free(buffer);
		return ESP_FAIL;
	}
	data_start = buffer + offset;
	data_len = content_length - (offset + 6 + strlen(boundary));
	offset += data_len;
	if (!expect_token(TOKEN_CRLF, buffer, &offset, content_length, boundary, NULL, NULL)) {
		ESP_LOGE(TAG, "expected CRLF at %d", offset);
		free(buffer);
		return ESP_FAIL;
	}
	if (!expect_token(TOKEN_BOUNDARY, buffer, &offset, content_length, boundary, NULL, NULL)) {
		ESP_LOGE(TAG, "expected `boundary' at %d", offset);
		free(buffer);
		return ESP_FAIL;
	}
	if (!expect_token(TOKEN_DOUBLE_MINUS, buffer, &offset, content_length, boundary, NULL, NULL)) {
		ESP_LOGE(TAG, "expected `--' at %d", offset);
		free(buffer);
		return ESP_FAIL;
	}
	if (!expect_token(TOKEN_CRLF, buffer, &offset, content_length, boundary, NULL, NULL)) {
		ESP_LOGE(TAG, "expected CRLF at %d", offset);
		free(buffer);
		return ESP_FAIL;
	}
	filename[filename_len] = '\0';
	ESP_LOGI(TAG, "multipart scan successful; length: %d; filename: '%s'", data_len, filename);

	build_effective_path(req, filename, path);

	ESP_LOGI(TAG, "upload %s '%s'", req->uri, filename);
	for (i = 0; i < 4; i++) {
		if (drives[i].path[0] && !strcmp(drives[i].path, path + sizeof(MOUNT_POINT))) {
			break;
		}
	}
	if (i < 4) {
		sprintf(path, "file '%s' inserted in drive D%d, cannot upload", filename, i + 1);	// hack: reuse array to create error message
		ESP_LOGI(TAG, "%s", path);
		http_show_error(req, path);
	} else {
		fp = fopen(path, "wb");
		if (fp != NULL) {
			fwrite(data_start, 1, data_len, fp);
			fclose(fp);
			httpd_resp_set_status(req, "301");
			httpd_resp_set_hdr(req, "Location", req->uri);
			httpd_resp_send_chunk(req, NULL, 0);
		} else {
			httpd_resp_set_status(req, "500");
			httpd_resp_send_chunk(req, NULL, 0);
		}
	}
	free(buffer);

	return ESP_OK;
}

static esp_err_t http_post_handler(httpd_req_t *req)
{
	char buffer[512];
	char buffer2[64];
	char *boundary;
	int len;
	int n;

	httpd_req_get_hdr_value_str(req, "Content-Type", buffer, sizeof(buffer));
	if (!strcmp(buffer, "application/x-www-form-urlencoded")) {
		return handle_post_urlencoded(req);
	} else if (!strncmp(buffer, "multipart/form-data;", 20)) {
		httpd_req_get_hdr_value_str(req, "Content-Length", buffer2, sizeof(buffer2));
		len = atoi(buffer2);
		boundary = strstr(buffer, "boundary=") + sizeof("boundary=") - 1;
		for (n = 0; boundary[n] != '\r'; n++);
		boundary[n] = '\0';
		//ESP_LOGI(TAG, "content-length: %d; boundary: '%s'", len, boundary);
		boundary -= 2;
		boundary[0] = '-';
		boundary[1] = '-';
		return handle_post_multipart(req, len, boundary);
	} else {
		return ESP_FAIL;
	}
}

static const httpd_uri_t root_post_uri = {
	.uri       = "/*",
	.method    = HTTP_POST,
	.handler   = http_post_handler,
	.user_ctx  = NULL
};

static httpd_handle_t start_webserver()
{
	httpd_handle_t server = NULL;
	httpd_config_t config = HTTPD_DEFAULT_CONFIG();
	config.stack_size = 16384;
	config.uri_match_fn = httpd_uri_match_wildcard;
	config.lru_purge_enable = true;

	ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
	if (httpd_start(&server, &config) == ESP_OK) {
		ESP_LOGI(TAG, "Registering URI handlers");
		httpd_register_uri_handler(server, &root_get_uri);
		httpd_register_uri_handler(server, &root_post_uri);
		return server;
	}

	ESP_LOGI(TAG, "Error starting server!");
	return NULL;
}

static esp_err_t stop_webserver(httpd_handle_t server)
{
	ESP_LOGI(TAG, "Stopping webserver");
	return httpd_stop(server);
}

int close_gate(int ticks, const char *where)
{
	ESP_LOGI(TAG, "closing gate (%d) - %s", ticks, where);

	if (semaphore_take(connection_gate, ticks) == TRUE) {
		ESP_LOGI(TAG, "gate closed successfully - %s", where);
		return 1;
	} else {
		ESP_LOGI(TAG, "gate closing failed - %s", where);
		return 0;
	}
}

void open_gate(const char *where)
{
	ESP_LOGI(TAG, "opening gate - %s", where);
	semaphore_give(connection_gate);
}

static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
		ESP_LOGI(TAG, "event - wifi start");
	} else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_SCAN_DONE) {
		ESP_LOGI(TAG, "event - wifi scan done");
		event_group_set_bits(s_wifi_event_group, WIFI_SCAN_DONE_BIT);
	} else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_STOP) {
		ESP_LOGI(TAG, "event - wifi sta stop");
		event_group_set_bits(s_wifi_event_group, WIFI_STA_STOP_BIT);
	} else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
		ESP_LOGI(TAG, "event - wifi disconnected");
		if (server != NULL) {
			ESP_ERROR_CHECK(stop_webserver(server));
			server = NULL;
			memset(ip_address, 0, sizeof(ip_address));
		}
		if (close_gate(0, "disconnect")) {
			if (s_retry_num < ESP_MAXIMUM_RETRY) {
				ESP_LOGI(TAG, "Retry to connect to the AP");
				esp_wifi_connect();
				s_retry_num++;
			} else {
				event_group_set_bits(s_wifi_event_group, WIFI_FAIL_BIT);
			}
			ESP_LOGI(TAG,"Connect to the AP fail");
			open_gate("disconnect");
		} else {
			ESP_LOGI(TAG, "gate blocked");
		}
		event_group_set_bits(s_wifi_event_group, WIFI_DISCONNECTED_BIT);
	} else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
		ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
		ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
		memset(ip_address, 0, sizeof(ip_address));
		sprintf(ip_address, IPSTR, IP2STR(&event->ip_info.ip));
		if (!strncmp(ip_address, "192.168.0", 9)) {
			strcpy(setup_server_ip, "192.168.0.22");
		} else if (!strncmp(ip_address, "192.168.1", 9)) {
			strcpy(setup_server_ip, "192.168.1.26");
		}
		s_retry_num = 0;
		event_group_set_bits(s_wifi_event_group, WIFI_CONNECTED_BIT);
		if (server == NULL) {
			ESP_LOGI(TAG, "Starting webserver");
			server = start_webserver();
		}
	} else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_START) {
		if (server == NULL) {
			ESP_LOGI(TAG, "Starting webserver");
			server = start_webserver();
		}
	}
}

esp_err_t wifi_connect_to_ap(char *ssid, char *password)
{
	ESP_LOGI(TAG, "wifi_connect_to_ap(%s,%s)", ssid, password);

	s_retry_num = 0;

	wifi_config_t wifi_config = {
		.sta = {
			.threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
			.sae_pwe_h2e = ESP_WIFI_SAE_MODE,
			.sae_h2e_identifier = EXAMPLE_H2E_IDENTIFIER,
		},
	};
	strcpy((char *) wifi_config.sta.ssid, ssid);
	strcpy((char *) wifi_config.sta.password, password);
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
	ESP_ERROR_CHECK(esp_wifi_connect());

	EventBits_t bits = event_group_wait_bits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, FALSE, FALSE, MAX_DELAY);

	if (bits & WIFI_CONNECTED_BIT) {
		ESP_LOGI(TAG, "Connected to SSID:%s", ssid);
		strcpy(ssid_connected, ssid);
		return ESP_OK;
	} else if (bits & WIFI_FAIL_BIT) {
		ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s", ssid, password);
		ssid_connected[0] = 0;
		return ESP_ERR_INVALID_STATE;
	} else {
		ESP_LOGE(TAG, "Unexpected event");
		return ESP_ERR_INVALID_STATE;
	}
}

static void wifi_scan()
{
	esp_netif_set_default_netif(sta_netif);

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	memset(ap_info, 0, sizeof(ap_info));

	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_start());

	ESP_LOGI(TAG, "wifi_scan_start");
	esp_wifi_scan_start(NULL, true);
	event_group_wait_bits(s_wifi_event_group, WIFI_SCAN_DONE_BIT, FALSE, FALSE, MAX_DELAY);

	ap_number = WIFI_SCAN_LIST_SIZE;
	ESP_LOGI(TAG, "Max AP number ap_info can hold = %u", ap_number);
	ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
	ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_number, ap_info));
	ESP_LOGI(TAG, "Total APs scanned = %u, actual AP number ap_info holds = %u", ap_count, ap_number);
	for (int i = 0; i < ap_number; i++) {
		ESP_LOGI(TAG, "SSID %s, RSSI %d", ap_info[i].ssid, ap_info[i].rssi);
	}
}

static void wifi_init_softap()
{
	unsigned int ip;
	unsigned int mask_size;
	unsigned int mask;
	char *p;
	int n;

	for (p = (char *) ap_ip, n = 0; *p != '.'; p++) {
		n = (n * 10) + (*p - '0');
	}
	ip = n << 24;
	for (p++, n = 0; *p != '.'; p++) {
		n = (n * 10) + (*p - '0');
	}
	ip |= n << 16;
	for (p++, n = 0; *p != '.'; p++) {
		n = (n * 10) + (*p - '0');
	}
	ip |= n << 8;
	for (p++, n = 0; *p != '/'; p++) {
		n = (n * 10) + (*p - '0');
	}
	ip |= n;
	for (p++, mask_size = 0; *p; p++) {
		mask_size = (mask_size * 10) + (*p - '0');
	}
	mask = ~((1 << (32 - mask_size)) - 1);

	ESP_ERROR_CHECK(esp_netif_dhcps_stop(ap_netif));
	esp_netif_ip_info_t ip_info;
	ESP_ERROR_CHECK(esp_netif_get_ip_info(ap_netif, &ip_info));
	IP4_ADDR(&ip_info.ip, ip >> 24, ip >> 16, ip >> 8, ip);
	IP4_ADDR(&ip_info.netmask, mask >> 24, mask >> 16, mask >> 8, mask);
	IP4_ADDR(&ip_info.gw, ip >> 24, ip >> 16, ip >> 8, ip);
	ESP_ERROR_CHECK(esp_netif_set_ip_info(ap_netif, &ip_info));
	dhcps_lease_t dhcps_poll = { .enable = true };
	IP4_ADDR(&dhcps_poll.start_ip, (ip + 1) >> 24, (ip + 1) >> 16, (ip + 1) >> 8, ip + 1);
	IP4_ADDR(&dhcps_poll.end_ip, (ip + 2) >> 24, (ip + 2) >> 16, (ip + 2) >> 8, ip + 2);
	ESP_ERROR_CHECK(esp_netif_dhcps_option(ap_netif, ESP_NETIF_OP_SET, REQUESTED_IP_ADDRESS, &dhcps_poll, sizeof(dhcps_poll)));
	ESP_ERROR_CHECK(esp_netif_dhcps_start(ap_netif));

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));

	wifi_config_t wifi_ap_config = {
		.ap = {
			.channel = AP_CHANNEL,
			.max_connection = 1,
			.authmode = WIFI_AUTH_WPA2_PSK,
			.pmf_cfg = {
				.required = false,
			},
		},
	};
	strcpy((char *) wifi_ap_config.ap.ssid, ap_ssid);
	wifi_ap_config.ap.ssid_len = strlen(ap_ssid);
	strcpy((char *) wifi_ap_config.ap.password, ap_password);

	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_ap_config));
	ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d", ap_ssid, ap_password, AP_CHANNEL);
	ESP_ERROR_CHECK(esp_wifi_start());

	esp_netif_set_default_netif(ap_netif);

	sprintf(setup_server_ip, IPSTR, IP2STR(&dhcps_poll.start_ip));
}

static void wifi_switch(wifi_mode_t new_wifi_mode, char *ssid, char *pass)
{
	wifi_mode_t current_wifi_mode;
	int i;
	int j;
	int done = 0;

	set_rgb(0, 0, 0);

	esp_wifi_get_mode(&current_wifi_mode);
	if (new_wifi_mode == WIFI_MODE_STA) {
		if (current_wifi_mode == WIFI_MODE_AP) {
			ESP_LOGI(TAG, "wifi switch: ap -> sta");
			ESP_ERROR_CHECK(esp_wifi_stop());
			ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));
		} else if (current_wifi_mode == WIFI_MODE_STA) {
			ESP_LOGI(TAG, "wifi switch: sta -> sta");
			if (!close_gate(10, "switch")) {
				ESP_LOGE(TAG, "cannot close gate");
			}
			ESP_ERROR_CHECK(esp_wifi_disconnect());
			event_group_wait_bits(s_wifi_event_group, WIFI_DISCONNECTED_BIT, FALSE, FALSE, MAX_DELAY);
			ESP_LOGI(TAG, "esp_wifi_stop()");
			ESP_ERROR_CHECK(esp_wifi_stop());
			EventBits_t bits = event_group_wait_bits(s_wifi_event_group, WIFI_STA_STOP_BIT, FALSE, FALSE, MAX_DELAY);
			ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));
			ESP_LOGI(TAG, "releasing gate; 0x%x", (int) bits);
			open_gate("switch");
		}
		if (ssid == NULL) {
			wifi_scan();
			for (i = 0; i < ap_count && ap_info[i].ssid[0]; i++) {
				for (j = 0; j < CREDENTIALS_LIST_SIZE && credentials[j].ssid[0]; j++) {
					if (!strcmp((char *) ap_info[i].ssid, credentials[j].ssid)) {
						if (wifi_connect_to_ap(credentials[j].ssid, credentials[j].pass) == ESP_OK) {
							set_rgb(0, 16, 0);
						} else {
							set_rgb(0, 16, 16);
						}
						done = 1;
						break;
					}
				}
			}
			if (!done) {
				set_rgb(16, 0, 16);
			}
		}
	} else if (new_wifi_mode == WIFI_MODE_AP) {
		if (current_wifi_mode == WIFI_MODE_AP) {
			ESP_LOGI(TAG, "wifi switch: ap -> ap");
			ESP_ERROR_CHECK(esp_wifi_stop());
			ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));
		} else if (current_wifi_mode == WIFI_MODE_STA) {
			ESP_LOGI(TAG, "wifi switch: sta -> ap");
			if (!close_gate(10, "switch")) {
				ESP_LOGE(TAG, "cannot close gate");
			}
			ESP_ERROR_CHECK(esp_wifi_disconnect());
			event_group_wait_bits(s_wifi_event_group, WIFI_DISCONNECTED_BIT, FALSE, FALSE, MAX_DELAY);
			ESP_ERROR_CHECK(esp_wifi_stop());
			event_group_wait_bits(s_wifi_event_group, WIFI_STA_STOP_BIT, FALSE, FALSE, MAX_DELAY);
			ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));
			ESP_LOGI(TAG, "releasing gate");
			open_gate("switch");
		}
		wifi_init_softap();
		set_rgb(0, 16, 0);
	}
}

void info()
{
	esp_chip_info_t chip_info;
	uint32_t flash_size;

	esp_chip_info(&chip_info);
	printf("CHIP: %s, %d CPU core(s), WiFi%s%s, ",
			CONFIG_IDF_TARGET,
			chip_info.cores,
			(chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
			(chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

	unsigned major_rev = chip_info.revision / 100;
	unsigned minor_rev = chip_info.revision % 100;
	printf("Silicon revision v%d.%d, ", major_rev, minor_rev);
	if (esp_flash_get_size(NULL, &flash_size) != ESP_OK) {
		printf("Get flash size failed");
		return;
	}

	printf("%luMB %s flash\n", flash_size / (1024 * 1024),
			(chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

	printf("Minimum free heap size: %ld bytes\n", esp_get_minimum_free_heap_size());
}

esp_err_t init_sdcard()
{
	esp_err_t ret;
	esp_vfs_fat_sdmmc_mount_config_t mount_config = {
		.format_if_mount_failed = false,
		.max_files = 5,
		.allocation_unit_size = 16 * 1024
	};
	sdmmc_card_t *card;
	const char mount_point[] = MOUNT_POINT;

	sdmmc_host_t host = SDMMC_HOST_DEFAULT();
	sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
	slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;
	slot_config.width = 4;
	slot_config.cmd = CMD_PIN;
	slot_config.clk = CLK_PIN;
	slot_config.d0 = D0_PIN;
	slot_config.d1 = D1_PIN;
	slot_config.d2 = D2_PIN;
	slot_config.d3 = D3_PIN;

	ESP_LOGI(TAG, "Mounting filesystem");
	ret = esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config, &mount_config, &card);
	if (ret != ESP_OK) {
		if (ret == ESP_FAIL) {
			ESP_LOGE(TAG, "Failed to mount filesystem");
		} else {
			ESP_LOGE(TAG, "Failed to initialize SD card (%s)", esp_err_to_name(ret));
		}
		return ret;
	}
	ESP_LOGI(TAG, "Filesystem mounted");
	sdmmc_card_print_info(stdout, card);

	return ESP_OK;
}

#define PIN_NUM_MOSI	34
#define PIN_NUM_MISO	39
#define PIN_NUM_CLK	38
#define PIN_NUM_CS	33

esp_err_t init_sdcard2()
{
	esp_err_t ret;

	esp_vfs_fat_sdmmc_mount_config_t mount_config = {
		.format_if_mount_failed = true,
		.max_files = 5,
		.allocation_unit_size = 16 * 1024
	};
	sdmmc_card_t *card;
	const char mount_point[] = MOUNT_POINT;
	ESP_LOGI(TAG, "Initializing SD card");
	ESP_LOGI(TAG, "Using SPI peripheral");
	sdmmc_host_t host = SDSPI_HOST_DEFAULT();

	spi_bus_config_t bus_cfg = {
		.mosi_io_num = PIN_NUM_MOSI,
		.miso_io_num = PIN_NUM_MISO,
		.sclk_io_num = PIN_NUM_CLK,
		.quadwp_io_num = -1,
		.quadhd_io_num = -1,
		.max_transfer_sz = 4000,
	};

	ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Failed to initialize bus.");
		return ret;
	}

	sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
	slot_config.gpio_cs = PIN_NUM_CS;
	slot_config.host_id = host.slot;

	ESP_LOGI(TAG, "Mounting filesystem");
	ret = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &card);

	if (ret != ESP_OK) {
		if (ret == ESP_FAIL) {
			ESP_LOGE(TAG, "Failed to mount filesystem");
		} else {
			ESP_LOGE(TAG, "Failed to initialize the card (%s)", esp_err_to_name(ret));
		}
		return ret;
	}
	ESP_LOGI(TAG, "Filesystem mounted");

	sdmmc_card_print_info(stdout, card);

	return ESP_OK;
}

static void IRAM_ATTR gpio_isr_handler(void *arg)
{
	BaseType_t higher_priority_task_woken = FALSE;
	uint32_t value = REG_READ(GPIO_IN_REG) & (1 << SIO_CMD_PIN);
	if (value) {
		task_notify_give_indexed_from_isr(sio_task_handle, NOTIFICATION_CMD_HI, &higher_priority_task_woken);
		yield_from_isr(higher_priority_task_woken);
	} else {
		task_notify_give_indexed_from_isr(sio_task_handle, NOTIFICATION_CMD_LO, &higher_priority_task_woken);
		yield_from_isr(higher_priority_task_woken);
	}
}

static void IRAM_ATTR setup_enable_isr_handler(void *arg)
{
	BaseType_t higher_priority_task_woken = FALSE;
	task_notify_give_indexed_from_isr(sio_task_handle, NOTIFICATION_ENABLE_SETUP, &higher_priority_task_woken);
	yield_from_isr(higher_priority_task_woken);
}

static uint8_t sio_cksum(uint8_t *data, int offset, int count)
{
	uint8_t cksum = 0;
	uint32_t tmp;
	int i;

	for (i = 0; i < count; i++) {
		tmp = cksum + data[offset + i];
		cksum = (tmp > 0xff) ? ((tmp & 0xff) + 1) : (tmp & 0xff);
	}
	return cksum;
}

static void send_sio_error()
{
	uint8_t b = 'E';
	uart_write_bytes(SIO_UART_NUM, &b, 1);
}

static void send_sio_ack()
{
	uint8_t b = 'A';
	uart_write_bytes(SIO_UART_NUM, &b, 1);
}

static void send_sio_nack()
{
	uint8_t b = 'N';
	uart_write_bytes(SIO_UART_NUM, &b, 1);
}

static void send_sio_compl()
{
	uint8_t b = 'C';
	uart_write_bytes(SIO_UART_NUM, &b, 1);
}

static void timeout_timer_callback(void *arg)
{
	BaseType_t higher_priority_task_woken = FALSE;
	task_notify_give_indexed_from_isr(sio_task_handle, NOTIFICATION_TIMEOUT, &higher_priority_task_woken);
	yield_from_isr(higher_priority_task_woken);
}

static esp_err_t config_update_station_entry(int n)
{
	char *ssid = credentials[n].ssid;
	char *password = credentials[n].pass;
	FILE *fp;
	char *data;
	char *p;
	int size = 8192;
	int found = 0;
	int i;
	int j;

	data = malloc(size);
	if (data == NULL) {
		return ESP_FAIL;
	}
	p = data;

	fp = fopen(MOUNT_POINT "/" YASIO_CONFIG_FNAME, "rt");
	if (fp == NULL) {
		free(data);
		return ESP_FAIL;
	}
	size = fread(data, 1, size, fp);
	fclose(fp);

	fp = fopen(MOUNT_POINT "/" YASIO_CONFIG_FNAME, "wt");
	i = 0;
	while (i < size) {
		p = data + i;
		if (!strncmp(p, "D1:", 3) || !strncmp(p, "D2:", 3)
				|| !strncmp(p, "D3:", 3) || !strncmp(p, "D4:", 3)
				|| !strncmp(p, "AP:", 3)) {
			j = i;
			while (data[i++] != '\n');
			fwrite(p, 1, i - j, fp);
		} else if (!strncmp(p, "MO:", 3)) {
			while (data[i++] != '\n');
			fwrite("MO:Station\n", 1, 11, fp);
		} else if (!strncmp(p, "ST:", 3)) {
			if (!strncmp(p + 3, ssid, strlen(ssid))) {
				found = 1;
				while (data[i++] != '\n');
				fwrite("ST:", 1, 3, fp);
				fwrite(ssid, 1, strlen(ssid), fp);
				fputc(' ', fp);
				fwrite(password, 1, strlen(password), fp);
				fputc('\n', fp);
			} else {
				j = i;
				while (data[i++] != '\n');
				fwrite(p, 1, i - j, fp);
			}
		}
	}
	if (!found) {
		fwrite("ST:", 1, 3, fp);
		fwrite(ssid, 1, strlen(ssid), fp);
		fputc(' ', fp);
		fwrite(password, 1, strlen(password), fp);
		fputc('\n', fp);
	}
	fclose(fp);

	free(data);

	return ESP_OK;
}

static esp_err_t config_update_ap_entry()
{
	FILE *fp;
	char *data;
	char *p;
	int size = 8192;
	int i;
	int j;

	data = malloc(size);
	if (data == NULL) {
		return ESP_FAIL;
	}
	p = data;

	fp = fopen(MOUNT_POINT "/" YASIO_CONFIG_FNAME, "rt");
	if (fp == NULL) {
		free(data);
		return ESP_FAIL;
	}
	size = fread(data, 1, size, fp);
	fclose(fp);

	fp = fopen(MOUNT_POINT "/" YASIO_CONFIG_FNAME, "wt");
	i = 0;
	while (i < size) {
		p = data + i;
		if (!strncmp(p, "D1:", 3) || !strncmp(p, "D2:", 3)
				|| !strncmp(p, "D3:", 3) || !strncmp(p, "D4:", 3)
				|| !strncmp(p, "ST:", 3)) {
			j = i;
			while (data[i++] != '\n');
			fwrite(p, 1, i - j, fp);
		} else if (!strncmp(p, "MO:", 3)) {
			while (data[i++] != '\n');
			fwrite("MO:AccessPoint\n", 1, 15, fp);
		} else if (!strncmp(p, "AP:", 3)) {
			while (data[i++] != '\n');
			fwrite("AP:", 1, 3, fp);
			fwrite(ap_ssid, 1, strlen(ap_ssid), fp);
			fputc(' ', fp);
			fwrite(ap_password, 1, strlen(ap_password), fp);
			fputc(' ', fp);
			fwrite(ap_ip, 1, strlen(ap_ip), fp);
			fputc('\n', fp);
		}
	}
	fclose(fp);

	free(data);

	return ESP_OK;

}

static void reconnect(int i)
{
	if (ssid_connected[0] != 0) {
		ssid_connected[0] = 0;
		set_rgb(0, 0, 0);
		ESP_ERROR_CHECK(esp_wifi_disconnect());
	}
	if (wifi_connect_to_ap(credentials[i].ssid, credentials[i].pass) == ESP_OK) {
		set_rgb(0, 16, 0);
		strcpy(ssid_connected, credentials[i].ssid);
	} else {
		ssid_connected[0] = 0;
	}
}

static void delay_250us()
{
	uart_wait_tx_done(SIO_UART_NUM, MS_TO_TICKS(10));
	ESP_ERROR_CHECK(esp_timer_start_once(timeout_timer, 250));
	gpio_set_level(DEBUG1_PIN, 1);
	while (task_notify_take_indexed(NOTIFICATION_TIMEOUT, TRUE, 0) == 0);
	esp_timer_stop(timeout_timer);
	gpio_set_level(DEBUG1_PIN, 0);
}

static int fetch_data(uint8_t *data, int size)
{
	int i;
	int timeout;

	ESP_ERROR_CHECK(esp_timer_start_once(timeout_timer, 350000));
	i = 0;
	timeout = 0;
	while (i < size + 1) {
		i += uart_read_bytes(SIO_UART_NUM, data + i, size + 1 - i, MS_TO_TICKS(1));
		if (task_notify_take_indexed(NOTIFICATION_TIMEOUT, TRUE, 0)) {
			timeout = 1;
			break;
		}
	}
	esp_timer_stop(timeout_timer);

	if (!timeout) {
		ESP_ERROR_CHECK(esp_timer_start_once(timeout_timer, 850));
		while (!task_notify_take_indexed(NOTIFICATION_TIMEOUT, TRUE, 0));
	}

	return timeout;
}

static void handle_sio_command(uint8_t ddevic, uint8_t dcmnd, uint8_t daux1, uint8_t daux2)
{
	uint8_t data[1025];
	uint32_t sector_number;
	uint8_t *p;
	uint32_t size;
	uint32_t offset;
	FILE *fp;
	int drive;
	int error;
	int i;
	int timeout;
	wifi_mode_t wifi_mode;

	switch (dcmnd) {
		case DCMND_FORMAT_DISK:
		case DCMND_FORMAT_MEDIUM:			// formally, the sector size for this command should be set to 128
			if (ddevic >= 0x31 && ddevic <= 0x34 && drives[ddevic-0x31].path[0] != 0) {
				send_sio_ack();
				drive = ddevic - 0x31;
				ESP_LOGI(TAG, "formatting drive D%d (%d)", drive + 1, (int) drives[drive].size - 16);
				memset(drives[drive].buffer + 16, 0, drives[drive].size - 16);
				sprintf((char *) data, "%s/%s", MOUNT_POINT, drives[drive].path);
				fp = fopen((char *) data, "wb");
				if (fp != NULL) {
					ESP_LOGI(TAG, "opened file for formatting '%s'", drives[drive].path);
					if (fwrite(drives[drive].buffer, 1, drives[drive].size, fp) != drives[drive].size) {
						ESP_LOGE(TAG, "failed to update '%s'", drives[drive].path);
						send_sio_error();
					} else {
						fclose(fp);
						size = drives[drive].sector_size;
						ESP_LOGI(TAG, "updated file '%s'", drives[drive].path);
						memset(data + 1, 0, size);
						delay_250us();
						data[0] = 'C';
						data[1] = 0xff;
						data[2] = 0xff;
						data[size+1] = sio_cksum(data, 1, size);
						uart_write_bytes(SIO_UART_NUM, data, size + 2);
					}
				} else {
					ESP_LOGE(TAG, "failed to open '%s' for writing", drives[drive].path);
					send_sio_error();
				}
			} else {
				send_sio_error();
			}
			break;

		case DCNMD_SEND_HIGH_SPEED_INDEX:
			send_sio_ack();
			delay_250us();
			data[0] = 'C';
			data[1] = high_speed_index;
			data[2] = sio_cksum(data, 1, 1);
			uart_write_bytes(SIO_UART_NUM, data, 3);
			uart_wait_tx_done(SIO_UART_NUM, MS_TO_TICKS(50));
			uart_set_baudrate(SIO_UART_NUM, (F_OSC/2) / (high_speed_index + 7));
			break;

		case DCNMD_HAPPY_CONFIG:
			send_sio_ack();
			delay_250us();
			data[0] = 'C';
			data[1] = 0;
			uart_write_bytes(SIO_UART_NUM, data, 2);
			break;

		case DCMND_READ_PERCOM:
			if (ddevic >= 0x31 && ddevic <= 0x34 && drives[ddevic-0x31].path[0] != 0) {
				send_sio_ack();
				delay_250us();
				drive = ddevic - 0x31;
				data[0] = 'C';
				data[1]  = drives[drive].total_tracks;				// [0] # of tracks
				data[2]  = 3;							// [1] head movement speed (6 ms)
				data[3]  = (drives[drive].sectors_per_track >> 8) & 0xff;	// [2] # sectors/track high byte
				data[4]  = drives[drive].sectors_per_track & 0xff;		// [3] # sectors/track low byte
				data[5]  = 0;							// [4] # active sides - 1
				data[6]  = 0x00;						// [5] bit1=1: 8" (=0: 5.25"), bit2=1: MFM (=0: FM), bit3=1: sideless disk
				data[7]  = (drives[drive].sector_size >> 8) & 0xff;		// [6] sector size high byte
				data[8]  = drives[drive].sector_size & 0xff;			// [7] sector size low byte
				data[9]  = 0xff;						// [8] unused, $FF
				data[10] = 0x00;						// [9] unused, $00
				data[11] = 0x00;						// [10] unused, $00
				data[12] = 0x00;						// [11] unused, $00
				data[13] = sio_cksum(data, 1, 11);
				uart_write_bytes(SIO_UART_NUM, data, 14);
			} else {
				send_sio_error();
			}
			break;

		case DCMND_PUT_SECTOR:
		case DCMND_WRITE_SECTOR:
			if (ddevic >= 0x31 && ddevic <= 0x34 && drives[ddevic-0x31].path[0] != 0) {
				send_sio_ack();
				drive = ddevic - 0x31;
				sector_number = (daux2 << 8) | daux1;
				size = drives[drive].sector_size;
				timeout = fetch_data(data, size);
				if (timeout) {
					ESP_LOGI(TAG, "put/write sector timeout (expected %d bytes)", (int) size + 1);
				} else if (sio_cksum(data, 0, size) != data[size]) {
					ESP_LOGE(TAG, "checksum mismatch, sector %d, drive D%d", (int) sector_number, drive + 1);
					send_sio_nack();
				} else {
					error = atr_sector_coordinates(drive, sector_number, &p, &size, &offset);
					if (error) {
						ESP_LOGE(TAG, "invalid sector %s %d, drive $%02x", drives[drive].path, (int) sector_number, (int) ddevic);
						send_sio_nack();
					} else if (drives[drive].write_protected) {
						ESP_LOGI(TAG, "disk %d is write protected", drive + 1);
						send_sio_error();
					} else {
						ESP_LOGI(TAG, "updating sector %d (len %d, offset $%x), drive D%d", (int) sector_number, (int) size, (int) offset, drive + 1);
						send_sio_ack();
						memcpy(p, data, size);
						sprintf((char *) data, "%s/%s", MOUNT_POINT, drives[drive].path);
						fp = fopen((char *) data, "wb");
						i = fseek(fp, offset, SEEK_SET);
						fwrite(p, 1, size, fp);
						fclose(fp);
						delay_250us();
						send_sio_compl();
					}
				}
			} else {
				ESP_LOGI(TAG, "no drive $%02x", (int) ddevic);
			}
			break;

		case DCMND_READ_STATUS:
			if (ddevic >= 0x31 && ddevic <= 0x34 && drives[ddevic-0x31].path[0] != 0) {
				send_sio_ack();
				delay_250us();
				drive = ddevic - 0x31;
				data[0] = 'C';
				data[1] = (drives[drive].sector_size == 256 ? 0x20 : 0x00) | (drives[drive].write_protected ? 0x08 : 0x00);
				data[2] = 0xff;
				data[3] = 0xf0;
				data[4] = 0x00;
				data[5] = sio_cksum(data, 1, 4);
				uart_write_bytes(SIO_UART_NUM, data, 6);
			} else {
				send_sio_error();
			}
			break;

		case DCMND_READ_SECTOR:
			if (ddevic >= 0x31 && ddevic <= 0x34 && drives[ddevic-0x31].path[0] != 0) {
				int error = 0;
				drive = ddevic - 0x31;
				sector_number = ((uint32_t) daux2 << 8) + (uint32_t) daux1;
				if (drives[drive].type == DRIVE_TYPE_ATR) {
					error = atr_sector_coordinates(drive, sector_number, &p, &size, NULL);
					if (error) {
						ESP_LOGE(TAG, "invalid ATR sector %d", (int) sector_number);
					}
				} else if (drives[drive].type == DRIVE_TYPE_XEX) {
					error = xex_sector_coordinates(sector_number, &p, &size);
					if (error) {
						ESP_LOGE(TAG, "invalid XEX sector %d", (int) sector_number);
					}
				} else {
					ESP_LOGE(TAG, "invalid drive type (%d)", drives[drive].type);
					error = 1;
				}
				if (!error) {
					send_sio_ack();
					delay_250us();
					data[0] = 'C';
					for (i = 0; i < size; i++) {
						data[1+i] = p[i];
					}
					data[1+size] = sio_cksum(data, 1, size);
					ESP_LOGI(TAG, "sending sector D%d %04x %d, cksum %02x", drive + 1, (int) sector_number, (int) size, data[1+size]);
					uart_write_bytes(SIO_UART_NUM, data, 2 + size);
				} else {
					send_sio_error();
				}
			} else {
				send_sio_error();
			}
			break;

		case DCMND_GET_IP_ADDRESS:
			send_sio_ack();
			delay_250us();
			data[0] = 'C';
			memset(data + 1, 0, 128);
			data[1] = strlen(ip_address);
			strncpy((char *) data + 2, ip_address, data[1]);
			data[129] = sio_cksum(data, 1, 128);
			uart_write_bytes(SIO_UART_NUM, data, 128 + 2);
			break;

		case DCMND_GET_WIFI_MODE:
			send_sio_ack();
			delay_250us();
			data[0] = 'C';
			memset(data + 1, 0, 128);
			esp_wifi_get_mode(&wifi_mode);
			data[1] = (wifi_mode == WIFI_MODE_STA) ? 0x00 : 0xff;
			data[129] = sio_cksum(data, 1, 128);
			uart_write_bytes(SIO_UART_NUM, data, 128 + 2);
			break;

		case DCMND_GET_NETWORK:
			send_sio_ack();
			delay_250us();
			data[0] = 'C';
			memset(data + 1, 0, 128);
			offset = ((uint32_t) daux2 << 8) + (uint32_t) daux1;
			if (offset < ap_count) {
				data[1] = strlen((char *) ap_info[offset].ssid);
				strncpy((char *) data + 2, (char *) ap_info[offset].ssid, data[1]);
				for (i = 0; i < CREDENTIALS_LIST_SIZE && credentials[i].ssid[0]; i++) {
					if (!strcmp((char *) ap_info[offset].ssid, credentials[i].ssid)) {
						data[65] = strlen(credentials[i].pass);
						memcpy(data + 66, credentials[i].pass, 63);
						break;
					}
				}
			} else {
				data[1] = 0;
			}
			data[129] = sio_cksum(data, 1, 128);
			uart_write_bytes(SIO_UART_NUM, data, 128 + 2);
			break;

		case DCMND_SELECT_NETWORK:
			send_sio_ack();
			delay_250us();
			size = 128;
			timeout = fetch_data(data, size);
			if (timeout) {
				ESP_LOGI(TAG, "select network data timeout (expected %d bytes)", (int) size + 1);
			} else if (sio_cksum(data, 0, size) != data[size]) {
				ESP_LOGE(TAG, "select network checksum mismatch");
				send_sio_nack();
			} else {
				send_sio_ack();
				delay_250us();
				send_sio_compl();
				data[data[0] + 1] = 0;
				for (i = 0; i < CREDENTIALS_LIST_SIZE && credentials[i].ssid[0] != 0; i++) {
					if (!strcmp((char *) ap_info[daux1].ssid, credentials[i].ssid)) {
						break;
					}
				}
				if (i < CREDENTIALS_LIST_SIZE) {
					strcpy(credentials[i].ssid, (char *) ap_info[daux1].ssid);
					strcpy(credentials[i].pass, (char *) data + 1);
					config_update_station_entry(i);
					reconnect(i);
				} else {
					ESP_LOGE(TAG, "too many WiFi network entries");
				}
			}
			break;

		case DCMND_SCAN_NETWORKS:
			send_sio_ack();
			delay_250us();
			wifi_switch(WIFI_MODE_STA, NULL, NULL);
			send_sio_compl();
			break;

		case DCMND_GET_AP_PARAMETERS:
			ESP_LOGI(TAG, "AP parameters: ssid '%s', pass '%s', ip '%s'\n", ap_ssid, ap_password, ap_ip);
			send_sio_ack();
			delay_250us();
			data[0] = 'C';
			memset(data + 1, 0, 128);
			data[1] = strlen(ap_ssid);
			strncpy((char *) data + 2, ap_ssid, 32);
			data[34] = strlen(ap_password);
			strncpy((char *) data + 35, ap_password, 32);
			data[67] = strlen(ap_ip);
			strncpy((char *) data + 68, ap_ip, 18);
			data[129] = sio_cksum(data, 1, 128);
			uart_write_bytes(SIO_UART_NUM, data, 128 + 2);
			break;

		case DCMND_SET_AP:
			send_sio_ack();
			delay_250us();
			size = 128;
			timeout = fetch_data(data, size);
			if (timeout) {
				ESP_LOGI(TAG, "set AP data timeout (expected %d bytes)", (int) size + 1);
			} else if (sio_cksum(data, 0, size) != data[size]) {
				ESP_LOGE(TAG, "set AP checksum mismatch");
				send_sio_nack();
			} else {
				send_sio_ack();
				delay_250us();
				send_sio_compl();
				size = data[0];
				memcpy(ap_ssid, data + 1, size);
				ap_ssid[size] = 0;
				size = data[33];
				memcpy(ap_password, data + 34, size);
				ap_password[size] = 0;
				size = data[66];
				memcpy(ap_ip, data + 67, size);
				config_update_ap_entry();
				wifi_switch(WIFI_MODE_AP, ap_ssid, ap_password);
			}
			break;

		case DCMND_GET_CHUNK:
		case DCMND_GET_NEXT_CHUNK:
			if (ddevic == 0x31 && drives[0].path[0] != 0 && drives[0].type == DRIVE_TYPE_XEX) {
				send_sio_ack();
				delay_250us();
				size = daux1 | (daux2 << 8);
				if (dcmnd == DCMND_GET_NEXT_CHUNK) {
					drives[0].chunk_offset += drives[0].prev_chunk_size;
					drives[0].prev_chunk_size = size;
				}
				data[0] = 'C';
				uint32_t src_i = drives[0].chunk_offset;
				for (i = 0; i < size; i++, src_i++) {
					data[1+i] = (src_i < drives[0].size) ? drives[0].buffer[src_i] : 0xff;
				}
				data[1+i] = sio_cksum(data, 1, size);
				ESP_LOGI(TAG, "sending %d bytes, offset %04x, cksum %02x", (int) size, (int) drives[0].chunk_offset, data[1+size]);
				uart_write_bytes(SIO_UART_NUM, data, 2 + size);
			} else {
				send_sio_error();
			}
			break;

		default:
			break;
	}
}

static void sio_task(void *arg)
{
	gpio_config_t io_conf = {};
	uart_config_t uart_config = {
		.baud_rate = 18866,
		.data_bits = UART_DATA_8_BITS,
		.parity = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
		.source_clk = UART_SCLK_DEFAULT
	};
	uint8_t data[5];
	int len = 0;
	uint8_t ddevic;
	uint8_t dcmnd;
	uint8_t daux1;
	uint8_t daux2;

	ESP_LOGI(TAG, "sio task starting");

	io_conf.intr_type = GPIO_INTR_ANYEDGE;
	io_conf.pin_bit_mask = 1 << SIO_CMD_PIN;
	io_conf.mode = GPIO_MODE_INPUT;
	io_conf.pull_down_en = 0;
	io_conf.pull_up_en = 1;
	gpio_config(&io_conf);
	gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
	gpio_isr_handler_add(SIO_CMD_PIN, gpio_isr_handler, NULL);

	io_conf.intr_type = GPIO_INTR_NEGEDGE;
	io_conf.pin_bit_mask = 1 << SETUP_ENABLE_PIN;
	io_conf.mode = GPIO_MODE_INPUT;
	io_conf.pull_down_en = 0;
	io_conf.pull_up_en = 1;
	gpio_config(&io_conf);
	gpio_isr_handler_add(SETUP_ENABLE_PIN, setup_enable_isr_handler, NULL);

	ESP_ERROR_CHECK(uart_driver_install(SIO_UART_NUM, SIO_BUFFER_SIZE, SIO_BUFFER_SIZE, 0, NULL, ESP_INTR_FLAG_IRAM));
	ESP_ERROR_CHECK(uart_param_config(SIO_UART_NUM, &uart_config));
	ESP_ERROR_CHECK(uart_set_pin(SIO_UART_NUM, SIO_UART_TXD_PIN, SIO_UART_RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

	uart_intr_config_t uart_intr;
	uart_intr.intr_enable_mask = UART_RXFIFO_FULL_INT_ENA_M
		| UART_RXFIFO_TOUT_INT_ENA_M
		| UART_FRM_ERR_INT_ENA_M
		| UART_RXFIFO_OVF_INT_ENA_M
		| UART_BRK_DET_INT_ENA_M
		| UART_PARITY_ERR_INT_ENA_M;
	uart_intr.rxfifo_full_thresh = 10;
	uart_intr.rx_timeout_thresh = 1; //UART_TOUT_THRESH_DEFAULT,  //10 works well for my short messages I need send/receive
	uart_intr.txfifo_empty_intr_thresh = 10; //UART_EMPTY_THRESH_DEFAULT
	uart_intr_config(SIO_UART_NUM, &uart_intr);

	const esp_timer_create_args_t timeout_timer_args = {
		.callback = &timeout_timer_callback,
		.dispatch_method = ESP_TIMER_ISR,
		.name = "timeout"
	};
	ESP_ERROR_CHECK(esp_timer_create(&timeout_timer_args, &timeout_timer));

	for (;;) {
		if (task_notify_take_indexed(NOTIFICATION_CMD_LO, TRUE, 0)) {
			uart_flush_input(SIO_UART_NUM);
		}
		if (task_notify_take_indexed(NOTIFICATION_CMD_HI, TRUE, 0)) {
			len = uart_read_bytes(SIO_UART_NUM, data, 5, 1);
			ESP_LOGI(TAG, "command (%d): %02x %02x %02x %02x %02x", len, data[0], data[1], data[2], data[3], data[4]);
			if (len == 5 && sio_cksum(data, 0, 4) == data[4]) {
				len = 0;
				ddevic = data[0];
				dcmnd = data[1];
				daux1 = data[2];
				daux2 = data[3];
				handle_sio_command(ddevic, dcmnd, daux1, daux2);
			} else if (len == 5) {
				ESP_LOGE(TAG, "checksum mismatch; resetting UART");
				uart_set_baudrate(SIO_UART_NUM, (F_OSC/2) / (7 + 40));
				uart_flush_input(SIO_UART_NUM);
				drives[0].chunk_offset = 0;
			} else if (len > 0) {
				ESP_LOGI(TAG, "sio uart error");
				uart_set_baudrate(SIO_UART_NUM, (F_OSC/2) / (7 + 40));
				uart_flush_input(SIO_UART_NUM);
				drives[0].chunk_offset = 0;
			}
		}
		if (task_notify_take_indexed(NOTIFICATION_ENABLE_SETUP, TRUE, 0)) {
			ESP_LOGI(TAG, "insert setup");
			insert_image(MOUNT_POINT "/" YASIO_SETUP_FNAME, 0);
		}
	}
}

static esp_err_t load_drive_state()
{
	char line[256];
	FILE *fp;
	int i = 0;
	int j;

	fp = fopen(MOUNT_POINT "/" YASIO_CONFIG_FNAME, "rt");
	if (fp == NULL) {
		return ESP_FAIL;
	}
	while (fgets(line, sizeof(line), fp) != NULL) {
		if (line[0] == 'D' && '1' <= line[1] && line[1] <= '4' && line[2] == ':') {
			if (line[3] != '\n') {
				j = line[1] - '1';
				for (i = 0; i < sizeof(line) && line[i] != '\n'; i++);
				line[i] = 0;
				do {
					line[i + sizeof(MOUNT_POINT) - 3] = line[i];
				} while (i-- > 0);
				strncpy(line, MOUNT_POINT, sizeof(MOUNT_POINT) - 1);
				line[sizeof(MOUNT_POINT) - 1] = '/';
				ESP_LOGI(TAG, "restoring '%s' in D%c:", line, j + '1');
				insert_image(line, j);
			}
		}
	}
	fclose(fp);

	return ESP_OK;
}

static esp_err_t load_wifi_config(wifi_mode_t *wifi_mode)
{
	char data[128];
	FILE *fp;
	int i = 0;
	int j;
	int k;

	*wifi_mode = WIFI_MODE_STA;

	fp = fopen(MOUNT_POINT "/" YASIO_CONFIG_FNAME, "rt");
	if (fp == NULL) {
		return ESP_FAIL;
	}
	while (!feof(fp)) {
		fgets(data, 64, fp);
		if (!strncmp(data, "MO:", 3)) {
			if (!strncmp(data, "MO:Station", 10)) {
				*wifi_mode = WIFI_MODE_STA;
			} else if (!strncmp(data, "MO:AccessPoint", 14)) {
				*wifi_mode = WIFI_MODE_AP;
			} else {
				ESP_LOGE(TAG, "unknown WiFi mode: %s", data + 3);
			}
		} else if (!strncmp(data, "AP:", 3)) {
			for (j = 3, k = 0; data[j] != ' ' && data[j] != '\n' && data[j]; j++, k++) {
				ap_ssid[k] = data[j];
			}
			ap_ssid[k] = 0;
			for (k = 0, j++; data[j] != ' ' && data[j] != '\n' && data[j]; j++, k++) {
				ap_password[k] = data[j];
			}
			ap_password[k] = 0;
			for (k = 0, j++; data[j] != '\n' && data[j]; j++, k++) {
				ap_ip[k] = data[j];
			}
			ap_ip[k] = 0;
		} else if (!strncmp(data, "ST:", 3)) {
			if (i < CREDENTIALS_LIST_SIZE) {
				for (j = 3, k = 0; data[j] != ' ' && data[j] != '\n' && data[j]; j++, k++) {
					credentials[i].ssid[k] = data[j];
				}
				credentials[i].ssid[k] = 0;
				for (k = 0, j++; data[j] != '\n' && data[j]; j++, k++) {
					credentials[i].pass[k] = data[j];
				}
				credentials[i].pass[k] = 0;
				i++;
			} else {
				ESP_LOGE(TAG, "too many credential entries in config");
			}
		}
	}
	fclose(fp);

	return ESP_OK;
}

static void common_wifi_init()
{
	ESP_ERROR_CHECK(esp_netif_init());
	ESP_ERROR_CHECK(esp_event_loop_create_default());

	sta_netif = esp_netif_create_default_wifi_sta();
	assert(sta_netif);
	ap_netif = esp_netif_create_default_wifi_ap();
	assert(ap_netif);

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));

	s_wifi_event_group = event_group_create();
	esp_event_handler_instance_t instance_any_id;
	esp_event_handler_instance_t instance_got_ip;
	ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id));
	ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip));
}

void app_main()
{
	wifi_mode_t wifi_mode;
	int i;

	for (i = 0; i < 4; i++) {
		drives[i].path[0] = 0;
		drives[i].buffer = NULL;
	}

	gpio_set_direction(DEBUG1_PIN, GPIO_MODE_OUTPUT);
	gpio_set_level(DEBUG1_PIN, 0);
	gpio_set_direction(DEBUG2_PIN, GPIO_MODE_OUTPUT);
	gpio_set_level(DEBUG2_PIN, 0);

	gpio_input_enable(SETUP_ENABLE_PIN);
	gpio_set_pull_mode(SETUP_ENABLE_PIN, GPIO_PULLUP_ONLY);

	//info();
	ESP_ERROR_CHECK(nvs_flash_init());

	ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_config, &led_chan));
	ESP_ERROR_CHECK(rmt_new_led_strip_encoder(&led_encoder));
	ESP_ERROR_CHECK(rmt_enable(led_chan));
	set_rgb(0x00, 0x00, 0x00);

	if (init_sdcard() != ESP_OK) {
		set_rgb(16, 0, 0);
		return;
	}
	strcpy(ap_ssid, "yasio");
	strcpy(ap_password, "yasio6502");
	strcpy(ap_ip, "192.168.4.1/24");
	load_wifi_config(&wifi_mode);

	if (gpio_get_level(SETUP_ENABLE_PIN) == 0) {
		ESP_LOGI(TAG, "inserting setup program");
		insert_image(MOUNT_POINT "/" YASIO_SETUP_FNAME, 0);
	} else {
		ESP_LOGI(TAG, "restoring state");
		load_drive_state();
	}

	connection_gate = semaphore_create_binary();
	open_gate("init");

	create_task_pinned_to_core(sio_task, "sio", 6144, NULL, 10, &sio_task_handle, 1);

	common_wifi_init();
	if (wifi_mode == WIFI_MODE_STA) {
		wifi_switch(WIFI_MODE_STA, NULL, NULL);
	} else if (wifi_mode == WIFI_MODE_AP) {
		wifi_switch(WIFI_MODE_AP, ap_ssid, ap_password);
	}
}
