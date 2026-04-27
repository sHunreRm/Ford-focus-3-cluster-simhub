
Проект позволяет использовать реальную приборную панель Ford Focus 3 как игровую через SimHub.

ESP8266 принимает телеметрию (скорость, обороты и т.д.) и отправляет CAN-сообщения в приборку через MCP2515.
<img width="1792" height="1440" alt="{218566DB-9904-4B42-BA35-5948A1636AB3}" src="https://github.com/user-attachments/assets/58c763ec-48b1-4b6d-9eae-d93820333ef3" />

---

## ⚙️ Оборудование

* ESP8266 (NodeMCU V3)
* MCP2515 CAN модуль
* Приборная панель Ford Focus 3
* Блок питания 12V
* Провода

---

## 🔌 Подключение

### 📡 ESP8266 → MCP2515<img width="684" height="437" alt="{F5D8253B-3BAB-4A44-A59A-2DEAD2E0DB1A}" src="https://github.com/user-attachments/assets/96a44501-4861-4388-907b-f3f0264945b8" />
<img width="802" height="529" alt="{801ECF83-8A41-4D0B-BD9A-81C1868577B5}" src="https://github.com/user-attachments/assets/c0736670-486a-4476-82ee-b3927b37a071" />


| MCP2515 | ESP8266     |
| ------- | ----------- |
| VCC     | 5V          |
| GND     | GND         |
| CS      | D8 (GPIO15) |
| SCK     | D5 (GPIO14) |
| MOSI    | D7 (GPIO13) |
| MISO    | D6 (GPIO12) |
| INT     | D1 (GPIO5)  |

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

настройки - плагины - найдите и включите пользовательские серийные устройства
<img width="1834" height="1005" alt="{20287173-9975-4C93-8DC9-AFAF801A2083}" src="https://github.com/user-attachments/assets/55700197-abde-46d9-b86c-7ac4ffcebe03" />

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


РАСПИНОВКА INSTRUMENT CLUSTER FORD FOCUS 3
1- MS CAN LOW
2- MS CAN HIGT
3- "+"
4- MM CAN LOW
5- MM CAN HIGT
6- Switch signal GND
7- TOGGLE SIGNAL DIM
8- TOGGLE SIGNAL MFD
9- "NOT IN PARK" SIGNAL (Только для АT)
10 - Power GND
12- CRANK DETECT 
