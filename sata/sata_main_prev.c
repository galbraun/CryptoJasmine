// Copyright 2011 INDILINX Co., Ltd.
//
// This file is part of Jasmine.
//
// Jasmine is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Jasmine is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Jasmine. See the file COPYING.
// If not, see <http://www.gnu.org/licenses/>.


#include "jasmine.h"

sata_context_t		g_sata_context;
sata_ncq_t			g_sata_ncq;
volatile UINT32		g_sata_action_flags;

#define HW_EQ_SIZE		128
#define HW_EQ_MARGIN	4


/****************** GAL AND SHANI PROJECT ******************/
char messageToUser[128] = {0};
int  commandCounter = 0;

ESystemState			currSystemState = SYS_INITIAL_STATE;
EAuthenticationState	currAuthenticationState = AUTH_INITIAL_STATE; // after an authentication process began - sign in which step in the process 
EMessageToUserState		currMessageState = MESSAGE_INVALID_STATE;

#define get_num_bank(lpn)             ((lpn) % NUM_BANKS)
#define get_lpn(bank, page_num)       (g_misc_meta[bank].lpn_list_of_cur_vblock[page_num])
#define CHECK_LPAGE(lpn)              ASSERT((lpn) < NUM_LPAGES)






void authentication(char* message, int lba, int sectCount) {
	 
	uint8_t iv[]		= {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};
	uint8_t buffer[64]	= {0};
	uint8_t result[64]	= {0};
	uint8_t input[64]	= {0};
	 
	char* password = "testtest"; // TODO: this is for test - need to pull from passwordfile by userName
	char* userName = NULL;
	char* userData = NULL;
	char* hashedPass_server = NULL;
	static uint32_t serverChallengeAck = 0;
	static uint32_t serverChallenge	= 0;
	static uint32_t userChallenge	= 0;
	static uint64_t server_public	= 0;
	static uint64_t server_random	= 0;
	int i = 0;
	 
	UINT32 sect_offset = lba % SECTORS_PER_PAGE;
	UINT32 next_read_buf_id;
	 
	switch (currAuthenticationState) {
		case AUTH_GET_USER_KEY: // server side: find user password decrypt it from the password file 
			userName = message;
			userData = strchr(message,'|') + 1;
				
			*(strchr(message,'|')) = 0; //null terminated string
				
			strncpy((char*)buffer, userData, strlen(userData)); // TODO: is userData is null terminated? is message is null terminated?
				
			uart_printf("user name is: %s", userName);
				
			hashedPass_server = md5Hash(password, strlen(password));
			uart_printf("hashedPass_server: %s\n", hashedPass_server);

			server_random = randomint64();
			server_public = powmodp(G, server_random);

			AES128_CBC_decrypt_buffer(result + 0,  buffer+0,  16, (uint8_t*)hashedPass_server, iv);
			AES128_CBC_decrypt_buffer(result + 16, buffer+16, 16, 0, 0);
			AES128_CBC_decrypt_buffer(result + 32, buffer+32, 16, 0, 0);
			AES128_CBC_decrypt_buffer(result + 48, buffer+48, 16, 0, 0);
				
			uint64_t decryptedKey = 0;
			memcpy(&decryptedKey, result, sizeof(uint64_t));
				
			uint64_t server_symetricKey = powmodp(decryptedKey, server_random);
			serverChallenge = randomint32();
			
			//uart_printf("decryptedKey: %lld\n", decryptedKey);
			//uart_printf("server_symetricKey: %lld\n", server_symetricKey);
			//uart_printf("serverChallenge: %ld\n", serverChallenge);
			
			AES128_CBC_encrypt_buffer((uint8_t*)messageToUser, &serverChallenge, sizeof(serverChallenge), &server_symetricKey, iv);
			AES128_CBC_encrypt_buffer(messageToUser+64, &server_public, sizeof(server_public), &hashedPass_server, iv);
			currMessageState = MESSAGE_READY;
				
				
			//uart_printf("messageToUser: %s\n", messageToUser);
			// sends to user the server challenge encrypted with server_symetricKey and server_public encrypted with hashedPass_server
			free(hashedPass_server); //allocated by md5Hash 

			break;

		case AUTH_GET_USER_CHALLENGE:	
			serverChallengeAck = message;
			userChallenge = strchr(message,'|') + 1; // TODO: get 64 bit, split to 32 msb's user challenge and 32lsb's server challenge ack
			
			*(strchr(message,'|')) = 0;
			
			strncpy(buffer, serverChallengeAck,strlen(serverChallengeAck));
			
			AES128_CBC_decrypt_buffer(result + 0,  buffer+0,  16, server_symetricKey, iv);
			AES128_CBC_decrypt_buffer(result + 16, buffer+16, 16, 0, 0);
			AES128_CBC_decrypt_buffer(result + 32, buffer+32, 16, 0, 0);
			AES128_CBC_decrypt_buffer(result + 48, buffer+48, 16, 0, 0);
			
			// TODO: do manipulation on challenge ( xor with symetric key?)
			uart_printf("result = %s", result);
			
			if ( result != serverChallenge ){
				//notAuthenticated(lba);
				uart_printf("challenges not equal...");
				//break; <-----------------------------------------------------
				
			}
			
			strncpy(buffer, userChallenge, strlen(userChallenge));
			
			AES128_CBC_decrypt_buffer(result + 0,  buffer+0,  16, server_symetricKey, iv);
			AES128_CBC_decrypt_buffer(result + 16, buffer+16, 16, 0, 0);
			AES128_CBC_decrypt_buffer(result + 32, buffer+32, 16, 0, 0);
			AES128_CBC_decrypt_buffer(result + 48, buffer+48, 16, 0, 0);
			
			// TODO: do some manipulation of userChallenge (xor with symetric key? ) and send it to user

			//AES128_CBC_encrypt_buffer(messageToUser, &serverChallenge, sizeof(serverChallenge), &server_symetricKey, iv); // TODO: encrypt user manipulated challenge ack <-----------------------------------------------------
			strncpy(messageToUser, userChallenge, (strlen(userChallenge) < sizeof(messageToUser) ? strlen(userChallenge) : sizeof(messageToUser)));
			currMessageState = MESSAGE_READY;
			uart_printf("msg is ready!");
			break;
			
		case AUTH_SEND_TO_USER:
			next_read_buf_id = (g_ftl_read_buf_id + 1) % NUM_RD_BUFFERS;
				
			while (next_read_buf_id == GETREG(SATA_RBUF_PTR));
				
			if(currMessageState != MESSAGE_READY){
				uart_printf("msg is not ready...");
				strcpy(messageToUser, "NOT AUTHENTICATED");
			}
			
			for (int k = 0; k < strlen(messageToUser); k++){
				write_dram_8(RD_BUF_PTR(g_ftl_read_buf_id) + (36*BYTES_PER_SECTOR) + k, messageToUser[k]);
			}

			SETREG(SATA_RBUF_PTR, g_ftl_read_buf_id);	// change sata_read_ptr
			SETREG(BM_STACK_RDSET, next_read_buf_id);	// change bm_read_limit
			SETREG(BM_STACK_RESET, 0x02);				// change bm_read_limit
				
			g_ftl_read_buf_id = next_read_buf_id;
			
			currMessageState = MESSAGE_INVALID_STATE;
			
			break;
			
		default:
			uart_printf("default? problem!");
			break;
	}	
}

