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
            /* MQTT status update moved to mqtt_app.c (MQTT_EVENT_DISCONNECTED handler)
               to maintain HAL independence from services layer */
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
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_ETH_LOST_IP) {
        ESP_LOGW(TAG, "Lost IP address");
        /* MQTT status update moved to mqtt_app.c (MQTT_EVENT_DISCONNECTED handler)
           to maintain HAL independence from services layer */
        // Сбросить бит ETH_GOT_IP_BIT при потере IP, чтобы eth_wait_for_ip()
        // не вернул OK сразу при повторном коннекте с link down
        if (s_eth_event_group) {
            xEventGroupClearBits(s_eth_event_group, ETH_GOT_IP_BIT);
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
/** Static storage for Ethernet driver handles (freed by eth_deinit) */
static esp_eth_handle_t s_eth_handle = NULL;
static esp_eth_mac_t *s_mac = NULL;
static esp_eth_phy_t *s_phy = NULL;
static esp_netif_t *s_eth_netif = NULL;

esp_err_t eth_init(void)
{
    esp_err_t err = ESP_OK;

    s_eth_event_group = xEventGroupCreate();
    configASSERT(s_eth_event_group);

    // Инициализация TCP/IP стека
    err = esp_netif_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "netif init failed: %s", esp_err_to_name(err));
        goto cleanup;
    }

    // Создание цикла обработки событий по умолчанию
    // ESP_ERR_INVALID_STATE означает, что кто-то другой (BT, NVS, Wi-Fi) уже создал loop
    esp_err_t loop_err = esp_event_loop_create_default();
    if (loop_err != ESP_OK && loop_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "esp_event_loop_create_default failed: %s", esp_err_to_name(loop_err));
        err = loop_err;
        goto cleanup;
    }
    if (loop_err == ESP_ERR_INVALID_STATE) {
        ESP_LOGI(TAG, "default event loop already exists, reusing");
    }

    /* Создание сетевого интерфейса Ethernet по умолчанию */
    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    s_eth_netif = esp_netif_new(&netif_cfg);
    if (!s_eth_netif) {
        ESP_LOGE(TAG, "Failed to create netif");
        err = ESP_FAIL;
        goto cleanup;
    }

    /* Конфигурация внутреннего EMAC контроллера ESP32-P4 */
    eth_mac_config_t mac_cfg = ETH_MAC_DEFAULT_CONFIG();
    eth_esp32_emac_config_t emac_cfg = ETH_ESP32_EMAC_DEFAULT_CONFIG();
    emac_cfg.smi_gpio.mdc_num = CONFIG_ETH_MDC_GPIO;   // GPIO тактирования SMI (MDC)
    emac_cfg.smi_gpio.mdio_num = CONFIG_ETH_MDIO_GPIO; // GPIO данных SMI (MDIO)
    s_mac = esp_eth_mac_new_esp32(&emac_cfg, &mac_cfg);
    if (!s_mac) {
        ESP_LOGE(TAG, "Failed to create MAC");
        err = ESP_FAIL;
        goto cleanup;
    }

    /* Конфигурация PHY-чипа IP101 */
    eth_phy_config_t phy_cfg = ETH_PHY_DEFAULT_CONFIG();
    phy_cfg.phy_addr = -1;                             // Autodetect адреса PHY (0 или 1 в зависимости от страпов на плате)
    phy_cfg.reset_gpio_num = CONFIG_ETH_PHY_RST_GPIO;  // GPIO аппаратного сброса PHY
    s_phy = esp_eth_phy_new_ip101(&phy_cfg);
    if (!s_phy) {
        ESP_LOGE(TAG, "Failed to create PHY");
        err = ESP_FAIL;
        goto cleanup;
    }

    /* Установка Ethernet-драйвера */
    esp_eth_config_t eth_cfg = ETH_DEFAULT_CONFIG(s_mac, s_phy);
    err = esp_eth_driver_install(&eth_cfg, &s_eth_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "driver install failed: %s", esp_err_to_name(err));
        goto cleanup;
    }

    /* Привязка Ethernet-драйвера к сетевому интерфейсу */
    err = esp_netif_attach(s_eth_netif, esp_eth_new_netif_glue(s_eth_handle));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "netif attach failed: %s", esp_err_to_name(err));
        goto cleanup;
    }

    /* Регистрация обработчиков событий Ethernet и IP */
    err = esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID,
                                     eth_event_handler, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ETH event handler registration failed: %s", esp_err_to_name(err));
        goto cleanup;
    }

    err = esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP,
                                     eth_event_handler, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "IP event handler registration failed: %s", esp_err_to_name(err));
        goto cleanup;
    }

    err = esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_LOST_IP,
                                     eth_event_handler, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "IP lost event handler registration failed: %s", esp_err_to_name(err));
        goto cleanup;
    }

    /* Запуск Ethernet-драйвера */
    err = esp_eth_start(s_eth_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "eth start failed: %s", esp_err_to_name(err));
        goto cleanup;
    }

    /* Логирование MAC-адреса для отладки (DHCP-резервации, настройка коммутаторов) */
    uint8_t mac[6];
    if (esp_eth_ioctl(s_eth_handle, ETH_CMD_G_MAC_ADDR, mac) == ESP_OK) {
        ESP_LOGI(TAG, "Ethernet MAC: %02x:%02x:%02x:%02x:%02x:%02x",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }

    ESP_LOGI(TAG, "Ethernet initialized, waiting for link...");
    return ESP_OK;

cleanup:
    if (s_eth_handle) {
        esp_eth_driver_uninstall(s_eth_handle);
        s_eth_handle = NULL;
    }
    if (s_mac) {
        s_mac->del(s_mac);
        s_mac = NULL;
    }
    if (s_phy) {
        s_phy->del(s_phy);
        s_phy = NULL;
    }
    if (s_eth_netif) {
        esp_netif_destroy(s_eth_netif);
        s_eth_netif = NULL;
    }
    return err;
}

/**
 * Деинициализация Ethernet и освобождение ресурсов.
 *
 * Останавливает Ethernet-драйвер и освобождает выделенные ресурсы.
 * Безопасна при вызове несколько раз (проверяет null-pointers).
 *
 * @return ESP_OK при успехе, код ошибки при неудаче
 */
esp_err_t eth_deinit(void)
{
    if (s_eth_handle) {
        esp_eth_driver_uninstall(s_eth_handle);
        s_eth_handle = NULL;
    }
    if (s_mac) {
        s_mac->del(s_mac);
        s_mac = NULL;
    }
    if (s_phy) {
        s_phy->del(s_phy);
        s_phy = NULL;
    }
    if (s_eth_netif) {
        esp_netif_destroy(s_eth_netif);
        s_eth_netif = NULL;
    }
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
    if (!s_eth_event_group) {
        ESP_LOGE(TAG, "Event group not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    /* Single bit: xWaitForAllBits vs xWaitForAnyBits semantically identical;
       pdFALSE (any) is more idiomatic for single-bit waits */
    EventBits_t bits = xEventGroupWaitBits(s_eth_event_group, ETH_GOT_IP_BIT,
                                           pdFALSE, pdFALSE, timeout_ticks);
    if (bits & ETH_GOT_IP_BIT) {
        return ESP_OK;
    }
    ESP_LOGW(TAG, "Timeout waiting for IP address");
    return ESP_ERR_TIMEOUT;
}
#endif /* !LVGL_LIVE_PREVIEW */
