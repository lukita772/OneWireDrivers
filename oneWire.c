/*
 * oneWire.c
 *
 *  Created on: 13 feb. 2023
 *      Author: LAGOSTINI
 *      brief: Protocolo OneWire
 */
#include <string.h>
#include "main.h"
#include "oneWire.h"

//private prototypes
static void resetPulse();
static uint8_t readBit();
static void writeZero();
static void writeOne();
static uint8_t writeInstruction(oneWire_instructionSet instruction);
static uint8_t readInstruction(oneWire_instructionSet instruction);
static uint8_t readFromDevice(oneWire_instructionSet instruction);
static uint8_t readRisingEdge();

// Variables globales
static int16_t Temperature = 0;
static oneWire_ROM_DATA oneWire_ROM = {0,0,0};

// Flags
volatile uint8_t NextTimeSlot_flag = 0; //cada 160us cambia
volatile uint8_t NextTimeSlotSuspend_flag = 0;
volatile oneWire_status OneWireStatus_flag = READY;



void InitializeOneWire()
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(ONE_WIRE_PORT, ONE_WIRE_PIN, GPIO_PIN_RESET);

	GPIO_InitStruct.Pin = ONE_WIRE_PIN;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;//GPIO_MODE_OUTPUT_OD;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(ONE_WIRE_PORT, &GPIO_InitStruct);

	//todo: generar flag para habilitacion de imp. en timer4.

}

// brief: Public functions

uint8_t GetFamilyCode()
{
	return oneWire_ROM.FAMILIY_CODE;
}

int8_t GetTemperature()
{
	return Temperature;
}

/*
 * function: TriggerTemperatureConversion()
 * type: public
 * brief: Maq. de estados responsable de pedir temp.
 * Author: LAGOSTINI
 */
oneWire_status TriggerTemperatureConversion()
{
	static uint8_t currentState = MASTER_RESET;
	static uint8_t tempConverted = 0;
	static uint8_t reseted = 0;

	switch(currentState)
	{
	case MASTER_RESET:
		if(!reseted)
		{
			resetPulse(); //envio un pulso a gnd de 500us y libero el wire
			reseted = 1;
		}
		if(readRisingEdge() == FINISHED) //espero un cero de parte del ds
		{
			currentState = MASTER_READ_ROM;
		}
		break;
	case MASTER_READ_ROM:
		if(readFromDevice(ReadRom) == FINISHED)
		{
			currentState = MASTER_TX_FN_COMMAND;
		}
		break;
	case MASTER_TX_FN_COMMAND:
		if(tempConverted == 0)
		{
			if(readFromDevice(ConvertT) == FINISHED) //disparo conversion de temp
			{
				tempConverted = 1;
				reseted = 0;
				currentState = MASTER_RESET;
				return PENDING;
			}
		}
		else
		{
			if(readFromDevice(ReadScratchpad) == FINISHED)
			{
				currentState = MASTER_RESET;
				Temperature >>= 4;
				tempConverted = 0;
				reseted = 0;
				return FINISHED;
			}
		}
		break;
	default:
		return ERROR;
		break;
	}

	return PENDING;
}

static uint8_t readFromDevice(oneWire_instructionSet instruction)
{
	static uint8_t state = WRITING;

	switch(state)
	{
		case WRITING:
			if(writeInstruction(instruction) == FINISHED) //envio instruccion para leer rom/scratchpad
			{
				state = READING;
			}
			break;
		case READING:
			if(instruction == ConvertT) //si la instruccion es ConvertT, solo espero un '1' en rta del device
			{
				if(readBit())
				{
					state = WRITING;
					return FINISHED;
				}
			}
			else if(readInstruction(instruction) == FINISHED) //recibo datos de rom o scratchpad
			{
				state = WRITING;
				return FINISHED;
			}
			break;
	}

	return PENDING;
}


/*
 * function: readInstruction
 * brief: Private function, avoid to use outside this function
 * Author: LAGOSTINI
 */
static uint8_t readInstruction(oneWire_instructionSet instruction)
{
	static uint8_t currBit = 0;
	static uint8_t i = 0;
	static oneWire_ReadRom_Status readStatus = Status_ReadFamilyCode;

	//if(!currBit){
	//	writeOne();
	//}

	if(instruction == ReadRom)
	{
		switch(readStatus)
		{
			case Status_ReadFamilyCode:
				oneWire_ROM.FAMILIY_CODE |=  (readBit()  << currBit);

				currBit++;
				if(currBit >= 8)
				{
					currBit = 0;
					readStatus = Status_ReadSerialNumber;
				}
				break;
			case Status_ReadSerialNumber:
				oneWire_ROM.SERIAL_NUMBER[i] |= (readBit()  << (currBit-(i*8)));

				currBit++;

				if(currBit%8 == 0){
					i++;
				}

				if(currBit >= 48)
				{
					currBit = 0;
					i = 0;
					readStatus = Status_ReadCRC;
				}
				break;
			case Status_ReadCRC:
				oneWire_ROM.CRC_BYTE |= (readBit()  << currBit);
				currBit++;
				if(currBit >= 8)
				{
					currBit = 0;
					readStatus = Status_ReadFamilyCode;
					return FINISHED;
				}
				break;
		}
	}
	else if(instruction == ReadScratchpad)
	{
		Temperature |=  (readBit()  << currBit);
		currBit++;
		if(currBit >= 15)
		{
			currBit = 0;
			resetPulse();
			return FINISHED;
		}

	}

	return PENDING;
}