static int autherization(char* user, char* password,UINT32 bank){
	int status = 0;
/*
	if (user == NULL || password == NULL){
		return 0;
	}
	
	FILE* fd = fopen(PASSWORD_FILE,"r"); // not realy work - need to choose place in hdd
	if (!fd){
		//also check here if place in hdd not valid 
		return 0;
	}
	do {
		
	} while (status != EOF);
	
	fclose(fd);
*/
	return status;
} 

void notAuthenticated(int lba){ // TODO: every user action will be write-read, so this function should only change the messageToUser content
	uart_printf("not authenticated...problem!");
	UINT32 sect_offset  = lba % SECTORS_PER_PAGE;
	strcpy(messageToUser,"NOT AUTHENTICATED!");
	
	UINT32 next_read_buf_id = (g_ftl_read_buf_id + 1) % NUM_RD_BUFFERS;
	
	while (next_read_buf_id == GETREG(SATA_RBUF_PTR));
	
	for ( int k=0 ; k< strlen(messageToUser); k++){
		write_dram_8(RD_BUF_PTR(g_ftl_read_buf_id)+(36*BYTES_PER_SECTOR)+k,messageToUser[k]);
	}
				
	SETREG(SATA_RBUF_PTR, g_ftl_read_buf_id);	// change sata_read_ptr
	SETREG(BM_STACK_RDSET, next_read_buf_id);	// change bm_read_limit
	SETREG(BM_STACK_RESET, 0x02);				// change bm_read_limit
	/*
	UINT32 status = (B_DRDY|BIT4|B_ERR);
	UINT32 err_code = B_AUTH;
	UINT32 fis_type = FISTYPE_REGISTER_D2H;
	UINT32 flags = B_IRQ;
	SETREG(SATA_FIS_D2H_0, fis_type | (flags << 8) | (status << 16) | (err_code << 24));
	SETREG(SATA_FIS_D2H_1, GETREG(SATA_FIS_H2D_1));
	SETREG(SATA_FIS_D2H_2, GETREG(SATA_FIS_H2D_2) & 0x00FFFFFF);
	SETREG(SATA_FIS_D2H_3, GETREG(SATA_FIS_H2D_3) & 0x0000FFFF);
	SETREG(SATA_FIS_D2H_4, 0);
	SETREG(SATA_FIS_D2H_LEN, 5);
	SETREG(SATA_INT_STAT,OPERATION_ERR);
	SETREG(APB_INT_STS, INTR_SATA);
	SETREG(SATA_CTRL_2,PIO_READ | COMPLETE);
	*/
	
	g_ftl_read_buf_id = next_read_buf_id;
	
	uart_printf("%d %d %d %d %d\n",GETREG(SATA_PHY_STATUS),GETREG(SATA_FIS_D2H_0),GETREG(SATA_FIS_D2H_1),GETREG(SATA_FIS_D2H_2),GETREG(SATA_FIS_D2H_3),GETREG(SATA_FIS_D2H_4));
}
/****************** GAL AND SHANI PROJECT ******************/
/****************** END OF CURRENT CHANGE ******************/




