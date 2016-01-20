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


/////// GAL AND SHANI PROJECT /////////////
char messageToUser[128] = {0};
messageToUserStatus messageStatus = INVALID_STATE;
#define get_num_bank(lpn)             ((lpn) % NUM_BANKS)
#define get_lpn(bank, page_num)       (g_misc_meta[bank].lpn_list_of_cur_vblock[page_num])
#define CHECK_LPAGE(lpn)              ASSERT((lpn) < NUM_LPAGES)

void authentication(char* message,int lba,int sectCount) {
	 
	 uint8_t iv[]  = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f };
	 uint8_t buffer[64] ={0};
	 uint8_t result[64] = {0};
	 uint8_t input[64] = {0};
	 
	 uint64_t int64Var = P;
	 uart_printf("int64Var= %" PRIx64 "\n", int64Var);
	 
	 char* password = "testtest"; // TODO: this is fot test - need to pull from passwordfile by username
	 char* username;
	 char* userData;
	 char* hashedPass_server;
	 				int i;

	 
	 UINT32 sect_offset = lba % SECTORS_PER_PAGE;
	 UINT32 next_read_buf_id;
	 
	switch (currentState){
		case GET_USER_KEY:
				// server side
				// find user password decrypt it from the password file 

	
				username=message;
				userData = strchr(message,'|')+1;
				
				*(strchr(message,'|'))=0;
				
				strncpy(buffer,userData,strlen(userData));
				
				hashedPass_server = md5Hash(password,strlen(password));
				uart_printf("hashedPass_server: %s\n",hashedPass_server);
				uint64_t server_random = randomint64();
				uart_printf("server_random= %" PRIx64 "\n", server_random);
				uint64_t server_public = powmodp(G, server_random);
				uart_printf("server_public= %" PRIx64 "\n", server_public);

				AES128_CBC_decrypt_buffer(result+0, buffer+0,  16, hashedPass_server, iv);
				AES128_CBC_decrypt_buffer(result+16, buffer+16, 16, 0, 0);
				AES128_CBC_decrypt_buffer(result+32, buffer+32, 16, 0, 0);
				AES128_CBC_decrypt_buffer(result+48, buffer+48, 16, 0, 0);
				
				uint64_t decryptedKey = 0;
				memcpy(&decryptedKey,result,sizeof(uint64_t));
				uart_printf("decrypted message: %" PRIx64 "\n",decryptedKey);
				
				uint64_t server_symetricKey = powmodp(decryptedKey,server_random);
				uart_printf("server_symetricKey= %" PRIx64 "\n", server_symetricKey);
				uint32_t serverChallenge = randomint32();
				uart_printf("(32bit)serverChallenge= %" PRIx64 "\n", serverChallenge);
				
				AES128_CBC_encrypt_buffer(messageToUser, &serverChallenge, sizeof(serverChallenge), &server_symetricKey, iv);
				AES128_CBC_encrypt_buffer(messageToUser+64, &server_public, sizeof(server_public), &hashedPass_server, iv);
				messageStatus = MESSAGE_READY;
				
				// sends to user the server challenge encrypted with  server_symetricKey and server_public encrypted with hashedPass_server
				free(hashedPass_server);

			break;
		case SEND_TO_USER:
				
				next_read_buf_id = (g_ftl_read_buf_id + 1) % NUM_RD_BUFFERS;
				
				while (next_read_buf_id == GETREG(SATA_RBUF_PTR));
				
				for ( int k=0 ; k< strlen(messageToUser); k++){
					write_dram_8(RD_BUF_PTR(g_ftl_read_buf_id)+(36*BYTES_PER_SECTOR)+k,messageToUser[k]);
				}

//				SETREG(SATA_SECT_CNT, -5);
				SETREG(SATA_RBUF_PTR, g_ftl_read_buf_id);	// change sata_read_ptr
				SETREG(BM_STACK_RDSET, next_read_buf_id);	// change bm_read_limit
				SETREG(BM_STACK_RESET, 0x02);				// change bm_read_limit
				
				g_ftl_read_buf_id = next_read_buf_id;
			break; 
		case SEND_SERVER_KEY_CHALLENGE:
		
		default:
			break;
	}	
}

static int autherization(char* user, char* password,UINT32 bank){
	int status;
	
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

	return status;
} 

