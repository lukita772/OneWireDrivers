/*
 * oneWire.h
 *
 *  Created on: 13 feb. 2023
 *      Author: LAGOSTINI
 */

#ifndef ONEWIRE_ONEWIRE_H_
#define ONEWIRE_ONEWIRE_H_

#define ONE_WIRE_PORT GPIOA
#define ONE_WIRE_PIN GPIO_PIN_7//22


typedef enum{
	MASTER_RESET,
	MASTER_READ_ROM,
	MASTER_TX_FN_COMMAND,
	BUSY,
	READY,
	PENDING,
	FINISHED,
	READING,
	WRITING,
} oneWire_status;

typedef enum{
	SearchRom = 0xF0,
	ReadRom = 0x33,
	MatchRom = 0x55,
	SkipRom = 0xCC,
	AalarmSearch = 0xEC,
	ConvertT = 0x44,
	WriteScratchpad = 0x4E,
	ReadScratchpad = 0xBE,
	CopyScratchpad = 0x48,
	ReadPowerSupply = 0xB4
} oneWire_instructionSet;

typedef struct{
	uint8_t FAMILIY_CODE;
	uint8_t SERIAL_NUMBER[6];
	uint8_t CRC_BYTE;
} oneWire_ROM_DATA;

typedef enum{
	Status_ReadFamilyCode,
	Status_ReadSerialNumber,
	Status_ReadCRC
}oneWire_ReadRom_Status;

//non implemented functions
void InitializeOneWire();


//public functions
oneWire_status TriggerTemperatureConversion();
int8_t GetTemperature();
uint8_t GetFamilyCode();


#endif /* ONEWIRE_ONEWIRE_H_ */