static UINT32 eventq_get_count(void)
{
	return (GETREG(SATA_EQ_STATUS) >> 16) & 0xFF;
}

static void eventq_get(CMD_T* cmd)
{
	disable_fiq();

	SETREG(SATA_EQ_CTRL, 1);	// The next entry from the Event Queue is copied to SATA_EQ_DATA_0 through SATA_EQ_DATA_2.

	while ((GETREG(SATA_EQ_DATA_2) & 8) != 0);

	UINT32 EQReadData0	= GETREG(SATA_EQ_DATA_0);
	UINT32 EQReadData1	= GETREG(SATA_EQ_DATA_1);

	cmd->lba			= EQReadData1 & 0x3FFFFFFF;
	cmd->sector_count	= EQReadData0 >> 16;
	cmd->cmd_type		= EQReadData1 >> 31;

	if(cmd->sector_count == 0)
		cmd->sector_count = 0x10000;

	if (g_sata_context.eq_full)
	{
		g_sata_context.eq_full = FALSE;

		if ((GETREG(SATA_PHY_STATUS) & 0xF0F) == 0x103)
		{
			SETREG(SATA_CTRL_2, g_sata_action_flags);
		}
	}

	enable_fiq();
}

__inline ATA_FUNCTION_T search_ata_function(UINT32 command_code)
{
	UINT32 index;
	ATA_FUNCTION_T ata_function;

	index = mem_search_equ(ata_command_code_table, sizeof(UINT8), CMD_TABLE_SIZE, MU_CMD_SEARCH_EQU_SRAM, command_code);

	ata_function = (index == CMD_TABLE_SIZE) ? ata_not_supported : ata_function_table[index];

	if (ata_function == (ATA_FUNCTION_T) INVALID32)
		ata_function = ata_not_supported;

	return ata_function;
}


