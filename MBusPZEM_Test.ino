// =================================================================================================
// eModbus: Copyright 2020 by Michael Harwerth, Bert Melis and the contributors to ModbusClient
//               MIT license - see license.md for details
// =================================================================================================
//Read the docs at http://emodbus.github.io
//2022-07-04 - Version modified by jaggil

// Example code to show the usage of the eModbus library. 
// Please refer to root/Readme.md for a full description.

// Note: this is an example for the "PZEM-004T_v3.0 Modbus" power meter!

// Includes: <Arduino.h> for Serial etc.
#include <Arduino.h>

// Include the header for the ModbusClient RTU style
#include "ModbusClientRTU.h"
#include "Logging.h"

// Definitions for this special case
#define RXPIN GPIO_NUM_16
#define TXPIN GPIO_NUM_17
#define REDEPIN GPIO_NUM_4
#define BAUDRATE 9600
//#define FIRST_REGISTER 0x002A
#define FIRST_REGISTER 0x0000
#define NUM_VALUES 5
//#define READ_INTERVAL 15*60*1000      //En milisegundos
#define READ_INTERVAL 30000             //10 s (En milisegundos)
#define PZEM_default_address 1
#define PZEM_default_alarm_level 230*5  //nivel de alarma por defecto= 1150 W
#define Token_Reset_energia 0xAA        //Los valores de Token_xx son arbitrarios
#define Token_Read_medidas 0x55
#define Token_Set_PZEM_address 0xF0
#define Token_Set_Alarm_level 0xFA

bool data_ready = false;
//float values[NUM_VALUES];
uint16_t values[NUM_VALUES];
uint32_t request_time;

float   V_, A_, W_, Wh_, Hz_, PF_;
double  W_PZEM_alarma;
bool    W_alarm = 0;
float   VA_, VAR_;
//float   V_Anterior, A_Anterior, Wh_Anterior, Hz_Anterior, PF_Anterior, W_PZEM_alarma_Anterior;
//float   V_delta = 1, A_delta = 0.1, Wh_delta = 1, Hz_delta = 0.1, PF_delta = 0.1, W_PZEM_alarma_delta = 1;
//byte    Variacion_medida = 0; //Variable que se activa por bit (segun orden de lectura en PZEM_loop())
//cuando alguna medida a variado el valor delta desde la lectura anterior

uint8_t Meter_Address = PZEM_default_address;       // 1 = Default ID/address MODBUS slave.
uint16_t W_Alarm_level = PZEM_default_alarm_level; 

// Create a ModbusRTU client instance
// The RS485 module has no halfduplex, so the second parameter with the DE/RE pin is required!
//ModbusClientRTU MB(Serial2, REDEPIN);
ModbusClientRTU MB(Serial2);

