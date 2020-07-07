#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "driver/mcpwm.h"
#include "esp_attr.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "mqtt_client.h"
#include "nvs_flash.h"
#include "protocol_examples_common.h"
#include "soc/mcpwm_periph.h"
#include "tcpip_adapter.h"

static const char *TAG = "MAILBOX";

#define SERVO_MIN_PULSEWIDTH 435   // Minimum pulse width in microsecond
#define SERVO_MAX_PULSEWIDTH 2320  // Maximum pulse width in microsecond
#define SERVO_MAX_DEGREE 180       // Maximum angle in degree upto which servo can rotate

static const int SERVO_ANGLE_FOR_FLAG_UP = 125;
static const int SERVO_ANGLE_FOR_FLAG_DOWN = 30;
static const int MAX_MESSAGE_LENGTH = 5;

static void mcpwm_example_gpio_initialize() {
  mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, 13);  // Set GPIO 18 as PWM0A, to which servo is connected
}

/**
 * @brief Use this function to calcute pulse width for per degree rotation
 *
 * @param  degree_of_rotation the angle in degree to which servo has to rotate
 *
 * @return
 *     - calculated pulse width
 */
static uint32_t servo_per_degree_init(uint32_t degree_of_rotation) {
  return (SERVO_MIN_PULSEWIDTH +
          (((SERVO_MAX_PULSEWIDTH - SERVO_MIN_PULSEWIDTH) * (degree_of_rotation)) / (SERVO_MAX_DEGREE)));
}

static void put_flag_up() {
  mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, servo_per_degree_init(SERVO_ANGLE_FOR_FLAG_UP));
}

static void put_flag_down() {
  mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, servo_per_degree_init(SERVO_ANGLE_FOR_FLAG_DOWN));
}

static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event) {
  esp_mqtt_client_handle_t client = event->client;
  char message[MAX_MESSAGE_LENGTH];
  // your_context_t *context = event->context;
  switch (event->event_id) {
    case MQTT_EVENT_CONNECTED:
      ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
      esp_mqtt_client_subscribe(client, "/mailbox/flag", 0);
      break;
    case MQTT_EVENT_DISCONNECTED:
      ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
      break;
    case MQTT_EVENT_SUBSCRIBED:
      ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
      break;
    case MQTT_EVENT_UNSUBSCRIBED:
      ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
      break;
    case MQTT_EVENT_PUBLISHED:
      ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
      break;
    case MQTT_EVENT_DATA:
      ESP_LOGI(TAG, "MQTT_EVENT_DATA");
      printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
      printf("DATA_LEN=%i\r\n", event->data_len);
      printf("DATA=%.*s\r\n", event->data_len, event->data);

      if (event->data_len > MAX_MESSAGE_LENGTH) {
          ESP_LOGI(TAG, "Data length %i exceeds maximum data length %i", event->data_len, MAX_MESSAGE_LENGTH);
          break;
      }

      strncpy(message, event->data, event->data_len);
      message[event->data_len] = '\0'; // Array isn't null terminated

      if (strcmp(message, "up") == 0) {
        put_flag_up();
      } else if (strcmp(message, "down") == 0) {
        put_flag_down();
      }
      break;
    case MQTT_EVENT_ERROR:
      ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
      break;
    default:
      ESP_LOGI(TAG, "Other event id:%d", event->event_id);
      break;
  }
  return ESP_OK;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
  mqtt_event_handler_cb(event_data);
}

static void mqtt_app_start(void) {
  esp_mqtt_client_config_t mqtt_cfg = {
      .uri = CONFIG_BROKER_URL,
  };

  esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
  esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
  esp_mqtt_client_start(client);
}

void app_main() {
  ESP_LOGI(TAG, "[APP] Startup..");
  ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
  ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

  esp_log_level_set("*", ESP_LOG_INFO);
  esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
  esp_log_level_set("MQTT_EXAMPLE", ESP_LOG_VERBOSE);
  esp_log_level_set("TRANSPORT_TCP", ESP_LOG_VERBOSE);
  esp_log_level_set("TRANSPORT_SSL", ESP_LOG_VERBOSE);
  esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
  esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);

  ESP_ERROR_CHECK(nvs_flash_init());
  tcpip_adapter_init();
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  // 1. mcpwm gpio initialization
  mcpwm_example_gpio_initialize();

  // 2. initial mcpwm configuration
  mcpwm_config_t pwm_config;
  pwm_config.frequency = 50;  // frequency = 50Hz, i.e. for every servo motor time period should be 20ms
  pwm_config.cmpr_a = 0;      // duty cycle of PWMxA = 0
  pwm_config.cmpr_b = 0;      // duty cycle of PWMxb = 0
  pwm_config.counter_mode = MCPWM_UP_COUNTER;
  pwm_config.duty_mode = MCPWM_DUTY_MODE_0;
  mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &pwm_config);  // Configure PWM0A & PWM0B with above settings

  /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
   * Read "Establishing Wi-Fi or Ethernet Connection" section in
   * examples/protocols/README.md for more information about this function.
   */
  ESP_ERROR_CHECK(example_connect());

  mqtt_app_start();
}