void Main(void)
{	
	uart_printf("MAIN STARTED"); /*** GAL AND SHANI PROJECT ***/
	
	while (1)
	{
		if (eventq_get_count())
		{	
			CMD_T cmd;
			eventq_get(&cmd);
			
			/*** GAL AND SHANI PROJECT ***/
			
			// systemAuthenticationState is in SYS_INITIAL_STATE so the basic format actions won't be block
			// after 5000 commands, changed to SYS_NOT_AUTHENTICATED
			// TODO: change it to timer with 30 secs timout? 
			if (commandCounter < 5000) {
				//uart_printf("commandCounter = %d, commandType = %s, cmd.lba = %d", commandCounter, (cmd.cmd_type == READ ? "READ" : "WRITE"), cmd.lba);
				commandCounter++;	
			} 
			else {
				currSystemState = SYS_NOT_AUTHENTICATED;
			}
			
			//if(currSystemState == SYS_INITIAL_STATE && ((cmd.cmd_type == WRITE && cmd.lba == 100) || (cmd.cmd_type == READ && cmd.lba == 96))){
			//	uart_printf("changing currSystemState!");
			//	currSystemState = SYS_NOT_AUTHENTICATED;
			//}
				
			//if(currSystemState == SYS_INITIAL_STATE){
				
			//	if (cmd.cmd_type == READ) {
			//		ftl_read(cmd.lba, cmd.sector_count);
			//	}
			//	else {
			//		ftl_write(cmd.lba, cmd.sector_count);
			//	}
			//}
			//else { 
				if(currSystemState == SYS_AUTHENTICATED){
					if (cmd.cmd_type == READ) {
						// TODO: check autherization
						ftl_read(cmd.lba, cmd.sector_count);
					}
					else {
						// TODO: check autherization
						ftl_write(cmd.lba, cmd.sector_count);
					}
				}
				else if (cmd.cmd_type == WRITE && cmd.lba == 100) { 
				//currSystemState == SYS_NOT_AUTHENTICATED, action is part of authentication process: writing data to Jasmine (send user response / user authentication data)
					uart_printf("part of auth process: write data to J");
					UINT32 sect_offset  = cmd.lba % SECTORS_PER_PAGE;
					char message[BYTES_PER_SECTOR] = {0};
					int i = 0;
					for (i = 0; i < BYTES_PER_SECTOR; i++) {
						char ch = (char) read_dram_8(WR_BUF_PTR(g_ftl_write_buf_id) + (sect_offset*BYTES_PER_SECTOR) + i);
						if (ch == 0) {
							break;
						}
						message[i] = ch;
					}
					message[i+1] = '\0';		

					uart_printf("message from user is: %s", message);
					
					switch (currAuthenticationState){
					//	currAuthenticationState = AUTH_GET_USER_KEY;
					//	authentication(message, cmd.lba, cmd.sector_count);
					//	break;
					//case AUTH_GET_USER_CHALLENGE: //message is user challenge (and response to server challenge)
					case AUTH_SEND_TO_USER: //message is user challenge (and response to server challenge)
						currAuthenticationState = AUTH_GET_USER_CHALLENGE;
						uart_printf("AUTH_GET_USER_CHALLENGE");
						authentication(message, cmd.lba, cmd.sector_count);
						break;
					
					case AUTH_INITIAL_STATE: //message is user authentication data
					default: // assume its the first message
						uart_printf("default. AUTH_INITIAL_STATE");
						currAuthenticationState = AUTH_GET_USER_KEY;
						authentication(message, cmd.lba, cmd.sector_count);
						break;
					}
					
					continue;
					
				}
				else if (cmd.cmd_type == READ && cmd.lba == 96) {
				//currSystemState == SYS_NOT_AUTHENTICATED, action is part of authentication process: reading data from Jasmine (get Jasmin response)
					uart_printf("part of auth process: read data from J");
					uart_printf("msg is: %s", messageToUser);
					//uart_printf("currAuthenticationState is: %d", currAuthenticationState);
					
					char message[BYTES_PER_SECTOR] = {0};
					switch (currAuthenticationState){	
					case AUTH_GET_USER_CHALLENGE: //message is user challenge (and response to server challenge)
						currAuthenticationState = AUTH_SEND_TO_USER;
						authentication(message, cmd.lba, cmd.sector_count);
						currAuthenticationState = AUTH_AUTHENTICATION_FINISHED;
						currSystemState = SYS_AUTHENTICATED;
						break;
					case AUTH_GET_USER_KEY:
					default:
						uart_printf("type R, lba 96, AuthState: %d", currAuthenticationState);
						currAuthenticationState = AUTH_SEND_TO_USER;
						authentication(message, cmd.lba, cmd.sector_count);
						break;
					}
					
					// if (currAuthenticationState == AUTH_GET_USER_KEY){
						// currAuthenticationState = AUTH_SEND_TO_USER;
						// authentication(message,cmd.lba,cmd.sector_count);
						// currAuthenticationState = AUTH_GET_USER_CHALLENGE;
					// } 
					// else {
						// currAuthenticationState = AUTH_SEND_TO_USER;
						// authentication(message,cmd.lba,cmd.sector_count);
						// currAuthenticationState = AUTHENTICATION_FINISHED;
						// currSystemState = AUTHENTICATED;
						//user verify server and then authentication finish
					// }
					
					
					continue;
				}
				else { 
					if(currSystemState == SYS_INITIAL_STATE){				
						if (cmd.cmd_type == READ) {
							ftl_read(cmd.lba, cmd.sector_count);
						}
						else {
							ftl_write(cmd.lba, cmd.sector_count);
						}
					}
					uart_printf("%d:   type = %s, lba = %d", commandCounter, (cmd.cmd_type == READ ? "R" : "W"), cmd.lba);
					// notAuthenticated(cmd.lba);	
					// continue;
				}
						
				//TODO: add timeout to Authentication after 5 minutes?
				//		change currSystemState to SYS_NOT_AUTHENTICATED and currAuthenticationState to AUTH_INITIAL_STATE
			
			/*** GAL AND SHANI PROJECT ***/
			/*** END OF CURRENT CHANGE ***/			
		}
		else if (g_sata_context.slow_cmd.status == SLOW_CMD_STATUS_PENDING) {
			void (*ata_function)(UINT32 lba, UINT32 sector_count);

			slow_cmd_t* slow_cmd = &g_sata_context.slow_cmd;
			slow_cmd->status = SLOW_CMD_STATUS_BUSY;

			ata_function = search_ata_function(slow_cmd->code);
			ata_function(slow_cmd->lba, slow_cmd->sector_count);

			slow_cmd->status = SLOW_CMD_STATUS_NONE;
		}
		else {
			// idle time operations
		}
	}
}

