# Control of meter PZEM-004T with ESP32 + Modbus
Base program for ESP32 where basic functions for meter testing are included

Programa base para ESP32 donde se incluyen funciones basicas para la prueba del medidor, desarrollado con VisualMicro.

Tambien compila con Arduino IDE ver 1.8.19

Arduino ESP32 ver 2.0.3

ModBus lib eModbus 1.5.0 (https://github.com/eModbus/eModbus)

First: make a wiring connection for PZEM-004 v3.0 and ESP32: 

ESP32 +3.3V			  ===> PZEM_5V

ESP32 GND  			  <==> PZEM_GND

ESP32 Tx (GPIO17)	===> PZEM_Rx

ESP32 Rx (GPIO16)	<=== PZEM_Tx

Se puede alimentar el PZEM con +3.3V y las comunicaciones no se ven afectadas.
De esta forma no es necesario un adaptador de niveles de tension para las señales Tx/Rx del ESP32

El esquema del PZEM-004 v3.0 se puede encontrar en el siguiente enlace 
https://www.youtube.com/watch?v=qRsjsenvlJA
https://www.youtube.com/c/TheHWcave

