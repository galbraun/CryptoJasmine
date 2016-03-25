#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdint.h>
#include "aes.h"
#include "md5.h"
#include "eke.h"

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#define SECT_SIZE 512

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

void printMenu(){
	printf("\nWelcome to Jasmine!\n");
	printf("1 - Login\n");
	printf("2 - Read from lba\n");
	printf("3 - Write to lba\n");
	printf("0 - Logout\n\n");
	printf("Enter your choise: ");
}

void authentication(){
	int fd;	
	FILE* f;

	int64_t r_lba = 100;
	off64_t r_pos = (off64_t)(r_lba * SECT_SIZE);
	int64_t w_lba = 102;
	off64_t w_pos = (off64_t)(w_lba * SECT_SIZE);
	int 	nSectors = 1;
	int 	readCnt = 1;
	int 	byteCnt = 1;	
	uint8_t readbuffer[nSectors * SECT_SIZE];

	uint8_t iv[]  = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f };
	uint8_t buffer[64] = {0};
	uint8_t result[64] = {0};
	
	char password[] = "testtest"; // TODO: this is for test - need to pull from passwordfile by userName
	char userName[] = "shani";

	uint8_t* dataPart1 = NULL;
	uint8_t* dataPart2 = NULL;
	uint8_t* hashedPass_user = NULL;
	
	uint64_t user_random	= 0;
	uint64_t user_public = 0;
	uint64_t user_symetricKey  = 0;
	uint64_t server_Challenge  = 0;
	uint64_t dec_server_Challenge  = 0;
	uint64_t user_Challenge	  = 0;
	uint64_t user_ChallengeAck = 0;

	int testCnt = 0;
	uint8_t msg[128] = {0};
	uint8_t tmpInput[64] = {0};

	// 1 - write userName, user public key
	if((fd = open("/dev/sdb", O_RDWR)) < 0) {
		perror("open (write) error on file /dev/sdb");
		exit(-1);
	}
	printf("OpenSSD opened correctly\n");

	int userNameSize = strlen(userName);
	
	memcpy(msg, userName, userNameSize);
	msg[userNameSize] = '|';

	hashedPass_user = (uint8_t*) md5Hash(password, strlen(password));	// hash user password	
	printf("hashedPass_user: %s\n", hashedPass_user);
	user_random = randomint64(); 
	user_public = powmodp(G, user_random); // create user public key
	//user_public = 0;
	printf("user_public: %016lx\n", user_public);

	memset(tmpInput, 0, sizeof(tmpInput));
	memcpy(tmpInput, &user_public, sizeof(user_public));
	encryptBuffer(msg+userNameSize+1, tmpInput, sizeof(tmpInput), hashedPass_user);// encrypt user public key with user pass

	testCnt = pwrite(fd, msg, sizeof(msg), w_pos); // send to server user name + encrypted user public key
	printf("testCnt is: %d, w_pos is %ld\n", testCnt, w_pos);

	f = fdopen(fd, "w");
	fflush(f);

	//printf("OK\n");
	//uint8_t msg2[SECT_SIZE] = {0};
	//memset(tmpInput, 0, sizeof(tmpInput));
	//memcpy(tmpInput,&user_public,64);
	//encryptBuffer(msg2, tmpInput, 64, hashedPass_user);// encrypt user public key with user pass
	//decryptBuffer(result, msg2, hashedPass_user);
	//uint64_t tmp = 0;

	//memcpy(&tmp, result, sizeof(tmp));
	//memset(result, 0, sizeof(result));
	//printf("dec user_public: %016lx\n", tmp);
	//if( tmp == user_public) printf("OK :) !\n");
	//else printf("NO! :( \n");
	
	free(hashedPass_user);

	close(fd);

	printf("\nend 1 - sleeping\n");
	sleep(3);		

	// 2 - read server challenge and server public	
	if((fd = open("/dev/sdb", O_RDWR)) < 0) {
		perror("open (write) error on file /dev/sdb");
		exit(-1);
	}
	printf("OpenSSD opened correctly\n");

	for(int i = 0; i < nSectors * SECT_SIZE; i++) 
		readbuffer[i] = 0;
	readCnt = pread(fd, readbuffer, nSectors*SECT_SIZE,r_pos);

	dataPart1 = readbuffer;
	dataPart2 = readbuffer + 64;
	
	memset(result,0,sizeof(result));
	hashedPass_user = (uint8_t*) md5Hash(password, strlen(password));
	decryptBuffer(result, dataPart2, hashedPass_user);

	uint64_t server_public = 0;
	memcpy(&server_public, result, sizeof(server_public));
	
	printf("server public is: %016lx\n", server_public);
				
	user_symetricKey = powmodp(server_public, user_random);
	printf("user_symetricKey: %016lx\n", user_symetricKey);
	
	uint8_t tmpSymetric[64] = {0};
	memset(result,0,sizeof(result));
	memcpy(tmpSymetric, &user_symetricKey, sizeof(user_symetricKey));
	decryptBuffer(result, dataPart1, tmpSymetric);
	
	server_Challenge=0;
	memcpy(&server_Challenge, result, sizeof(server_Challenge));
	
	printf("server challenge is: %016lx\n", server_Challenge);
	
	free(hashedPass_user); //allocated by md5Hash 
	
	//printf("Cnt is: %d, readbuffer is %s\n", readCnt, readbuffer);

	close(fd);

	printf("\nend 2 - sleeping\n");
	sleep(3);	
		
	//3 - write server challenge ake and user challenge
	if((fd = open("/dev/sdb", O_RDWR)) < 0) {
		perror("open (write) error on file /dev/sdb");
		exit(-1);
	}
	printf("OpenSSD opened correctly\n");

	memset(msg,0,sizeof(msg));
	memset(tmpInput,0,sizeof(tmpInput));
	uint8_t tmpKey[64] = {0};

	memcpy(tmpKey,&user_symetricKey,sizeof(user_symetricKey));
	memcpy(tmpInput,&server_Challenge,sizeof(server_Challenge));
	encryptBuffer(msg, tmpInput, sizeof(tmpInput), tmpKey);
	//**
	memset(result, 0, sizeof(result));
	decryptBuffer(result, msg, tmpKey);
	memcpy(&dec_server_Challenge, result, sizeof(dec_server_Challenge));

	//**
	user_Challenge = randomint64();
	memset(tmpInput,0,sizeof(tmpInput));
	memcpy(tmpInput,&user_Challenge,sizeof(user_Challenge));
	encryptBuffer(msg + 64, tmpInput, sizeof(tmpInput), tmpKey);

	//**
	dec_server_Challenge = 0; //dec user challenge
	memset(result, 0, sizeof(result));
	decryptBuffer(result, msg+64, tmpKey);
	memcpy(&dec_server_Challenge, result, sizeof(dec_server_Challenge));
	printf("dec_user_Challenge is: %016lx\n", dec_server_Challenge);
	//**

	testCnt = pwrite(fd, msg, sizeof(msg), w_pos); // send to server user name + encrypted user public key
	printf("testCnt is: %d, w_pos is %ld\n", testCnt, w_pos);

	f = fdopen(fd, "w");
	fflush(f);

	close(fd);

	printf("\nend 3 - sleeping\n");
	sleep(3);

	//4 - read user challenge ake
	//printf("dec_server_Challenge is: %016lx\n", dec_server_Challenge);			
	if((fd = open("/dev/sdb", O_RDWR)) < 0) {
		perror("open (write) error on file /dev/sdb");
		exit(-1);
	}
	printf("OpenSSD opened correctly\n");

	for(int i = 0; i < nSectors * SECT_SIZE; i++) 
		readbuffer[i] = 0;
	readCnt = pread(fd, readbuffer, nSectors*SECT_SIZE, r_pos);
	
	dataPart1 = readbuffer;
	
	memset(tmpKey, 0, sizeof(tmpKey));
	memcpy(tmpKey,&user_symetricKey,sizeof(user_symetricKey));
	decryptBuffer(result, dataPart1, tmpKey);
	memcpy(&user_ChallengeAck, result, sizeof(user_ChallengeAck));
	
	printf("user challenge: %016lx\n", user_Challenge);
	printf("user challenge ake: %016lx\n", user_ChallengeAck);

	close(fd);

}

