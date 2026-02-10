#pragma once

/**
 * Route and parse incoming MQTT message. Updates plant_data and alarm_ring.
 * Called from MQTT event handler in mqtt_client.c.
 */
void mqtt_handle_incoming(const char *topic, int topic_len,
                          const char *data, int data_len);