// Define an onData handler function to receive the regular responses
// Arguments are received response message and the request's token
/*
void handleData(ModbusMessage response, uint32_t token) 
{
  // First value is on pos 3, after server ID, function code and length byte
  uint16_t offs = 3;
  // The device has values all as IEEE754 float32 in two consecutive registers
  // Read the requested in a loop
  for (uint8_t i = 0; i < NUM_VALUES; ++i) {
    offs = response.get(offs, values[i]);
  }
  // Signal "data is complete"
  request_time = token;
  data_ready = true;
}
*/
void handleData(ModbusMessage response, uint32_t token)
{
  Serial.printf("Response: serverID=%d, FC=%02XHex, Token=%08XHex, length=%d:\n", response.getServerID(), response.getFunctionCode(), token, response.size());
  for(auto &byte : response)
  {
    Serial.printf("%02X ", byte);
  }
  Serial.println("");
  uint16_t Aux = 0;
  byte Aux1 = 0;
  uint32_t Aux32 = 0;
  if(token == Token_Read_medidas)
  {
    response.get(0, Aux1);
    if(response.getFunctionCode() == READ_INPUT_REGISTER && (Aux1 == Meter_Address))
    {
      response.get(3, Aux); V_ = Aux / 10.0f;
      response.get(17, Aux); Hz_ = Aux / 10.0f;
      response.get(19, Aux); PF_ = Aux / 100.0f;
      response.get(21, Aux); W_PZEM_alarma = Aux ; 
      if(W_PZEM_alarma==0xFFFF) { W_alarm = 1;}
      else { W_alarm = 0; }

      Aux32 = 0;
      response.get(7, Aux); Aux32 = Aux32 | Aux; Aux32 = Aux32 << 16;
      response.get(5, Aux); A_ = (Aux32 | Aux) / 1000.000f;

      Aux32 = 0;
      response.get(11, Aux); Aux32 = Aux32 | Aux; Aux32 = Aux32 << 16;
      response.get(9, Aux); W_ = (Aux32 | Aux) / 10.0f;

      Aux32 = 0;
      response.get(15, Aux); Aux32 = Aux32 | Aux; Aux32 = Aux32 << 16;
      response.get(13, Aux); Wh_ = (Aux32 | Aux) / 1000.0f; //Variable de energia en KWh

      if(W_ != 0) { VA_ = W_ / PF_; VAR_ = sqrtf(sq(VA_) - sq(W_)); }
      else { VA_ = 0; VAR_ = 0; }

      Imprimir_Medidas();
    }
    else
    {
      Serial.println("Error en comando lectura de medidas(READ_INPUT_REGISTER)");
    }

  }
  
  if(token == Token_Reset_energia)
  {
    response.get(0, Aux1);
    Serial.printf("Reset_energia ->Meter_Address: %02XHex\n", Aux1);
    if((response.getFunctionCode() == USER_DEFINED_42) && (Aux1 == Meter_Address) )
    {
      Serial.println("Reset energia ok");
    }
    else
    {
      Serial.println("Error en comando Reset energia(USER_DEFINED_42)");
    }
    Serial.printf("*************\n\n");
  }

  if(token == Token_Set_Alarm_level)
  {
    response.get(0, Aux1); //Direccion desde la que se envia la espuesta
    response.get(2, Aux);  //Numero registro Alarm_level= 0x0001
    Serial.printf("Registro Alarm_level: %04XHex num_reg\n", Aux); 
    if((response.getFunctionCode() == WRITE_HOLD_REGISTER) && (Aux1 == Meter_Address) && (Aux == 0x0001))
    {
      response.get(4, W_Alarm_level); //La respuesta confirma el nuevo valor de nivel de alarma
      Serial.printf("Set_Alarm_level ok: %5.0d W\n", W_Alarm_level);
    }
    else
    {
      Serial.println("Error en comando Set_Alarm_level(WRITE_HOLD_REGISTER)");
    }
    Serial.printf("+++++++++++++\n\n");
  }

  if(token == Token_Set_PZEM_address)
  {
    uint16_t Aux2 = 0; //dato para confirmar num_reg
    response.get(2, Aux2);
    Serial.printf("Registro PZEM_address: %04XHex num_reg\n", Aux2);
    response.get(4, Aux);
    Serial.printf("Valor nuevo PZEM_address: %04XHex \n", Aux);
    response.get(0, Aux1);    //Valor actual de la direccion
    if((response.getFunctionCode() == WRITE_HOLD_REGISTER) && (Aux2 == 0x0002) && (Aux1 == Meter_Address))
    {
      Meter_Address = Aux; //Confirmacion de la nueva direccion en la respuesta del PZEM
      Serial.println("Set_PZEM_address ok");
    }
    else
    {
      Serial.println("Error en comando Set_PZEM_address(WRITE_HOLD_REGISTER)");
    }
    Serial.printf("-----------------\n\n");
  }
}

// Define an onError handler function to receive error responses
// Arguments are the error code returned and a user-supplied token to identify the causing request
void handleError(Error error, uint32_t token) 
{
  // ModbusError wraps the error code and provides a readable error message for it
  ModbusError me(error);
  LOG_E("Error response: %02X - %s\n", (int)me, (const char *)me);
}

// Setup() - initialization happens here
void setup() {
// Init Serial monitor
  Serial.begin(115200);
  while (!Serial) {}
  Serial.println("Serial port __ OK __\n");

// Set up Serial2 connected to Modbus RTU
  Serial2.begin(BAUDRATE, SERIAL_8N1, RXPIN, TXPIN);

// Set up ModbusRTU client.
// - provide onData handler function
  MB.onDataHandler(&handleData);
// - provide onError handler function
  MB.onErrorHandler(&handleError);
// Set message timeout to 2000ms
  MB.setTimeout(2000);
// Start ModbusRTU background task
  MB.begin();

  /*
    2.5 Reset energy
    The command format of the master to reset the slave's energy is (total 4 bytes)
    Slave address + 0x42 + CRC check high byte + CRC check low byte.
    Correct reply: slave address + 0x42 + CRC check high byte + CRC check low byte.
    Error Reply: Slave address + 0xC2 + Abnormal code + CRC check high byte + CRC check low byte
  */
  Set_MB_Address(Meter_Address);
  resetEnergy(Meter_Address);
  Set_Power_Alarm_Level(W_Alarm_level);
}

