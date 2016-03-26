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
#define AUTH_SESSION_TIMEOUT 60 // in seconds
#define POW10BY7 10000000

unsigned char msgToUser[MAX_MSG_SIZE] = {0};
int  commandCounter = 0;

const char username[] = "user";
char password[] = "12345678";

const UINT32 readPermissions = 17; // read permissions to banks 4 & 0
const UINT32 writePermissions = 18; // write permissions to banks 1 & 4

ESystemState		 currSysState  = SYS_INITIAL_STATE;
EAuthenticationState currAuthState = AUTH_INITIAL_STATE; // after an authentication process began - sign in which step in the process 
EMsgToUserState		 currMsgState  = MSG_INVALID_STATE;
int timerFlag = 0;
//int currentAuthenticatedUserId = -1;
int notAuthorizedFlag = 0;

#define get_num_bank(lpn)       ((lpn) % NUM_BANKS)
#define get_lpn(bank, page_num) (g_misc_meta[bank].lpn_list_of_cur_vblock[page_num])
#define CHECK_LPAGE(lpn)        ASSERT((lpn) < NUM_LPAGES)

// Get data form RAM
void getData(int lba, unsigned char* msg, int msgMaxSize)
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
	memset(msgToUser, 0, MAX_MSG_SIZE);
}

void decryptBuffer(uint8_t* result, const uint8_t* data, const uint8_t* key)
{	
	uint8_t iv[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};
	
	uint8_t input[64] = {0};
	memcpy(input, data, 64);
	
	AES128_CBC_decrypt_buffer(result + 0,  input+0,  16, key, iv);
	AES128_CBC_decrypt_buffer(result + 16, input+16, 16, 0, 0);
	AES128_CBC_decrypt_buffer(result + 32, input+32, 16, 0, 0);
	AES128_CBC_decrypt_buffer(result + 48, input+48, 16, 0, 0);
}

void encryptBuffer(uint8_t* result, uint8_t* data, uint32_t length, const uint8_t* key)
{	
	uint8_t iv[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};
	
	AES128_CBC_encrypt_buffer(result, data, length, key, iv);
}


void badRead(int lba){
	memset(msgToUser,0,sizeof(msgToUser));
	if(lba == 102){
		sendData(msgToUser, sizeof(msgToUser));
	}
	else{
		char tmpString[] = "NOT_AUTHENTICATED";
		memcpy(msgToUser,tmpString,sizeof(tmpString));
		if(currSysState != SYS_INITIAL_STATE) sendData(msgToUser, sizeof(msgToUser));
	}
}

