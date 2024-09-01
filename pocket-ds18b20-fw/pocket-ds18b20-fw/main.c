#include <avr/io.h>
#include <util/delay.h>
#include <stdbool.h>
#include "ds18b20.h"

#define CS 3
#define MOSI 2
#define SCK 1

void set(uint8_t pin, uint8_t val) {
	if (val == 0) {
		PORTA &= ~(1 << pin);
	}
	else {
		PORTA |= (1 << pin);
	}
}

void shiftOut(uint8_t data) {
	for(uint8_t i = 0; i < 8; i++) {
		int bit = data & (1 << (7-i));
		set(MOSI, bit);

		_delay_us(1);
		set(SCK, 1);
		_delay_us(1);
		set(SCK, 0);
	}
}

void setMAXRegister(uint8_t reg, uint8_t value) {
	set(CS, 0);
	_delay_us(1);
	shiftOut(reg);
	shiftOut(value);
	_delay_us(1);
	set(CS, 1);
	_delay_us(1);
}

void setMAXDigit(uint8_t digit, uint8_t val, bool dot) {
	uint8_t lookup[] = {0, 2, 3, 1};
	uint8_t dig = lookup[digit];
	if (dot == true) {
		val |= (1 << 7);
	}
	setMAXRegister(dig+1, val);
}

/*
void setMAXInteger(uint16_t integer) {
	integer %= 10000; // Trim extra digits that can't be displayed


	setMAXDigit(3, integer % 10, false);
	integer /= 10;
	setMAXDigit(2, integer % 10, false);
	integer /= 10;
	setMAXDigit(1, integer % 10, false);
	integer /= 10;
	setMAXDigit(0, integer, false);
}
*/

void showVccVoltage(uint16_t adcval) {
	// Bandgap voltage: (actualVcc * adcReading) / 1024
	// Vcc: (bandgapVoltage * 1024) / adcReading
	
	uint16_t num = 1116860 / adcval; // Multiplied by 100 to not have to deal with floats
	
	setMAXDigit(0, num / 1000, true);
	num = num % 1000;
	setMAXDigit(1, num / 100, false);
	num = num % 100;
	setMAXDigit(2, num / 10, false);
	setMAXDigit(3, num % 10, false);
	
}

void setMAXfromDS(int16_t dstemp) {
	if(dstemp >= 0) {
		uint16_t temp_integer = dstemp / 16;
		if(dstemp < 100*16) { // Under 100C
			uint16_t temp_fraction = ((dstemp % 16) * 100) / 16;

			setMAXDigit(0, temp_integer / 10, false);
			setMAXDigit(1, temp_integer % 10, true);
			setMAXDigit(2, temp_fraction / 10, false);
			setMAXDigit(3, temp_fraction % 10, false);
		}
		else { // Over 100C
			uint16_t temp_fraction = ((dstemp % 16) * 10) / 16;
				
			setMAXDigit(0, temp_integer / 100, false);
			temp_integer = temp_integer % 100;
			setMAXDigit(1, temp_integer / 10, false);
			setMAXDigit(2, temp_integer % 10, true);
			setMAXDigit(3, temp_fraction, false);
		}
	}
	else {
		//TODO - negative temp handling
	}
}

int main(void) {
	_delay_ms(500);
	DDRA |= (1 << CS);
	DDRA |= (1 << SCK);
	DDRA |= (1 << MOSI);
	
	set(CS, 1);
	set(SCK, 0);
	set(MOSI, 0);
	
	ADMUX = 0b00100001; // Vcc as reference, measuring 1.1V internal bandgap
	ADCSRA = 0b10000100; // Enable ADC, prescaler 16
	_delay_ms(1);

	setMAXRegister(0x0C, 1);	// Shutdown -> Normal mode
	setMAXRegister(0x0F, 0);	// Display Test -> Normal mode
	setMAXRegister(0x09, 0x0F);	// Decode Mode -> Code B decode for digits 0-3
	setMAXRegister(0x0A, 0);	// Intensity -> 0
	setMAXRegister(0x0B, 3);	// Scan Limit -> 0,1,2,3
	_delay_ms(200);

	ds18b20wsp(&PORTB, &DDRB, &PINB, ( 1 << 2 ), 0, 0, 125, DS18B20_RES11);
	
	for(uint8_t j = 0; j < 30; j++) {
		uint16_t adcsum = 0;
		for(uint8_t i = 0; i < 100; i++) {
			ADCSRA |= (1 << ADSC); // Start conversion
			while(!(ADCSRA & (1 << ADIF))); // Wait for conversion to complete
			ADCSRA |= (1 << ADIF); // Clear the ADC interrupt flag
			volatile uint16_t adcl = ADCL; // Read lower byte
			adcsum += (((uint16_t)ADCH) << 8) | adcl; // Read upper byte and merge into uint16_t
			_delay_ms(1);
		}
		showVccVoltage(adcsum / 100);
	}
	
	int16_t temp = 0;
	while (1) {
		ds18b20convert(&PORTB, &DDRB, &PINB, ( 1 << 2 ), 0);
		_delay_ms(10);
		ds18b20read(&PORTB, &DDRB, &PINB, ( 1 << 2 ), 0, &temp);
		setMAXfromDS(temp);
	}
	
	
	return 0;
}