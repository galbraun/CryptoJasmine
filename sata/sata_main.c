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
#define MAX_MSG_SIZE 128
unsigned char msgToUser[MAX_MSG_SIZE] = {0};
int  commandCounter = 0;

ESystemState		 currSysState  = SYS_INITIAL_STATE;
EAuthenticationState currAuthState = AUTH_INITIAL_STATE; // after an authentication process began - sign in which step in the process 
EMsgToUserState		 currMsgState  = MSG_INVALID_STATE;

#define get_num_bank(lpn)       ((lpn) % NUM_BANKS)
#define get_lpn(bank, page_num) (g_misc_meta[bank].lpn_list_of_cur_vblock[page_num])
#define CHECK_LPAGE(lpn)        ASSERT((lpn) < NUM_LPAGES)

void cleanMsgToUser(){
	for(int i = 0; i < MAX_MSG_SIZE; i++){
		msgToUser[i] = 0;
	}
}

// Get data form RAM
void getData(int lba, char* msg, int msgMaxSize)
{
	UINT32 sect_offset  = lba % SECTORS_PER_PAGE;
	msg[0] = (char) read_dram_8 (WR_BUF_PTR(g_ftl_write_buf_id) + (sect_offset*BYTES_PER_SECTOR));
	
	for (int i = 1; (i < msgMaxSize) && (i < BYTES_PER_SECTOR) && (msg[i-1] != 0); i++) {
		msg[i] = (char) read_dram_8 (WR_BUF_PTR(g_ftl_write_buf_id) + (sect_offset*BYTES_PER_SECTOR) + i);
	}
	
	msg[msgMaxSize-1] = 0; //msg should be null terminated
	
	g_ftl_write_buf_id = (g_ftl_write_buf_id + 1) % NUM_WR_BUFFERS;
}

// Send data to RAM
void sendData(unsigned char* msg, int msgSize)
{
	UINT32 next_read_buf_id = (g_ftl_read_buf_id + 1) % NUM_RD_BUFFERS;
				
	while (next_read_buf_id == GETREG(SATA_RBUF_PTR));
	
	for (int k = 0; k < msgSize; k++){
		write_dram_8(RD_BUF_PTR(g_ftl_read_buf_id) + (36 * BYTES_PER_SECTOR) + k, msg[k]);
	}

	SETREG(SATA_RBUF_PTR, g_ftl_read_buf_id);	// change sata_read_ptr
	SETREG(BM_STACK_RDSET, next_read_buf_id);	// change bm_read_limit
	SETREG(BM_STACK_RESET, 0x02);				// change bm_read_limit
		
	g_ftl_read_buf_id = next_read_buf_id;
	cleanMsgToUser();
}

void decryptBuffer(uint8_t* result, const uint8_t* data, const uint8_t* key)
{	
	uint8_t iv[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};
	
	uint8_t input[64] = {0};
	memcpy(input, data, 64);
	
	AES128_CBC_decrypt_buffer(result + 0,  input+0,  16, key, iv);
	uart_printf("  before decryption2");
	AES128_CBC_decrypt_buffer(result + 16, input+16, 16, 0, 0);
	AES128_CBC_decrypt_buffer(result + 32, input+32, 16, 0, 0);
	AES128_CBC_decrypt_buffer(result + 48, input+48, 16, 0, 0);
}

void encryptBuffer(uint8_t* result, uint8_t* data, uint32_t length, const uint8_t* key)
{	
	uint8_t iv[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};
	
	AES128_CBC_encrypt_buffer(result, data, length, key, iv);
}