int main(){
	int fd;	

	int64_t r_lba = 100;
	off64_t r_pos = (off64_t)(r_lba * SECT_SIZE);
	int 	nSectors = 1;
	int 	readCnt = 1;
	int 	byteCnt = 1;	
	uint8_t readbuffer[nSectors * SECT_SIZE];

	int choice = 0;
	printMenu();
	scanf("%d", &choice);
	while(choice != 0) 
	{


	
		switch ( choice ) {
		case 1:
		{
			authentication();
			break;
		}
		case 2:
		{
			if((fd = open("/dev/sdb", O_RDWR)) < 0) {
				perror("open (write) error on file /dev/sdb");
				exit(-1);
			}
			printf("OpenSSD opened correctly\n");

			int64_t user_read_lba;
			
			printf("Enter lba: ");
			scanf("%ld", &user_read_lba);
		
			off64_t user_read_pos = (off64_t)(user_read_lba * SECT_SIZE);
		
			for(int i = 0; i < nSectors * SECT_SIZE; i++) 
				readbuffer[i] = 0;
			readCnt = pread(fd, readbuffer, nSectors*SECT_SIZE, user_read_pos);
			readbuffer[nSectors*SECT_SIZE-1] = 0;

			char responseBuffer[nSectors * SECT_SIZE];
			for(int i = 0; i < nSectors * SECT_SIZE; i++) 
				responseBuffer[i] = 0;

			readCnt = pread(fd, responseBuffer, nSectors*SECT_SIZE, r_pos);
			responseBuffer[nSectors*SECT_SIZE-1] = 0;

			if (strcmp(responseBuffer,"AUTHENTICATED")==0){
				printf("Data received: \n%s\n", readbuffer);			
			}
			else {
				printf("Data received: \n%s\n", responseBuffer);
			}
			
			// TODO: add encryption/decryption for data transfer
			
			close(fd);

			break;
		}
		case 3:
		{

			if((fd = open("/dev/sdb", O_RDWR)) < 0) {
				perror("open (write) error on file /dev/sdb");
				exit(-1);
			}
			printf("OpenSSD opened correctly\n");

			int64_t user_write_lba;
			
			printf("Enter lba: ");
			scanf("%ld", &user_write_lba);
			
			char msg[nSectors*SECT_SIZE];
			for(int i = 0; i < nSectors * SECT_SIZE; i++)
				msg[i]=0;			

			printf("Enter data to write ( max size - 512 charecters ): ");
			scanf("%s",msg);
			
			off64_t user_write_pos = (off64_t)(user_write_lba * SECT_SIZE);
			
			int writeCnt = pwrite(fd, msg, nSectors*SECT_SIZE, user_write_pos);
			
			char responseBuffer[nSectors * SECT_SIZE];
			for(int i = 0; i < nSectors * SECT_SIZE; i++) 
				responseBuffer[i] = 0;

			readCnt = pread(fd, responseBuffer, nSectors*SECT_SIZE, r_pos);
			responseBuffer[nSectors*SECT_SIZE-1] = 0;

			if (strcmp(responseBuffer,"AUTHENTICATED")==0){
				printf("Data has been written successfully\n");
			}
			else {
				printf("Response: \n%s\n", responseBuffer);
			}

			close(fd);

			break;
		}
		default:
			printf("Invalid choise\n");		
		}	


		printMenu();
		scanf("%d", &choice);

	}
		//TODO: add logout
	return 0;
}

