# Kconfig.projbuild

**Путь:** `main/Kconfig.projbuild`

**Назначение:** Конфигурационные параметры проекта HMI-дисплея для системы menuconfig ESP-IDF.

---

## Описание

Файл `Kconfig.projbuild` определяет пользовательские параметры конфигурации, доступные через систему `idf.py menuconfig`. Параметры группируются в меню "RO HMI Configuration" и включают настройки MQTT-брокера и Ethernet-интерфейса.

Значения этих параметров записываются в файл `sdkconfig` при сборке и доступны в коде через макросы `CONFIG_*` (например, `CONFIG_MQTT_BROKER_URI`).

---

## Структура меню

```
RO HMI Configuration
 +-- MQTT Settings
 |     +-- MQTT Broker URI
 |     +-- MQTT Client ID
 +-- Ethernet Settings
       +-- MDC GPIO
       +-- MDIO GPIO
       +-- PHY Address
       +-- PHY Reset GPIO
```

---

## Параметры конфигурации

### Меню "MQTT Settings"

| Параметр | Макрос в коде | Тип | Значение по умолчанию | Описание |
|----------|---------------|-----|----------------------|----------|
| MQTT Broker URI | `CONFIG_MQTT_BROKER_URI` | string | `mqtt://192.168.1.1:1883` | URI MQTT-брокера (Mosquitto на Orange Pi) |
| MQTT Client ID | `CONFIG_MQTT_CLIENT_ID` | string | `ro_hmi` | Идентификатор MQTT-клиента |

**Примечания:**
- URI MQTT-брокера указывает на Orange Pi с Mosquitto по адресу 192.168.1.1, порт 1883 (стандартный)
- Client ID `ro_hmi` должен быть уникальным среди всех MQTT-клиентов в сети
- Поддерживаются схемы: `mqtt://` (TCP), `mqtts://` (TLS), `ws://` (WebSocket)

### Меню "Ethernet Settings"

| Параметр | Макрос в коде | Тип | Значение по умолчанию | Описание |
|----------|---------------|-----|----------------------|----------|
| MDC GPIO | `CONFIG_ETH_MDC_GPIO` | int | 31 | GPIO для линии MDC интерфейса MDIO |
| MDIO GPIO | `CONFIG_ETH_MDIO_GPIO` | int | 52 | GPIO для линии MDIO интерфейса MDIO |
| PHY Address | `CONFIG_ETH_PHY_ADDR` | int | 1 | Адрес PHY-чипа на шине MDIO |
| PHY Reset GPIO | `CONFIG_ETH_PHY_RST_GPIO` | int | -1 | GPIO сброса PHY (-1 = не используется) |

**Примечания:**
- MDC и MDIO — линии управления PHY-чипом Ethernet (Management Data Clock / Data)
- PHY Address = 1 — стандартный адрес для большинства Ethernet PHY-чипов
- PHY Reset GPIO = -1 означает, что аппаратный сброс PHY не подключён (используется программный)

---

## Использование в коде

Параметры доступны через макросы `CONFIG_*`:

```c
// В mqtt_app.c:
esp_mqtt_client_config_t mqtt_cfg = {
    .broker.address.uri = CONFIG_MQTT_BROKER_URI,
    .credentials.client_id = CONFIG_MQTT_CLIENT_ID,
};

// В eth_init.c:
eth_mac_config_t mac_config = {
    .smi_mdc_gpio_num = CONFIG_ETH_MDC_GPIO,
    .smi_mdio_gpio_num = CONFIG_ETH_MDIO_GPIO,
};
esp_eth_phy_new_ip101(..., CONFIG_ETH_PHY_ADDR, ...);
```

---

## Сетевая архитектура

```
ESP32-P4 (HMI дисплей)                    Orange Pi (брокер)
     |                                          |
     +-- Ethernet PHY (MDIO: GPIO31/52)         |
     |       |                                  |
     |       +-- IP: получается по DHCP         |
     |                                          |
     +-- MQTT Client (ro_hmi) ---- TCP:1883 --> MQTT Broker (Mosquitto)
             |                                  |
             +-- Подписка: ro_plant/status/#    +-- ESP32-S3 (основной контроллер)
             +-- Подписка: ro_plant/alarms      |     публикует данные
             +-- Публикация: ro_plant/command/* |
```

---

## Связь с другими модулями

- **mqtt_app.c** — использует `CONFIG_MQTT_BROKER_URI` и `CONFIG_MQTT_CLIENT_ID`
- **eth_init.c** — использует `CONFIG_ETH_MDC_GPIO`, `CONFIG_ETH_MDIO_GPIO`, `CONFIG_ETH_PHY_ADDR`, `CONFIG_ETH_PHY_RST_GPIO`