void authentication(CMD_T cmd){
	
	char* password = "testtest"; // TODO: this is for test - need to pull from passwordfile by userName
	char* userName = NULL;

	uint8_t  result[64]	= {0};
	uint8_t* dataPart1 = NULL;
	uint8_t* dataPart2 = NULL;
	uint8_t* hashedPass_server = NULL;
	
	static uint64_t server_random	= 0;
	static uint64_t server_public 	= 0;
	static uint64_t server_symetricKey 	= 0;
	static uint32_t server_Challenge	= 0;
	static uint32_t server_ChallengeAck = 0;
	static uint32_t user_Challenge		= 0;
	
	switch(currAuthState)
	{
	case AUTH_INITIAL_STATE: //expect to receive user pablic key
		if(cmd.cmd_type == READ){ // Not Good
			if(cmd.lba == 102){
				unsigned char msg[MAX_MSG_SIZE] = {0};
				sendData(msg, sizeof(msg));
				//uart_printf("  XXX msg to user");
			}
			else{
				unsigned char msg[MAX_MSG_SIZE] = "SORRY, NOT AUTHENTICATED!";
				if(currSysState != SYS_INITIAL_STATE) sendData(msg, sizeof(msg));
				//uart_printf("  PROBLEM: State 0, cmd = R, lba = %d", cmd.lba);
			}
		}
		if(cmd.cmd_type == WRITE && cmd.lba == 102) { //OK: get user key and do actions
			uart_printf(" OK AUTH_INITIAL_STATE: ");
			
			//if(currSysState == SYS_INITIAL_STATE) {
			//	currSysState = SYS_NOT_AUTHENTICATED;				
			//}
			
			char msg[BYTES_PER_SECTOR] = {0};
			getData(cmd.lba, msg, BYTES_PER_SECTOR);
			
			userName = msg;
			dataPart1 = (uint8_t*) strchr(msg,'|') + 1;
			*strchr(msg,'|') = 0; //so userName will be null terminated string
			uart_printf("  user name: %s", userName);
			
			hashedPass_server = md5Hash(password, strlen(password));
			uart_printf("  hashedPass = %s", hashedPass_server);
			decryptBuffer(result, dataPart1, hashedPass_server);
			
			uint64_t user_public = 0;
			memcpy(&user_public, result, sizeof(uint64_t));
			
			uart_printf("  g^x = %016llx", user_public);
			
			server_random = randomint64();				
			server_symetricKey = powmodp(user_public, server_random);
			
			server_Challenge = randomint32();
			uint8_t tmpBuff[65] = {0};
			memcpy(tmpBuff, &server_symetricKey, sizeof(server_symetricKey));
			encryptBuffer(msgToUser, (uint8_t*) &server_Challenge, sizeof(server_Challenge), tmpBuff);
			
			server_public = powmodp(G, server_random);
			memcpy(tmpBuff, &server_public, sizeof(server_public));
			encryptBuffer(msgToUser+64, tmpBuff, sizeof(server_public),hashedPass_server);
			
			free(hashedPass_server); //allocated by md5Hash 
			
			currMsgState  = MSG_READY;
			currAuthState = AUTH_GOT_USER_KEY;
			
			uart_printf("  y = %016llx", server_random);
			uart_printf("  g^y = %016llx", server_public);
			
			uart_printf("  symetricKey = %016llx", server_symetricKey);
			//uart_printf("massage from user: %s", msg);
		}
		break;
		
	case AUTH_GOT_USER_KEY: //expect to send server key and challenge
		if(cmd.cmd_type == READ && cmd.lba == 96) { //OK: send server key and challenge
			//char msg[MAX_MSG_SIZE] = "Good! In State 1!";
			//sendData(msg, strlen(msg)+1);
			//uart_printf("massage to user: %s", msg);
			uart_printf(" OK AUTH_GOT_USER_KEY: ");
			if(currMsgState != MSG_READY){
				memcpy(msgToUser, "Msg is not ready! ..?", 21);
			}
			sendData(msgToUser, sizeof(msgToUser)+1);
			currMsgState = MSG_INVALID_STATE;
			memset(msgToUser, 0, MAX_MSG_SIZE);
			currAuthState = AUTH_SERVER_KEY_CHALLANGE_WAS_SENT;
		}
		if(cmd.cmd_type == WRITE){ // Not Good
			uart_printf("  PROBLEM: State 1, cmd = W, lba = %d", cmd.lba);
			// TODO: print error?
			// currAuthState = AUTH_INITIAL_STATE;
		}
		break;
		
	case AUTH_SERVER_KEY_CHALLANGE_WAS_SENT: //expect to receive server challenge ake and user challenge
		if(cmd.cmd_type == READ){ // Not Good
			if(cmd.lba == 102){
				unsigned char msg[MAX_MSG_SIZE] = "stam";
				sendData(msg, sizeof(msg));
				uart_printf("  XXX massage to user: %s", msg);
			}
			else{
				unsigned char msg[MAX_MSG_SIZE] = "SORRY, NOT AUTHENTICATED!";
				if(currSysState != SYS_INITIAL_STATE) sendData(msg, sizeof(msg));
				uart_printf("  PROBLEM: State 2, cmd = R, lba = %d", cmd.lba);
				// currAuthState = AUTH_INITIAL_STATE;
			}
		}
		if(cmd.cmd_type == WRITE && cmd.lba == 102) { // OK: get server challenge ake and user challenge
			uart_printf(" OK AUTH_SERVER_KEY_CHALLANGE_WAS_SENT: ");
			char msg[BYTES_PER_SECTOR] = {0};
			getData(cmd.lba, msg, BYTES_PER_SECTOR);
			
			uart_printf("  got data. size = %d", strlen(msg));
			
			dataPart1 = (uint8_t*) msg; //encrypted server_ChallengeAck
			dataPart2 = (uint8_t*) + 64; // encrypted userChallenge
			//*strchr(msg,'|') = 0; //so dataPart1 will be null terminated string
			
			uint8_t tmpBuff[65] = {0};
			memcpy(tmpBuff, &server_symetricKey, sizeof(server_symetricKey));
			decryptBuffer(result, dataPart1, tmpBuff); 
			uart_printf("  buff was decrypt");
			memcpy(&server_ChallengeAck, result, sizeof(uint32_t));
			
			uart_printf("  server challenge: %08x", server_Challenge);
			uart_printf("  server challenge Ake: %08x", server_ChallengeAck);
			if(server_Challenge == server_ChallengeAck) uart_printf("  challenge OK!");
			
			decryptBuffer(result, dataPart2, tmpBuff);
			memcpy(&user_Challenge, result, sizeof(uint32_t));
			
			// uart_printf("  user challenge: %lu", user_Challenge);
			
			// TODO: do some manipulation of user_Challenge (xor with symetric key? ) and send it to user
			//encryptBuffer(msgToUser, &user_Challenge, sizeof(user_Challenge), &server_symetricKey);
			int dataSize = 64; //strlen((char*)dataPart2);
			memcpy(msgToUser, dataPart2, ( dataSize < MAX_MSG_SIZE ? dataSize : MAX_MSG_SIZE));
			currMsgState  = MSG_READY;
			currAuthState = AUTH_GOT_USER_CHALLENGE;	
			//uart_printf("massage from user: %s", msg);			
		}
		break;
		
	case AUTH_GOT_USER_CHALLENGE: // expect to send user challenge ack
		if(cmd.cmd_type == READ && cmd.lba == 96) { // OK: send user challenge ack
			//char msg[MAX_MSG_SIZE] = "Good! In State 3!";
			//sendData(msg, strlen(msg)+1);
			//uart_printf("massage to user: %s", msg);
			uart_printf(" OK AUTH_GOT_USER_CHALLENGE: ");
			if(currMsgState != MSG_READY){
				memcpy(msgToUser, "Msg is not ready! ..?", 21);
			}
			sendData(msgToUser, sizeof(msgToUser));
			currMsgState = MSG_INVALID_STATE;
			memset(msgToUser, 0, MAX_MSG_SIZE);
			currAuthState = AUTH_AUTHENTICATION_FINISHED;
			currSysState  = SYS_AUTHENTICATED;		
		}
		if(cmd.cmd_type == WRITE){ // Not Good
			uart_printf("  PROBLEM: State 3, cmd = R, lba = %d", cmd.lba);
			// TODO: print error
			// currAuthState = AUTH_INITIAL_STATE;
		}
		break;
	default:
		break;
	}
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
	while (1)
	{
		if (eventq_get_count())
		{
			CMD_T cmd;

			eventq_get(&cmd);

			/*if (cmd.cmd_type == READ)
			{
				ftl_read(cmd.lba, cmd.sector_count);
			}
			else
			{
				ftl_write(cmd.lba, cmd.sector_count);
			}*/
			
			commandCounter++;
			//if(currSysState == SYS_INITIAL_STATE)
			//	currSysState = (commandCounter++ < 5000 ? SYS_INITIAL_STATE : //SYS_NOT_AUTHENTICATED);
			
			//if(commandCounter % 50 == 0)
				//uart_printf("%d: type = %s, lba = %d", commandCounter, (cmd.cmd_type == READ ? "R" : "W"), cmd.lba);
			//#TODO: change currSystemState 
			
			if(currSysState == SYS_AUTHENTICATED){
				uart_printf("SYS_AUTHENTICATED");
				if (cmd.cmd_type == READ) {
					// TODO: check autherization
					ftl_read(cmd.lba, cmd.sector_count);
				}
				else {
					// TODO: check autherization
					ftl_write(cmd.lba, cmd.sector_count);
				}
			}
			else {
				if (commandCounter > 2750){
					uart_printf("%d", commandCounter);
					uart_printf("before: SysState=%d AuthState=%d", currSysState, currAuthState);
					authentication(cmd);
					uart_printf("after : SysState=%d AuthState=%d", currSysState, currAuthState);
				} 
				if(currSysState == SYS_INITIAL_STATE){
					if (cmd.cmd_type == READ && cmd.lba != 96 && cmd.lba != 102) {
						ftl_read(cmd.lba, cmd.sector_count);
					}
					else if (cmd.cmd_type == WRITE && cmd.lba != 102){
						ftl_write(cmd.lba, cmd.sector_count);
					}
				}
			}
			
			/*if (cmd.cmd_type == READ)
			{
				if(cmd.cmd_type == READ && cmd.lba == 96){
					char msg[MAX_MSG_SIZE] = "  hi shani";
					msg[0] = (++commandCounter)%10 + '0';
					sendData(msg, strlen(msg)+1);
					uart_printf("massage to user: %s", msg);					
				}
				else{
					ftl_read(cmd.lba, cmd.sector_count);
				}
			}
			else
			{
				if(cmd.cmd_type == WRITE && cmd.lba == 102){
					char msg[BYTES_PER_SECTOR] = {0};
					getData(cmd.lba, msg, BYTES_PER_SECTOR);
					uart_printf("massage from user: %s", msg);
				}
				else{
					ftl_write(cmd.lba, cmd.sector_count);
				}
			}*/
			
		}
		else if (g_sata_context.slow_cmd.status == SLOW_CMD_STATUS_PENDING)
		{
			void (*ata_function)(UINT32 lba, UINT32 sector_count);

			slow_cmd_t* slow_cmd = &g_sata_context.slow_cmd;
			slow_cmd->status = SLOW_CMD_STATUS_BUSY;

			ata_function = search_ata_function(slow_cmd->code);
			ata_function(slow_cmd->lba, slow_cmd->sector_count);

			slow_cmd->status = SLOW_CMD_STATUS_NONE;
		}
		else
		{
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