// loop() - cyclically request the data
void loop() {
  static unsigned long next_request = millis();

  // Shall we do another request?
  if (millis() - next_request > READ_INTERVAL) 
  {
    // Yes.
    data_ready = false;
    // Issue the request
    uint16_t Token = Token_Read_medidas;
    Error err = MB.addRequest(Token, Meter_Address, READ_INPUT_REGISTER, FIRST_REGISTER, NUM_VALUES * 2);
    if (err!=SUCCESS) 
    {
      ModbusError e(err);
      LOG_E("Error creating request Token %02X: %02X - %s\n", Token, (int)e, (const char *)e);
    }
    // Save current time to check for next cycle
    next_request = millis();
  } 
  else 
  {
    // No, but we may have another response
    if (data_ready) {
      // We do. Print out the data
      Serial.printf("Requested at %8.3fs:\n", request_time / 1000.0);
      for (uint8_t i = 0; i < NUM_VALUES; ++i) {
        Serial.printf("   %04X: %8.3f\n", i * 2 + FIRST_REGISTER, values[i]);
      }
      Serial.printf("----------\n\n");
      data_ready = false;
    }
  }
}

void resetEnergy(uint16_t slaveAddr)
{
  //The command to reset the slave's energy is (total 4 bytes):
  //Slave address + 0x42 + CRC check high byte + CRC check low byte.
  uint16_t Token = Token_Reset_energia;
  //Error err = MB.addRequest(Token, Meter_Address, USER_DEFINED_42, 0, 2);
  Error err = MB.addRequest(Token, slaveAddr, USER_DEFINED_42);
  if(err != SUCCESS)
  {
    ModbusError e(err);
    LOG_E("Error creating request resetEnergy. Token %02X: %02X - %s\n", Token, (int)e, (const char *)e);
  }
}

void Set_Power_Alarm_Level(uint16_t power_level)
{
  uint16_t Token = Token_Set_Alarm_level;
  Error err = MB.addRequest(Token, Meter_Address, WRITE_HOLD_REGISTER, 1, power_level);
  //Error err = MB.syncRequest(Token, Meter_Address, WRITE_HOLD_REGISTER, 1, power_level);
  if(err != SUCCESS)
  {
    ModbusError e(err);
    LOG_E("Error creating request Set_Power_Alarm_Level. Token %02X: %02X - %s\n", Token, (int)e, (const char *)e);
  }
}

uint8_t Set_MB_Address(uint8_t MB_address)
{
  uint16_t Token = Token_Set_PZEM_address;
  Error err = MB.addRequest(Token, Meter_Address, WRITE_HOLD_REGISTER, 2, MB_address);
  if(err != SUCCESS)
  {
    ModbusError e(err);
    LOG_E("Error creating request Set_MB_Address %02X: %02X - %s\n", Token, (int)e, (const char *)e);
  }
  return MB_address;
}

void Imprimir_Medidas()
{
  //Serial.printf("Causas por la que se imprime: %#x\n", Variacion_medida);
  //print_binint(Variacion_medida);
  Serial.printf("Tension V_:      %3.2f V \n", V_);
  Serial.printf("Corriente A_:    %3.3f A \n", A_);
  //Serial.printf("A_max_dia A_:    %3.3f A H_A_max_alarma_dia= %5.0d \n", A_max_dia, H_A_max_alarma_dia);
  Serial.printf("Potencia W_:     %5.2f W \n", W_);
  //Serial.printf("W_max_dia W_:    %3.3f W H_W_max_alarma_dia= %5.0d \n", W_max_dia, H_W_max_alarma_dia);
  Serial.printf("Energía KWh_:     %4.3f KWh \n", Wh_);
  Serial.printf("Frecuencia Hz_:  %2.2f Hz \n", Hz_);
  Serial.printf("Factor Pot. PF_: %1.2f \n", PF_);
  Serial.printf("Pot. Aparente:   %5.2f VA \n", VA_);
  Serial.printf("Pot. Reactiva:   %5.2f VAR \n", VAR_);
  Serial.printf("Alarma potencia superada W_alarm (Alarma=1): %d \n", W_alarm);
  Serial.printf("Alarm_level: %5.0d W\n", W_Alarm_level);
  Serial.printf("Meter_Address: %d \n", Meter_Address);
  Serial.println("====================================================");
}
#define CHAR_BITS 8
void print_binint(int num)
{
  int sup = ( CHAR_BITS * sizeof(int) )-1;
  //Serial.printf("sup = CHAR_BITS * sizeof(int): %d \n", sup);
  while(sup >= 0)
  {
    if(num & (((long int)1) << sup))
      printf("1");
    else
      printf("0");
    sup--;
  }
  printf("\n");
}