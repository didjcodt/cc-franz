#include "espmissingincludes.h"
#include "ets_sys.h"
#include "osapi.h"
#include "os_type.h"
#include "uart.h"
#include "websocketd.h"

#define RX_PRIO 0
#define RX_QUEUE_LEN 1
#define COMMAND_MAXLEN 1024

os_event_t user_procTaskQueue[RX_QUEUE_LEN];
static char *statusCommand = "STATUS:";
static uint8_t commandMatcher = 0;
static uint8_t commandMatched = FALSE;
static char command[COMMAND_MAXLEN];
static uint32_t commandPosition = 0;


//Main code function
static void ICACHE_FLASH_ATTR rxLoop(os_event_t *events) {

	int c = uart0_rx_one_char();

	if (c != -1) {

		if (commandMatched == TRUE) {
			if (c == '\n' || c == '\r') {
				broadcastWsMessage(command, commandPosition, FLAG_FIN | OPCODE_TEXT);
				commandMatched = FALSE;
				commandPosition = 0;
				commandMatcher = 0;
			}else{
				command[commandPosition++] = c;
			}

			if (commandPosition == COMMAND_MAXLEN) {
				commandMatched = FALSE;
				commandPosition = 0;
				commandMatcher = 0;
			}
		} else {
			if (statusCommand[commandMatcher] == c) {
				commandMatcher++;

				if (commandMatcher == strlen(statusCommand)) {
					commandMatched = TRUE;
				}
			} else {
				commandMatcher = 0;
			}
		}
	}

	system_os_post(RX_PRIO, 0, 0 );
}

void onWsMessage(WSConnection *connection, const WSFrame *message) {
	for (int i = 0; i < message->payloadLength; i++) {
		stdoutPutchar(message->payloadData[i]);
	}
	stdoutPutchar('\n');
}

void onWsConnection(WSConnection *connection) {
	connection->onMessage = &onWsMessage;
	os_printf("New Websocket Client Connected");
}

void wifiInit(){
	// set as Software Access Point
	wifi_set_opmode(SOFTAP_MODE); 	

	// set modules IP adress
	struct ip_info info;
	IP4_ADDR(&info.ip,192,168,4,1);
	IP4_ADDR(&info.gw,192,168,4,1);
	IP4_ADDR(&info.netmask,255,255,255,0);
	wifi_set_ip_info(SOFTAP_IF,&info);

	// basic WIFI setup
	struct softap_config  apConfig = {
		.ssid = "CameraControl",
		.password = "",
		.channel = 8,
		.authmode = 0,
		.ssid_hidden = 0,
		.max_connection = 5
	};

	// load WIFI setup config for Software Access Point
	wifi_softap_set_config(&apConfig);
}


//Main routine. Initialize stdout, the I/O and the webserver and we're done.
void user_init(void) {
	uart_init();
//	ioInit();
	wifiInit();
	websocketdInit(8080, &onWsConnection);

	//Start os task
	system_os_task(rxLoop, RX_PRIO, user_procTaskQueue, RX_QUEUE_LEN);
	system_os_post(RX_PRIO, 0, 0 );

	os_printf("\nReady\n");
}
