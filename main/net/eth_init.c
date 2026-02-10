#include "eth_init.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "esp_check.h"
#include "driver/gpio.h"
#include "freertos/event_groups.h"
#include "sdkconfig.h"

static const char *TAG = "eth";

#define ETH_GOT_IP_BIT BIT0

static EventGroupHandle_t s_eth_event_group = NULL;

static void eth_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    if (event_base == ETH_EVENT) {
        switch (event_id) {
        case ETHERNET_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Ethernet link up");
            break;
        case ETHERNET_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "Ethernet link down");
            break;
        case ETHERNET_EVENT_START:
            ESP_LOGI(TAG, "Ethernet started");
            break;
        case ETHERNET_EVENT_STOP:
            ESP_LOGI(TAG, "Ethernet stopped");
            break;
        default:
            break;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_ETH_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        if (s_eth_event_group) {
            xEventGroupSetBits(s_eth_event_group, ETH_GOT_IP_BIT);
        }
    }
}

esp_err_t eth_init(void)
{
    s_eth_event_group = xEventGroupCreate();

    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "netif init failed");
    ESP_RETURN_ON_ERROR(esp_event_loop_create_default(), TAG, "event loop failed");

    /* Create default Ethernet netif */
    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    esp_netif_t *eth_netif = esp_netif_new(&netif_cfg);

    /* Configure internal EMAC */
    eth_mac_config_t mac_cfg = ETH_MAC_DEFAULT_CONFIG();
    eth_esp32_emac_config_t emac_cfg = ETH_ESP32_EMAC_DEFAULT_CONFIG();
    emac_cfg.smi_gpio.mdc_num = CONFIG_ETH_MDC_GPIO;
    emac_cfg.smi_gpio.mdio_num = CONFIG_ETH_MDIO_GPIO;
    esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&emac_cfg, &mac_cfg);

    /* Configure PHY */
    eth_phy_config_t phy_cfg = ETH_PHY_DEFAULT_CONFIG();
    phy_cfg.phy_addr = CONFIG_ETH_PHY_ADDR;
    phy_cfg.reset_gpio_num = CONFIG_ETH_PHY_RST_GPIO;
    esp_eth_phy_t *phy = esp_eth_phy_new_ip101(&phy_cfg);

    /* Install Ethernet driver */
    esp_eth_config_t eth_cfg = ETH_DEFAULT_CONFIG(mac, phy);
    esp_eth_handle_t eth_handle = NULL;
    ESP_RETURN_ON_ERROR(esp_eth_driver_install(&eth_cfg, &eth_handle), TAG, "driver install failed");

    /* Attach to netif */
    ESP_RETURN_ON_ERROR(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handle)),
                        TAG, "netif attach failed");

    /* Register event handlers */
    ESP_RETURN_ON_ERROR(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID,
                        eth_event_handler, NULL), TAG, "ETH event handler failed");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP,
                        eth_event_handler, NULL), TAG, "IP event handler failed");

    /* Start Ethernet */
    ESP_RETURN_ON_ERROR(esp_eth_start(eth_handle), TAG, "eth start failed");

    ESP_LOGI(TAG, "Ethernet initialized, waiting for link...");
    return ESP_OK;
}

esp_err_t eth_wait_for_ip(TickType_t timeout_ticks)
{
    EventBits_t bits = xEventGroupWaitBits(s_eth_event_group, ETH_GOT_IP_BIT,
                                           pdFALSE, pdTRUE, timeout_ticks);
    if (bits & ETH_GOT_IP_BIT) {
        return ESP_OK;
    }
    ESP_LOGW(TAG, "Timeout waiting for IP address");
    return ESP_ERR_TIMEOUT;
}