void sata_reset(void)
{
	disable_interrupt();

	mem_set_sram(&g_sata_context, 0, sizeof(g_sata_context));

	g_sata_context.write_cache_enabled = TRUE;
	g_sata_context.read_look_ahead_enabled = TRUE;

	SETREG(PMU_ResetCon, RESET_SATA | RESET_SATADWCLK | RESET_SATAHCLK | RESET_PMCLK | RESET_PHYDOMAIN);
	delay(100);

	SETREG(PHY_DEBUG, 0x400A040E);
	while ((GETREG(PHY_DEBUG) & BIT30) == 1);

	SETREG(SATA_BUF_PAGE_SIZE, BYTES_PER_PAGE);
	SETREG(SATA_WBUF_BASE, (WR_BUF_ADDR - DRAM_BASE));
	SETREG(SATA_RBUF_BASE, (RD_BUF_ADDR - DRAM_BASE));
	SETREG(SATA_WBUF_SIZE, NUM_WR_BUFFERS);
	SETREG(SATA_RBUF_SIZE, NUM_RD_BUFFERS);
	SETREG(SATA_WBUF_MARGIN, 16);
	SETREG(SATA_RESET_WBUF_PTR, BIT0);
	SETREG(SATA_RESET_RBUF_PTR, BIT0);

	SETREG(SATA_NCQ_BASE, g_sata_ncq.queue);

	SETREG(SATA_EQ_CFG_1, BIT0 | BIT14 | BIT9 | BIT16 | ((NUM_BANKS / 2) << 24));
	SETREG(SATA_EQ_CFG_2, (EQ_MARGIN & 0xF) << 16);

	SETREG(SATA_CFG_10, BIT0);

	SETREG(SATA_NCQ_CTRL, AUTOINC | FLUSH_NCQ);
	SETREG(SATA_NCQ_CTRL, AUTOINC);
	SETREG(SATA_CFG_5, BIT12 | BIT11*BSO_RX_SSC | (BIT9|BIT10)*BSO_TX_SSC | BIT4*0x05);
	SETREG(SATA_CFG_8, 0);
	SETREG(SATA_CFG_9, BIT20);

	SETREG(SATA_MAX_LBA, MAX_LBA);

	SETREG(APB_INT_STS, INTR_SATA);

	#if OPTION_SLOW_SATA
	SETREG(SATA_PHY_CTRL, 0x00000310);
	#else
	SETREG(SATA_PHY_CTRL, 0x00000300);
	#endif

	SETREG(SATA_ERROR, 0xFFFFFFFF);
	SETREG(SATA_INT_STAT, 0xFFFFFFFF);

	SETREG(SATA_CTRL_1, BIT31);

	while ((GETREG(SATA_INT_STAT) & PHY_ONLINE) == 0);

	SETREG(SATA_CTRL_1, BIT31 | BIT25 | BIT24);

	SETREG(SATA_INT_ENABLE, PHY_ONLINE);

	enable_interrupt();
}

void delay(UINT32 const count)
{
	static volatile UINT32 temp;
	UINT32 i;

	for (i = 0; i < count; i++)
	{
		temp = i;
	}
}

