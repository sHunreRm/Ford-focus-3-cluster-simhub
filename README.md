
Проект позволяет использовать реальную приборную панель Ford Focus 3 как игровую через SimHub.

ESP8266 принимает телеметрию (скорость, обороты и т.д.) и отправляет CAN-сообщения в приборку через MCP2515.

---

## ⚙️ Оборудование

* ESP8266 (NodeMCU V3)
* MCP2515 CAN модуль
* Приборная панель Ford Focus 3
* Блок питания 12V
* Провода

---

## 🔌 Подключение

### 📡 ESP8266 → MCP2515

| MCP2515 | ESP8266     |
| ------- | ----------- |
| VCC     | 5V          |
| GND     | GND         |
| CS      | D8 (GPIO15) |
| SCK     | D5 (GPIO14) |
| MOSI    | D7 (GPIO13) |
| MISO    | D6 (GPIO12) |
| INT     | D1          |

---

### 🚗 MCP2515 → Приборка (контакты приборки)

| MCP2515 |  Приборка  |
| ------- | ---------  |
| CANH    | pin 2      |
| CANL    | pin 1      |

---

### ⚡ Питание приборки

| Пин приборки | Назначение      |
| ------------ | --------------- |
| 3            | +12V            |
| 10           | GND             |


---

## ⚠️ ВАЖНЫЕ МОМЕНТЫ

* ОБЩИЙ GND блока питания, esp8266, MCP2515 обязателен!!!
* CAN шина чувствительна к помехам
* при необходимости использовать резистор 120 Ом между CANH и CANL

---

## 🖥️ SimHub

Используется кастомный Serial/UDP формат:

'SPD:'     + format([DataCorePlugin.GameData.NewData.SpeedKmh],'0') +
';RPM:'    + format([DataCorePlugin.GameData.NewData.Rpms],'0') +
';FUEL:' + format([DataCorePlugin.GameData.NewData.FuelPercent],'0') +
';COOLANT:'+ format([DataCorePlugin.GameData.NewData.WaterTemperature],'0') +
';L:'      + (if([DataCorePlugin.GameData.TurnIndicatorLeft],'1','0')) +
';R:'      + (if([DataCorePlugin.GameData.TurnIndicatorRight],'1','0')) +
';HB:'     + (if([DataCorePlugin.GameData.NewData.Handbrake]>0,'1','0')) +
';BRAKE:'  + (if([DataCorePlugin.GameData.NewData.Brake]>0,'1','0')) +
';ABS:'    + (if([DataCorePlugin.GameData.NewData.ABSActive],'1','0')) +
';ESP:'    + (if([DataCorePlugin.GameData.NewData.TCSActive],'1','0')) +
';TCOFF:'  + (if([DataCorePlugin.GameData.NewData.TCCutted],'1','0')) +
';LIGHTS:' + (if([DataCorePlugin.GameData.NewData.Headlights],'1','0')) +
';HIBEAM:' + (if([DataCorePlugin.GameData.NewData.HighBeams],'1','0')) +
';FOG:'    + (if([DataCorePlugin.GameData.NewData.FogLight],'1','0')) +
';LOWFUEL:'+ (if([DataCorePlugin.GameData.CarSettings_FuelAlertActive],'1','0')) +
'\n'


---

## 🧪 Тестирование

Тестировалось на:

* ESP8266
* MCP2515
* FF3 Cluster

---

## 📦 Структура проекта

```
src/
 └── main.cpp
platformio.ini
README.md
```

---

## 🚀 Как запустить

1. Открыть проект в PlatformIO
2. Подключить ESP8266
3. Залить прошивку
4. Подключить SimHub
5. запустить приборку
6. Запустить игру

---