////////////////////////////////////


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
			
			if (commandCounter<5000){
				commandCounter++;
			} else {
				systemAuthenticationState = NOT_AUTHENTICATED;
			}
			
			// GAL AND SHANI PROJECT ///////////////////////////////
			
			// checking user is authenticated - if not return an error to user
			if ( cmd.cmd_type == READ && cmd.lba == 96 ){  // if ( systemAuthenticationState == NOT_AUTHENTICATED && messageStatus == MESSAGE_INVALID_STATE )
				uart_printf("not authenticated...problem!");
				UINT32 sect_offset  = cmd.lba % SECTORS_PER_PAGE;
				strcpy(messageToUser,"NOT AUTHENTICATED!");
				int i;
				
				UINT32 next_read_buf_id = (g_ftl_read_buf_id + 1) % NUM_RD_BUFFERS;
				
				while (next_read_buf_id == GETREG(SATA_RBUF_PTR));
				
				for ( int k=0 ; k< strlen(messageToUser); k++){
					write_dram_8(RD_BUF_PTR(g_ftl_read_buf_id)+(36*BYTES_PER_SECTOR)+k,messageToUser[k]);
				}
/*
				SETREG(SATA_FIS_D2H_4,0xFFFFFFFF);
				SETREG(SATA_SECT_CNT,0xFFFFFFFF);
				SETREG(SATA_XFER_BYTES,0xFFFFFFFF);
				SETREG(SATA_FIS_D2H_3,0xFFFFFFFF);
				SETREG(SATA_FIS_D2H_1,0xFFFFFFFF);
				SETREG(SATA_FIS_D2H_2,0xFFFFFFFF);
				SETREG(FCP_DMA_CNT,0xFFFFFFFF);
				SETREG(SATA_FIS_D2H_0,0xFFFFFFFF);
		*/		
				SETREG(SATA_FIS_D2H_3,0x0);
				SETREG(SATA_FIS_D2H_1,0x0);
				SETREG(SATA_FIS_D2H_2,0x0);

				UINT32 fis_type = FISTYPE_REGISTER_D2H;
				UINT32 flags = B_IRQ;
				UINT32 status = 1;

				SETREG(SATA_FIS_D2H_0, fis_type | (flags << 8) | (status << 16) | (0 << 24));
							
				SETREG(SATA_RBUF_PTR, g_ftl_read_buf_id);	// change sata_read_ptr
				SETREG(BM_STACK_RDSET, next_read_buf_id);	// change bm_read_limit
				SETREG(BM_STACK_RESET, 0x02);				// change bm_read_limit
				
				g_ftl_read_buf_id = next_read_buf_id;
				
				SETREG(SATA_CTRL_2, SEND_NON_DATA_FIS);

				continue;
			}
			
			if ( cmd.cmd_type == WRITE && cmd.lba == 96 ) { // user sent data to Jasmine - authentication process
				UINT32 sect_offset  = cmd.lba % SECTORS_PER_PAGE;
				char message[BYTES_PER_SECTOR] = {0};
				int i;
				for (  i=0; i<BYTES_PER_SECTOR ; i++){
					char ch = (char)read_dram_8(WR_BUF_PTR(g_ftl_write_buf_id)+(sect_offset*BYTES_PER_SECTOR)+i);
					if (ch==0){
						break;
					}
					message[i]=ch;
				}
				message[i+1]="\0";			

				if (currentState == INVALID_STATE){
					currentState = GET_USER_KEY;
					authentication(message,cmd.lba,cmd.sector_count);
				} else {
					currentState = SEND_SERVER_KEY_CHALLENGE;
					authentication(message,cmd.lba,cmd.sector_count);
				}
			}
			
			if ( cmd.cmd_type == READ && cmd.lba == 96 ) { // user reading data from Jasmine - authentication process
				char message[BYTES_PER_SECTOR] = {0};
				//while (messageStatus == MESSAGE_INVALID_STATE); // wait for the message to be ready for read
				currentState = SEND_TO_USER;
				authentication(message,cmd.lba,cmd.sector_count);
				currentState = GET_USER_CHALLENGE;
				continue;
			}
						
			/////////////////////////////////////////////////////////////////
			
			if (cmd.cmd_type == READ){
				ftl_read(cmd.lba, cmd.sector_count);
			}
			else
			{
				ftl_write(cmd.lba, cmd.sector_count);
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

