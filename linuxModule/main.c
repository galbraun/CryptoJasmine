

void main(){
	
//	char* data= "heyyou";
//	char* mngmnt = "111";
//	char* combined = data | mngmnt;
//	printf("%s\n",combined);
	
//	write(,(void*)data,sizeof(data));
	
	if((fd = open("/dev/sdb", O_RDWR | O_DIRECT | O_SYNC | O_LARGEFILE )) < 0) {
        perror("open error on file /dev/sdb");
        exit(-1);
	}
	printf("OpenSSD opened correctly\n");
	
}