void authentication(CMD_T cmd){
	
	 // TODO: this is for test - need to pull from passwordfile by userName
	char* currentUserName = NULL;
//	static int currentUserId = -1;

	uint8_t  result[64]	= {0};
	uint8_t* dataPart1 = NULL;
	uint8_t* dataPart2 = NULL;
	uint8_t* hashedPass_server = NULL;
	unsigned char msg[BYTES_PER_SECTOR] = {0};
	
	static uint64_t server_random;
	static uint64_t server_symetricKey;
	static uint64_t server_Challenge;
	uint64_t server_public 	= 0;
	uint64_t server_ChallengeAck = 0;
	uint64_t user_Challenge	= 0;
	
	uint8_t tmpKey[64] = {0};
	uint8_t tmpInput[64] = {0};
	
	switch(currAuthState)
	{
	case AUTH_INITIAL_STATE: //expect to receive user pablic key
		if(cmd.cmd_type == READ){ // Not Good
			badRead(cmd.lba);
		}
		if(cmd.cmd_type == WRITE && cmd.lba == 102) { //OK: get user key and do actions
			uart_printf(" OK AUTH_INITIAL_STATE: ");
			
			if(currSysState == SYS_INITIAL_STATE) {
				currSysState = SYS_NOT_AUTHENTICATED;				
			}
			
			memset(msg,0,sizeof(msg));
			getData(cmd.lba, msg, BYTES_PER_SECTOR);
			
			currentUserName = (char*)msg;
			dataPart1 = (uint8_t*) strchr((char*)msg,'|') + 1;
			*strchr((char*)msg,'|') = 0; //so userName will be null terminated string
			
//			currentUserId = getUserID(userName);
//			password = users[currentUserId].password;
			
			if (strcmp(currentUserName,username)==0){
				//uart_printf("  user name: %s, msg size = %d", userName, size);
			
				hashedPass_server = md5Hash(password, strlen(password));
				
				uart_printf( "hashedPass_server - %s " ,hashedPass_server);
				
				memset(result, 0, sizeof(result));
				decryptBuffer(result, dataPart1, hashedPass_server);
				uint64_t user_public = 0;
				memcpy(&user_public, result, sizeof(user_public));
				memset(result, 0, sizeof(result));
				
//				uart_printf("  g^x = %016llx", user_public);
				
				server_random = randomint64();
				server_public = powmodp(G, server_random);			
				server_symetricKey = powmodp(user_public, server_random);
				
				server_Challenge = randomint64();
				memset(tmpKey, 0, sizeof(tmpKey));
				memcpy(tmpKey, &server_symetricKey, sizeof(server_symetricKey));
				memset(tmpInput, 0, sizeof(tmpInput));
				memcpy(tmpInput, &server_Challenge, sizeof(server_Challenge));
				encryptBuffer(msgToUser, tmpInput, sizeof(tmpInput), tmpKey);
				
				memset(tmpInput, 0, sizeof(tmpInput));
				memcpy(tmpInput, &server_public, sizeof(server_public));
				encryptBuffer(msgToUser+64, tmpInput, sizeof(tmpInput), hashedPass_server);
				
				free(hashedPass_server); //allocated by md5Hash 
				
				currMsgState  = MSG_READY;
				currAuthState = AUTH_GOT_USER_KEY;
				
//				uart_printf("  g^y = %016llx", server_public);
				uart_printf("  symetricKey = %016llx", server_symetricKey);
			}
			else {
				uart_printf("Invalid username");
			}
			
			

		}
		break;
		
	case AUTH_GOT_USER_KEY: //expect to send server key and challenge
		if(cmd.cmd_type == READ && cmd.lba == 96) { //OK: send server key and challenge
			uart_printf(" OK AUTH_GOT_USER_KEY: ");
			if(currMsgState != MSG_READY){
				memcpy(msgToUser, "Msg is not ready! ..?", 21);
			}
			sendData(msgToUser, sizeof(msgToUser)+1);
			currMsgState = MSG_INVALID_STATE;
			currAuthState = AUTH_SERVER_KEY_CHALLANGE_WAS_SENT;
		}
		if(cmd.cmd_type == WRITE){ // Not Good
			memset(msg,0,sizeof(msg));
			getData(cmd.lba, msg, BYTES_PER_SECTOR);
			
			// TODO: print error?
			currAuthState = AUTH_INITIAL_STATE;
		}
		break;
		
	case AUTH_SERVER_KEY_CHALLANGE_WAS_SENT: //expect to receive server challenge ake and user challenge
		if(cmd.cmd_type == READ){ // Not Good
			badRead(cmd.lba);
		}
		if(cmd.cmd_type == WRITE && cmd.lba == 102) { // OK: get server challenge ake and user challenge
			uart_printf(" OK AUTH_SERVER_KEY_CHALLANGE_WAS_SENT: ");
			memset(msg,0,sizeof(msg));
			getData(cmd.lba, msg, BYTES_PER_SECTOR);
			
			//uart_printf("  got data. size = %d", strlen(msg));
			
			dataPart1 = (uint8_t*) msg; //encrypted server_ChallengeAck
			dataPart2 = (uint8_t*) msg + 64; // encrypted userChallenge
			//*strchr(msg,'|') = 0; //so dataPart1 will be null terminated string
			
			memset(tmpKey, 0, sizeof(tmpKey));
			memcpy(tmpKey, &server_symetricKey, sizeof(server_symetricKey));
			decryptBuffer(result, dataPart1, tmpKey); 
			//uart_printf("  buff was decrypttmpSymetric
			server_ChallengeAck=0;
			memcpy(&server_ChallengeAck, result, sizeof(server_ChallengeAck));
			memset(result, 0, sizeof(result));
			
			uart_printf("  server challenge: %016llx", server_Challenge);
			uart_printf("  server challenge Ake: %016llx", server_ChallengeAck);
			if(server_Challenge == server_ChallengeAck){
				uart_printf("  challenge OK!");
				decryptBuffer(result, dataPart2, tmpKey);
				user_Challenge=0;
				memcpy(&user_Challenge, result, sizeof(user_Challenge));
				memset(result, 0, sizeof(result));
			
				uart_printf("  user challenge: %016llx", user_Challenge);
			
				// TODO: do some manipulation of user_Challenge (xor with symetric key? ) and send it to user
				//encryptBuffer(msgToUser, &user_Challenge, sizeof(user_Challenge), &server_symetricKey);
				//int dataSize = 64; //strlen((char*)dataPart2);
				memcpy(msgToUser, dataPart2, 64);
			
				currMsgState  = MSG_READY;
				currAuthState = AUTH_GOT_USER_CHALLENGE;	
			//uart_printf("massage from user: %s", msg);
			} 
			else{
				uart_printf("  challenge NOT ok!");
				currAuthState = AUTH_INITIAL_STATE;
			}						
		}
		break;
		
	case AUTH_GOT_USER_CHALLENGE: // expect to send user challenge ack
		if(cmd.cmd_type == READ && cmd.lba == 96) { // OK: send user challenge ack
			//char msg[MAX_MSG_SIZE] = "Good! In State 3!";
			//sendData(msg, strlen(msg)+1);
			//uart_printf("massage to user: %s", msg);
//			uart_printf(" OK AUTH_GOT_USER_CHALLENGE: ");
			if(currMsgState != MSG_READY){
				memcpy(msgToUser, "Msg is not ready! ..?", 21);
			}
			sendData(msgToUser, sizeof(msgToUser));
			currMsgState = MSG_INVALID_STATE;
			currAuthState = AUTH_AUTHENTICATION_FINISHED;
			currSysState  = SYS_AUTHENTICATED;
//			currentAuthenticatedUserId = currentUserId;
//			currentUserId = -1;
			timerFlag=1;
			uart_printf("started auth timeout timer");
			start_interval_measurement(TIMER_CH2,TIMER_PRESCALE_1);
		}
		if(cmd.cmd_type == WRITE){ // Not Good
			memset(msg,0,sizeof(msg));
			getData(cmd.lba, msg, BYTES_PER_SECTOR);
			// TODO: print error
			currAuthState = AUTH_INITIAL_STATE;
		}
		break;
	default:
		break;
	}
}

