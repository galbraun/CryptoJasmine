#include "aes.h"
#include "eke.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#define SECT_SIZE 512
#define MESSAGE_TO_SERVER_SIZE 128

void authentication(int fd){
	int64_t lba = 100;	
	int nSectors = 1;
	char readbuffer[nSectors*SECT_SIZE];

	off_t pos = (off_t)(lba*SECT_SIZE);

	// phase 1
	// sends to server username and encrypted user_publickey with hashedPass_user as the key
	char messageToServer[MESSAGE_TO_SERVER_SIZE] = {0};
	
	
	//TODO: add username into messageToServer from user input and add | after.
	// calculate the size of username , 
	
	char* username = "gal";
	
	strcpy(messageToServer,username);
	messageToServer[strlen(username)] = '|';

	 uint8_t iv[]  = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f };
	 uint8_t buffer[64] ={0};
	 uint8_t result[64] = {0};
	 uint8_t input[64] = {0};

	char password[] = "testtest"; // TODO: change to get this from user input
	char* hashedPass_user = md5Hash(password,strlen(password));	// hash user password	
	uint64_t user_random = randomint64(); 
	uint64_t user_publickey = powmodp(G, user_random); // create user public key
	memcpy(input,&user_publickey,sizeof(user_publickey));

	AES128_CBC_encrypt_buffer(messageToServer[strlen(username)+1], input, sizeof(input), hashedPass_user, iv); // encrypt user public key with user password
	int testCnt = pwrite(fd,messageToServer,nSectors*SECT_SIZE,pos); // send to server user name + encrypted user public key
	printf("testCnt is: %d,pos is %d\n",testCnt,pos);
	
	// get server public key and server challenge
	int readCnt = pread(fd,readbuffer,nSectors*SECT_SIZE,pos);
	printf("readCnt is: %d,buffer is %s\n",readCnt,readbuffer);
		
	for (int i = 0; i<nSectors*SECT_SIZE;i++ ) {
		printf("%c",readbuffer[i]);		
	}

	// TODO: split message 
	
	
	// phase 2
	// send to server combinedChallagne encryted with user_symetricKey
	// decrypt server_public 
	uint64_t user_symetricKey = powmodp(server_public,user_random);
	uint32_t userChallenge = randomint32();

	// decrypt serverChallenge
	uint64_t combinedChallange = userChallenge;
	combinedChallange = combinedChallange << 32 | serverChallenge;
	
	// TODO: add encryption to combinedChallagne
	for (int i=0; i<MESSAGE_TO_SERVER_SIZE; i++ ){
		messageToServer[i]=0;
	}
	
	memcpy(messageToServer,combinedChallagne,sizeof(combinedChallagne));
	
	int testCnt = pwrite(fd,messageToServer,nSectors*SECT_SIZE,pos); // send to server combined challenge
	printf("testCnt is: %d,pos is %d\n",testCnt,pos);
	
	//phase 3 
	// check response from server is valid
	int readCnt = pread(fd,readbuffer,nSectors*SECT_SIZE,pos);
	printf("readCnt is: %d,buffer is %s\n",readCnt,readbuffer);
		
	for (int i = 0; i<nSectors*SECT_SIZE;i++ ) {
		printf("%c",readbuffer[i]);		
	}

	// TODO: verify server result 
	return true;
}

int main(){
//	char* data= "heyyou";
//	char* mngmnt = "111";
//	char* combined = data | mngmnt;
//	printf("%s\n",combined);

//	write(,(void*)data,sizeof(data));
//	int fd;	
//	if((fd = open("/dev/sdb", O_RDWR )) < 0) {
//		perror("open error on file /dev/sdb");
//		exit(-1);
//	}
//	printf("OpenSSD opened correctly\n");
//	int64_t lba = 100;	
//	char* buffer = "testtest";
//	int nSectors = 1;
//	char readbuffer[nSectors*SECT_SIZE];

//	off64_t pos = (off64_t)(lba*SECT_SIZE);
//	int byteCnt = pwrite(fd,buffer,nSectors*SECT_SIZE,pos);
//	printf("byteCnt is: %d,pos is %d\n",byteCnt,pos);

//	lseek64(fd,pos,SEEK_SET);

//	int readCnt = read(fd,readbuffer,nSectors*SECT_SIZE);
//	printf("readCnt is: %d,buffer is %s\n",readCnt,readbuffer);
//	return 0;

//	int testCnt = pwrite(fd,buffer,nSectors*SECT_SIZE,pos);
//	printf("testCnt is: %d,pos is %d\n",testCnt,pos);

	int fd;	
	if((fd = open("/dev/sdb", O_RDWR )) < 0) {
		perror("open error on file /dev/sdb");
		exit(-1);
	}
	
	bool status = authentication(fd);
	if (!status){
		printf("Authentication Failed!");
	}

	return 0;
}