/*
 * function: write instructions
 * brief: Private function, avoid to use outside this function
 * Author: LAGOSTINI
 */
static uint8_t writeInstruction(oneWire_instructionSet instruction)
{
	static uint8_t mask = 0b00000001;

	if(mask > 0)
	{
		if(instruction & mask){
			writeOne();
		}
		else{
			writeZero();
		}
		mask <<=1;
	}
	else
	{
		mask = 0b00000001;
		return FINISHED;
	}

	return PENDING;
}

static uint8_t readRisingEdge()
{
	static uint8_t currSt = 0;
	static uint8_t bitCounter= 0;

	switch (currSt)
	{
	case 0:
		if(!readBit()){ //si leo un 0
			bitCounter ++;
		}
		else if(bitCounter > 0) //si leo un 1 y previamente hubo al menos un 0
		{
			currSt = 1;
		}
		if(bitCounter > 5){ //si lei cinco 0s (pasaron 5 time slots en 0, es decir, mas de 500us, posible error.
			bitCounter = 0;
			currSt = 2;
		}
		break;
	case 1:
		currSt = 0;
		bitCounter = 0;
		return FINISHED;
		break;
	case 2:
		currSt = 0;
		bitCounter = 0;
		return ERROR; //error
		break;

	}

	return PENDING;
}

/*
 * function: write functions
 * brief: Write a '0' after the next time slot
 * Author: LAGOSTINI
 */
static void writeZero()
{
	uint16_t counter = 0;

	while(!NextTimeSlot_flag);

	//HAL_GPIO_WritePin(ONE_WIRE_PORT, ONE_WIRE_PIN, GPIO_PIN_RESET);
	for(counter = 0 ; counter < 400; counter ++);
	HAL_GPIO_WritePin(ONE_WIRE_PORT, ONE_WIRE_PIN, GPIO_PIN_SET);
	NextTimeSlot_flag = 0;

}


/*
 * function: write functions
 * brief: Write a '1' after the next time slot
 * Author: LAGOSTINI
 */
static void writeOne()
{
	uint16_t counter = 0;

	while(!NextTimeSlot_flag);

	//HAL_GPIO_WritePin(ONE_WIRE_PORT, ONE_WIRE_PIN, GPIO_PIN_RESET);
	for(counter = 0 ; counter < 25; counter ++);
	HAL_GPIO_WritePin(ONE_WIRE_PORT, ONE_WIRE_PIN, GPIO_PIN_SET);
	NextTimeSlot_flag = 0;

}

/*
 * function: write functions
 * brief: Do a 480-500us pulse after the next time slot (time slot system disabled in this period)
 * Author: LAGOSTINI
 */
static void resetPulse()
{
	uint32_t counter = 0;
	while(!NextTimeSlot_flag);

	NextTimeSlotSuspend_flag = 1;

	HAL_GPIO_WritePin(ONE_WIRE_PORT, ONE_WIRE_PIN, GPIO_PIN_RESET);
	for(counter = 0 ; counter < 2800; counter ++); //500us
	HAL_GPIO_WritePin(ONE_WIRE_PORT, ONE_WIRE_PIN, GPIO_PIN_SET);

	NextTimeSlotSuspend_flag = 0;
	NextTimeSlot_flag = 0;

}

/*
 * function: write functions
 * brief: Wait for the next time slot, wait a few microsecunds, free the wire and read the pin
 * Author: LAGOSTINI
 */
static uint8_t readBit()
{
	uint16_t bit = 0;
	uint16_t counter = 0;

	while(!NextTimeSlot_flag);

	//HAL_GPIO_WritePin(ONE_WIRE_PORT, ONE_WIRE_PIN, GPIO_PIN_RESET); //generamos un read slot
	for(counter = 0 ; counter < 20; counter ++);
	HAL_GPIO_WritePin(ONE_WIRE_PORT, ONE_WIRE_PIN, GPIO_PIN_SET); //liberamos el wire
	for(counter = 0 ; counter < 100; counter ++);
	bit =  HAL_GPIO_ReadPin(ONE_WIRE_PORT,ONE_WIRE_PIN); //leemos

	NextTimeSlot_flag = 0;

	return bit;
}