int authorization(int bank,EAuthorizationAction action){
	int tmpBank = 1 << bank;
	uart_printf("tmpBank - %d, rP - %d , wP - %d ",tmpBank,readPermissions,writePermissions);
	if ((action == AUTH_READ) && (readPermissions & tmpBank) ){
		return 1;
	}
	if ((action == AUTH_WRITE) && (writePermissions & tmpBank)){
		return 1;
	}
	
	return 0;
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
unsigned char msg[1] = {0};

	while (1)
	{
		
		if (timerFlag){
			
			UINT32 rtime = 0xFFFFFFFF - GET_TIMER_VALUE(TIMER_CH2);
			// Tick to seconds
			rtime = ((UINT32)((UINT64)rtime * 2 * 1000000 * PRESCALE_TO_DIV(TIMER_PRESCALE_1) / CLOCK_SPEED))/(POW10BY7); 
			
			if ( currSysState == SYS_AUTHENTICATED && rtime > AUTH_SESSION_TIMEOUT ){
				currSysState = SYS_NOT_AUTHENTICATED;
				currAuthState = AUTH_INITIAL_STATE;
				timerFlag = 0;
//				currentAuthenticatedUserId = -1;
				uart_printf("stoped auth timeout timer");
			}
		}
		
		if (eventq_get_count())
		{
			CMD_T cmd;

			eventq_get(&cmd);

			commandCounter++;
	
			UINT32 bank = get_num_bank(cmd.lba / SECTORS_PER_PAGE);
						
			if(currSysState == SYS_AUTHENTICATED){
				uart_printf("SYS_AUTHENTICATED");
				uart_printf("bank is - %d",bank);
				if (cmd.cmd_type == READ && cmd.lba == 96) {
					if (notAuthorizedFlag){
						unsigned char msg[] = "NOT_AUTHORIZED";
						sendData(msg,sizeof(msg));
						notAuthorizedFlag = 0;
					} else {
						unsigned char msg[] = "SUCCESS";
						sendData(msg,sizeof(msg));
					}	
				} 
				else if (cmd.cmd_type == READ) {
					if (authorization(bank,AUTH_READ)){
						ftl_read(cmd.lba, cmd.sector_count);
					}
					else {
						notAuthorizedFlag = 1;
						memset(msg,0,sizeof(msg));
						sendData(msg, sizeof(msg));						
					}
				}
				else {
					if (authorization(bank,AUTH_WRITE)){
						ftl_write(cmd.lba, cmd.sector_count);
					}
					else {
						notAuthorizedFlag = 1;
						memset(msg,0,sizeof(msg));
						getData(cmd.lba, msg, BYTES_PER_SECTOR);
					}
				}
			}
			else {
				if (commandCounter > 2750){
					uart_printf("%d", commandCounter);
//					uart_printf("before: SysState=%d AuthState=%d", currSysState, currAuthState);
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

