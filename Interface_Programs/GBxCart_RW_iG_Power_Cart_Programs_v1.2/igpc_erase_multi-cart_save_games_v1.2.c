/*
 GBxCart RW - insideGadgets Power Cart, Erase Multi-Game Cart save games
 Version: 1.2
 Author: Alex from insideGadgets (www.insidegadgets.com)
 Created: 1/05/2020
 Last Modified: 1/05/2021
 
 Erase Multi-game cart saves to your insideGadgets Gameboy 1MB Power Flash Cart. 
 
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

#include "setup.h" // See defines, variables, constants, functions here

int main(int argc, char **argv) {
	
	printf("GBxCart RW - insideGadgets Power Cart:\n");
	printf("Erase Multi-Game Saves v1.2 by insideGadgets\n");
	printf("############################################\n");
	
	read_config();
	
	// Open COM port
	if (com_test_port() == 0) {
		printf("Device not connected and couldn't be auto detected. Please make sure the COM port isn't open elsewhere. If you are using Linux/Mac, make sure you use \"sudo\".\n");
		read_one_letter();
		return 1;
	}
	printf("Connected on COM port: %i at ", cport_nr+1);
	
	// Break out of any existing functions on ATmega
	set_mode('0');
	RS232_flushRX(cport_nr);
	
	// Get cartridge mode - Gameboy or Gameboy Advance
	cartridgeMode = request_value(CART_MODE);
	
	// Get PCB version
	gbxcartPcbVersion = request_value(READ_PCB_VERSION);
	xmas_wake_up();
	
	// Increase to 1.7Mbit baud for v1.4 PCB
	if (gbxcartPcbVersion == PCB_1_4) {
		set_mode(USART_1_7M_SPEED);
		RS232_CloseComport(cport_nr);
		delay_ms(200);
		
		if (RS232_OpenComport(cport_nr, 1700000, "8N1") == 0) {
			printf("1.7M Baud\n");
		}
	}
	else {
		printf("1M Baud\n");
	}
	
	char pcbVersionString[20];
	if (gbxcartPcbVersion == PCB_1_0) {
		strncpy(pcbVersionString, "v1.0", 5);
	}
	else if (gbxcartPcbVersion == PCB_1_1) {
		strncpy(pcbVersionString, "v1.1/v1.2", 10);
	}
	else if (gbxcartPcbVersion == PCB_1_3) {
		strncpy(pcbVersionString, "v1.3", 5);
	}
	else if (gbxcartPcbVersion == PCB_1_4) {
		strncpy(pcbVersionString, "v1.4", 5);
	}
	else if (gbxcartPcbVersion == MINI) {
		strncpy(pcbVersionString, "Mini", 5);
	}
	else if (gbxcartPcbVersion == GBXMAS) {
		strncpy(pcbVersionString, "XMAS", 5);
	}
	
	// Get firmware version
	gbxcartFirmwareVersion = request_value(READ_FIRMWARE_VERSION);
	printf("PCB version: %s, Firmware version: %i\n", pcbVersionString, gbxcartFirmwareVersion);
	
	// PCB v1.3+ - Set 5V
	if (gbxcartPcbVersion == PCB_1_3 || gbxcartPcbVersion == PCB_1_4 || gbxcartPcbVersion == GBXMAS) {
		if (gbxcartPcbVersion == GBXMAS) {
			xmas_set_leds(0x6555955);
			delay_ms(50);
			set_mode('!');
			delay_ms(50); // Wait for ATmega169 to WDT reset if more than 2 mins idle
		}
		set_mode(VOLTAGE_5V);
		delay_ms(100);
	}
	
	// GBx v1.4 - Power up the cart if not already powered up and flush buffer
	gbx_cart_power_up();
	RS232_flushRX(cport_nr);
	
	if (gbxcartPcbVersion == PCB_1_0) {
		printf("\nPCB v1.0 is not supported for this function.");
		read_one_letter();
		return 1;
	}
	
	if (gbxcartFirmwareVersion <= 12) {
		printf("Firmware R13 or higher is required for this functionality.\n");
		read_one_letter();
		return 1;
	}
	
	
	// Power cart setup
	set_bank(0x6000, 0x00); // ROM bank 0, allow for all ROM access
	set_bank(0x2000, 0);
	set_bank(0x4000, 0x00); // RAM bank 0
	set_bank(0x0000, 0x00); // RAM not accessible
	
	
	// Read game ROM/RAM info
	uint16_t startAddr = 0;
	currAddr = 0x2000;
	endAddr = 0x2300;
	set_number(currAddr, SET_START_ADDRESS);
	set_mode(READ_ROM_RAM);
	
	uint8_t infoBuffer[0x400];
	while (currAddr < endAddr) {
		uint8_t comReadBytes = com_read_bytes(READ_BUFFER, 64);
		
		if (comReadBytes == 64) {
			memcpy(&infoBuffer[startAddr], readBuffer, 64);
			startAddr += 64;
			currAddr += 64;
			
			// Request 64 bytes more
			if (currAddr < endAddr) {
				com_read_cont();
			}
		}
		else { // Didn't receive 64 bytes, usually this only happens for Apple MACs
			com_read_stop();
			delay_ms(500);
			
			// Flush buffer
			RS232_PollComport(cport_nr, readBuffer, 64);											
			
			// Start off where we left off
			set_number(currAddr, SET_START_ADDRESS);
			set_mode(READ_ROM_RAM);				
		}
	}
	com_read_stop();
	
	// Set Audio pin as output
	if (gbxcartFirmwareVersion >= 22) {
		set_mode(AUDIO_HIGH);
		delay_ms(300);
	}
	else {
		RS232_cputs(cport_nr, "OE0x02");
		RS232_SendByte(cport_nr, 0);
		RS232_drain(cport_nr);
	}
	
	
	// Ask for game slot
	printf("\nPlease select a game slot to erase to or enter \"a\" for all slots:\n");
	
	// Query list of game titles
	for (uint8_t g = 1; g < 8; g++) {
		// Game present and has RAM
		if (infoBuffer[g] != 0xFF && infoBuffer[g+0x100] == 1) {
			// Audio - set low to reset multi-game
			if (gbxcartFirmwareVersion >= 22) {
				set_mode(AUDIO_LOW);
				delay_ms(300);
			}
			else {
				RS232_cputs(cport_nr, "LE0x02");
				RS232_SendByte(cport_nr, 0);
				RS232_drain(cport_nr);
				delay_ms(300);
			}
			
			// Audio - set high
			if (gbxcartFirmwareVersion >= 22) {
				set_mode(AUDIO_HIGH);
				delay_ms(300);
			}
			else {
				RS232_cputs(cport_nr, "HE0x02");
				RS232_SendByte(cport_nr, 0);
				RS232_drain(cport_nr);
				delay_ms(300);
			}
			
			// Power cart setup
			set_bank(0x6000, infoBuffer[g]); // ROM bank
			set_bank(0x2000, 0);
			set_bank(0x4000, infoBuffer[g+0x200]); // RAM bank + 8KB or 32KB locked
			set_bank(0x0000, 0x01); // RAM accessible
			
			printf("%d: ", g);
			
			// Check if game has the Nintendo logo
			if (check_for_valid_nintendo_logo()) {
				printf("%s", gameTitle);
			}
			printf("\n");
		}
	}
	
	// Audio - set low to reset multi-game
	if (gbxcartFirmwareVersion >= 22) {
		set_mode(AUDIO_LOW);
		delay_ms(300);
	}
	else {
		RS232_cputs(cport_nr, "LE0x02");
		RS232_SendByte(cport_nr, 0);
		RS232_drain(cport_nr);
		delay_ms(300);
	}
	
	// Audio - set high
	if (gbxcartFirmwareVersion >= 22) {
		set_mode(AUDIO_HIGH);
		delay_ms(300);
	}
	else {
		RS232_cputs(cport_nr, "HE0x02");
		RS232_SendByte(cport_nr, 0);
		RS232_drain(cport_nr);
		delay_ms(300);
	}
	
	// Set back to menu
	set_bank(0x6000, 0x00); // ROM bank 0, allow for all ROM access
	set_bank(0x2000, 0);
	set_bank(0x4000, 0x00); // RAM bank 0
	set_bank(0x0000, 0x00); // RAM not accessible
	
	printf(">");
	
	char optionString[5];
	fgets(optionString, 5, stdin);
	uint8_t optionSelected = atoi(optionString);
	
	// Audio - set low to reset multi-game
	if (gbxcartFirmwareVersion >= 22) {
		set_mode(AUDIO_LOW);
		delay_ms(300);
	}
	else {
		RS232_cputs(cport_nr, "LE0x02");
		RS232_SendByte(cport_nr, 0);
		RS232_drain(cport_nr);
		delay_ms(300);
	}
	
	// Audio - set high
	if (gbxcartFirmwareVersion >= 22) {
		set_mode(AUDIO_HIGH);
		delay_ms(300);
	}
	else {
		RS232_cputs(cport_nr, "HE0x02");
		RS232_SendByte(cport_nr, 0);
		RS232_drain(cport_nr);
		delay_ms(300);
	}
	
	
	// Power cart setup
	set_bank(0x6000, infoBuffer[optionSelected]); // ROM bank
	set_bank(0x2000, 0);
	set_bank(0x4000, infoBuffer[optionSelected+0x200]); // RAM bank + 8KB or 32KB locked
	set_bank(0x0000, 0x01); // RAM accessible
	
	// Erase all slots?
	if (strncmp(optionString, "a\0", 1) == 0) {
		printf("\n\n*** This will erase all the save games from your Gameboy Cartridge ***");
		printf("\nPress y to continue or any other key to abort.\n");
		
		char confirmWrite = read_one_letter();
		if (confirmWrite == 'y') {
			for (uint8_t g = 1; g < 8; g++) {
				// Game present and has RAM
				if (infoBuffer[g] != 0xFF && infoBuffer[g+0x100] == 1) {
					// Audio - set low to reset multi-game
					if (gbxcartFirmwareVersion >= 22) {
						set_mode(AUDIO_LOW);
						delay_ms(300);
					}
					else {
						RS232_cputs(cport_nr, "LE0x02");
						RS232_SendByte(cport_nr, 0);
						RS232_drain(cport_nr);
						delay_ms(300);
					}
					
					// Audio - set high
					if (gbxcartFirmwareVersion >= 22) {
						set_mode(AUDIO_HIGH);
						delay_ms(300);
					}
					else {
						RS232_cputs(cport_nr, "HE0x02");
						RS232_SendByte(cport_nr, 0);
						RS232_drain(cport_nr);
						delay_ms(300);
					}
					
					
					// Power cart setup
					set_bank(0x6000, infoBuffer[g]); // ROM bank
					set_bank(0x2000, 0);
					set_bank(0x4000, infoBuffer[g+0x200]); // RAM bank + 8KB or 32KB locked
					set_bank(0x0000, 0x01); // RAM accessible
					
					// Check if game has the Nintendo logo
					if (check_for_valid_nintendo_logo()) {
						
						printf("\nSelecting Game -\n");
						read_gb_header();
						
						// Does cartridge have RAM
						if (ramEndAddress > 0) {
							printf("\nErasing save\n");
							printf("[             25%%             50%%             75%%            100%%]\n[");
							
							mbc2_fix();
							if (cartridgeType <= 4) { // MBC1
								set_bank(0x6000, 1); // Set RAM Mode
							}
							set_bank(0x0000, 0x0A); // Initialise MBC
							
							if (ramEndAddress == 0xA1FF) {
								xmas_setup(ramEndAddress / 28);
							}
							else if (ramEndAddress == 0xA7FF) {
								xmas_setup(ramEndAddress / 4 / 28);
							}
							else {
								xmas_setup((ramBanks * (ramEndAddress - 0xA000 + 1)) / 28);
							}
							
							// Erase RAM
							uint32_t readBytes = 0;
							for (uint8_t bank = 0; bank < ramBanks; bank++) {
								uint16_t ramAddress = 0xA000;
								set_bank(0x4000, bank);
								set_number(0xA000, SET_START_ADDRESS); // Set start address again
								
								while (ramAddress < ramEndAddress) {
									com_write_bytes_from_file(WRITE_RAM, NULL, 64);
									ramAddress += 64;
									readBytes += 64;
									
									// Print progress
									if (ramEndAddress == 0xA1FF) {
										print_progress_percent(readBytes, 64);
										led_progress_percent(readBytes, 28);
									}
									else if (ramEndAddress == 0xA7FF) {
										print_progress_percent(readBytes / 4, 64);
										led_progress_percent(readBytes / 4, 28);
									}
									else {
										print_progress_percent(readBytes, (ramBanks * (ramEndAddress - 0xA000 + 1)) / 64);
										led_progress_percent(readBytes, (ramBanks * (ramEndAddress - 0xA000 + 1)) / 28);
									}
									
									com_wait_for_ack();
								}
							}
							printf("]");
							set_bank(0x0000, 0x00); // Disable RAM
							
							gbx_set_done_led();
							
							printf("\nFinished\n");
						}
						else {
							printf("\nCartridge has no RAM\n");
						}
						
					}
					printf("\n");
				}
			}
		}
	}
	else {
		printf("\nSelecting Game -\n");
		read_gb_header();
		
		// Does cartridge have RAM
		if (ramEndAddress > 0) {
			
			printf("\n\n*** This will erase the save game from your Gameboy Cartridge ***");
			printf("\nPress y to continue or any other key to abort.\n");
			
			char confirmWrite = read_one_letter();
			if (confirmWrite == 'y') {
				printf("\nErasing save\n");
				printf("[             25%%             50%%             75%%            100%%]\n[");
				
				mbc2_fix();
				if (cartridgeType <= 4) { // MBC1
					set_bank(0x6000, 1); // Set RAM Mode
				}
				set_bank(0x0000, 0x0A); // Initialise MBC
				
				if (ramEndAddress == 0xA1FF) {
					xmas_setup(ramEndAddress / 28);
				}
				else if (ramEndAddress == 0xA7FF) {
					xmas_setup(ramEndAddress / 4 / 28);
				}
				else {
					xmas_setup((ramBanks * (ramEndAddress - 0xA000 + 1)) / 28);
				}
				
				// Erase RAM
				uint32_t readBytes = 0;
				for (uint8_t bank = 0; bank < ramBanks; bank++) {
					uint16_t ramAddress = 0xA000;
					set_bank(0x4000, bank);
					set_number(0xA000, SET_START_ADDRESS); // Set start address again
					
					while (ramAddress < ramEndAddress) {
						com_write_bytes_from_file(WRITE_RAM, NULL, 64);
						ramAddress += 64;
						readBytes += 64;
						
						// Print progress
						if (ramEndAddress == 0xA1FF) {
							print_progress_percent(readBytes, 64);
							led_progress_percent(readBytes, 28);
						}
						else if (ramEndAddress == 0xA7FF) {
							print_progress_percent(readBytes / 4, 64);
							led_progress_percent(readBytes / 4, 28);
						}
						else {
							print_progress_percent(readBytes, (ramBanks * (ramEndAddress - 0xA000 + 1)) / 64);
							led_progress_percent(readBytes, (ramBanks * (ramEndAddress - 0xA000 + 1)) / 28);
						}
						
						com_wait_for_ack();
					}
				}
				printf("]");
				set_bank(0x0000, 0x00); // Disable RAM
				
				gbx_set_done_led();
				
				printf("\nFinished\n");
			}
			else {
				printf("\nAborted\n");
			}
		}
		else {
			printf("\nCartridge has no RAM\n");
		}
	}
	
	// Audio - set low to reset multi-game
	if (gbxcartFirmwareVersion >= 22) {
		set_mode(AUDIO_LOW);
		delay_ms(300);
	}
	else {
		RS232_cputs(cport_nr, "LE0x02");
		RS232_SendByte(cport_nr, 0);
		RS232_drain(cport_nr);
		delay_ms(300);
	}
	
	// Audio - set high
	if (gbxcartFirmwareVersion >= 22) {
		set_mode(AUDIO_HIGH);
		delay_ms(300);
	}
	else {
		RS232_cputs(cport_nr, "HE0x02");
		RS232_SendByte(cport_nr, 0);
		RS232_drain(cport_nr);
		delay_ms(300);
	}
	
	// Set back to menu
	set_bank(0x6000, 0x00); // ROM bank 0, allow for all ROM access
	set_bank(0x2000, 0);
	set_bank(0x4000, 0x00); // RAM bank 0
	set_bank(0x0000, 0x00); // RAM not accessible
	
	gbx_cart_power_down();
	read_one_letter();
	
	return 0;
}