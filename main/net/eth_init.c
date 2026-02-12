/**
 * @file eth_init.c
 * @brief Инициализация Ethernet-интерфейса на встроенном EMAC контроллере ESP32-P4.
 *
 * Настраивает внутренний MAC, PHY-чип IP101, сетевой стек (netif),
 * регистрирует обработчики событий Ethernet/IP и запускает драйвер.
 * Используется для связи HMI-дисплея с основным контроллером RO-установки
 * через локальную сеть (MQTT-брокер на 192.168.1.1).
 */
#ifndef LVGL_LIVE_PREVIEW
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

/** Бит события FreeRTOS: IP-адрес получен по DHCP */
#define ETH_GOT_IP_BIT BIT0

/** Группа событий для синхронизации ожидания получения IP-адреса */
static EventGroupHandle_t s_eth_event_group = NULL;

/**
 * Обработчик событий Ethernet и IP.
 *
 * Обрабатывает следующие события:
 * - ETHERNET_EVENT_CONNECTED    -- физическое соединение установлено
 * - ETHERNET_EVENT_DISCONNECTED -- кабель отключен
 * - ETHERNET_EVENT_START        -- драйвер Ethernet запущен
 * - ETHERNET_EVENT_STOP         -- драйвер Ethernet остановлен
 * - IP_EVENT_ETH_GOT_IP         -- IP-адрес получен (DHCP/статика),
 *                                  устанавливает бит ETH_GOT_IP_BIT
 */
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
        // Сигнализируем ожидающим задачам о получении IP
        if (s_eth_event_group) {
            xEventGroupSetBits(s_eth_event_group, ETH_GOT_IP_BIT);
        }
    }
}

/**
 * Инициализация Ethernet-интерфейса.
 *
 * Последовательность инициализации:
 * 1. Создание группы событий FreeRTOS для синхронизации
 * 2. Инициализация сетевого стека (esp_netif) и цикла событий
 * 3. Создание сетевого интерфейса Ethernet по умолчанию
 * 4. Конфигурация внутреннего EMAC (GPIO для SMI: MDC, MDIO)
 * 5. Конфигурация PHY-чипа IP101 (адрес, GPIO сброса)
 * 6. Установка и подключение Ethernet-драйвера к netif
 * 7. Регистрация обработчиков событий ETH и IP
 * 8. Запуск Ethernet-драйвера
 *
 * @return ESP_OK при успешной инициализации, код ошибки при неудаче
 */
esp_err_t eth_init(void)
{
    s_eth_event_group = xEventGroupCreate();

    // Инициализация TCP/IP стека
    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "netif init failed");
    // Создание цикла обработки событий по умолчанию
    ESP_RETURN_ON_ERROR(esp_event_loop_create_default(), TAG, "event loop failed");

    /* Создание сетевого интерфейса Ethernet по умолчанию */
    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    esp_netif_t *eth_netif = esp_netif_new(&netif_cfg);

    /* Конфигурация внутреннего EMAC контроллера ESP32-P4 */
    eth_mac_config_t mac_cfg = ETH_MAC_DEFAULT_CONFIG();
    eth_esp32_emac_config_t emac_cfg = ETH_ESP32_EMAC_DEFAULT_CONFIG();
    emac_cfg.smi_gpio.mdc_num = CONFIG_ETH_MDC_GPIO;   // GPIO тактирования SMI (MDC)
    emac_cfg.smi_gpio.mdio_num = CONFIG_ETH_MDIO_GPIO; // GPIO данных SMI (MDIO)
    esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&emac_cfg, &mac_cfg);

    /* Конфигурация PHY-чипа IP101 */
    eth_phy_config_t phy_cfg = ETH_PHY_DEFAULT_CONFIG();
    phy_cfg.phy_addr = CONFIG_ETH_PHY_ADDR;            // Адрес PHY на шине SMI
    phy_cfg.reset_gpio_num = CONFIG_ETH_PHY_RST_GPIO;  // GPIO аппаратного сброса PHY
    esp_eth_phy_t *phy = esp_eth_phy_new_ip101(&phy_cfg);

    /* Установка Ethernet-драйвера */
    esp_eth_config_t eth_cfg = ETH_DEFAULT_CONFIG(mac, phy);
    esp_eth_handle_t eth_handle = NULL;
    ESP_RETURN_ON_ERROR(esp_eth_driver_install(&eth_cfg, &eth_handle), TAG, "driver install failed");

    /* Привязка Ethernet-драйвера к сетевому интерфейсу */
    ESP_RETURN_ON_ERROR(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handle)),
                        TAG, "netif attach failed");

    /* Регистрация обработчиков событий Ethernet и IP */
    ESP_RETURN_ON_ERROR(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID,
                        eth_event_handler, NULL), TAG, "ETH event handler failed");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP,
                        eth_event_handler, NULL), TAG, "IP event handler failed");

    /* Запуск Ethernet-драйвера */
    ESP_RETURN_ON_ERROR(esp_eth_start(eth_handle), TAG, "eth start failed");

    ESP_LOGI(TAG, "Ethernet initialized, waiting for link...");
    return ESP_OK;
}

/**
 * Блокирующее ожидание получения IP-адреса по Ethernet.
 *
 * Использует FreeRTOS EventGroup для ожидания бита ETH_GOT_IP_BIT,
 * который устанавливается обработчиком события IP_EVENT_ETH_GOT_IP.
 * Бит не сбрасывается после чтения (pdFALSE), чтобы повторные вызовы
 * возвращали результат немедленно.
 *
 * @param timeout_ticks Максимальное время ожидания в тиках FreeRTOS
 * @return ESP_OK если IP-адрес получен, ESP_ERR_TIMEOUT при превышении таймаута
 */
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
#endif /* !LVGL_LIVE_PREVIEW */