/*

	char buffer[20] = {0};

	printf("input max = 20\n");

	scanf("%s", buffer);

	printf("buffer is %s\n", buffer);



	int byteCnt = 1;

 	byteCnt = pwrite(fd, buffer, sizeof(buffer), w_pos);

	printf("byteCnt is: %d, w_pos is %ld\n", byteCnt, w_pos);

	//lseek64(fd, pos, SEEK_SET);



	char 	readbuffer[nSectors * SECT_SIZE];

	for(int i = 0; i < nSectors * SECT_SIZE; i++) 

		readbuffer[i] = 0;



	int readCnt = pread(fd, readbuffer, nSectors * SECT_SIZE, r_pos);

	printf("readCnt is: %d, readbuffer is %s\n", readCnt, readbuffer);

*/

	//int testCnt = pwrite(fd, buffer, nSectors * SECT_SIZE, pos);
	//printf("testCnt is: %d,pos is %d\n",testCnt,pos);

/*	int fd;	
	if((fd = open("/dev/sdb", O_RDWR )) < 0) {
		perror("open error on file /dev/sdb");
		exit(-1);
	}

	int64_t lba = 100;	
	char* buffer = "testtest";
	int nSectors = 1;
	char readbuffer[nSectors*SECT_SIZE];

	off64_t pos = (off64_t)(lba*SECT_SIZE);
	lseek64(fd,pos,SEEK_SET);
	int readCnt = read(fd,readbuffer,nSectors*SECT_SIZE);
	printf("readCnt is: %d,buffer is %s\n",readCnt,readbuffer);
	return 0;
*/
/*

	 uint8_t iv[]  = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f };

	 uint8_t buffer[64] ={0};

	 uint8_t result[64] = {0};

	 uint8_t input[64] = {0};

	 

	 uint64_t int64Var = P;

	 uart_printf("int64Var= %" PRIx64 "\n", int64Var);

	 

	// user side

	char password[] = "testtest";

	char* hashedPass_user = md5Hash(password,strlen(password));		

	uart_printf("hashedPass_user: %s\n",hashedPass_user);

	uint64_t user_random = randomint64();

	uart_printf("user_random= %" PRIx64 "\n", user_random);

	uint64_t user_public = powmodp(G, user_random);

	memcpy(input,&user_public,sizeof(user_public));

	uart_printf("user_public= %" PRIx64 "\n", user_public);

	uart_printf("size of user_public is: %d\n", sizeof(user_public));

	uart_printf("size of input is: %d\n", sizeof(input));

	AES128_CBC_encrypt_buffer(buffer, input, sizeof(input), hashedPass_user, iv);

	// sends to server username and encrypted user_public with hashedPass_user as the key



	int testCnt = pwrite(fd,buffer,nSectors*SECT_SIZE,pos);

	printf("testCnt is: %d,pos is %d\n",testCnt,pos);





	// user side

	// decrypt server_public 

	uint64_t user_symetricKey = powmodp(server_public,user_random);

	uart_printf("user_symetricKey= %" PRIx64 "\n", user_symetricKey);

	uint32_t userChallenge = randomint32();

	uart_printf("(32bit)userChallenge= %" PRIx64 "\n", userChallenge);

	// decrypt serverChallenge

	uint64_t combinedChallange = userChallenge;

	combinedChallange = combinedChallange << 32 | serverChallenge;

	uart_printf("combinedChallange= %" PRIx64 "\n", combinedChallange);

	// send to server combinedChallagne encryted with user_symetricKey



		free(hashedPass_user);

*/