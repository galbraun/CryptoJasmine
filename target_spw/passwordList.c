#include "passwordList.h"

Node* createNode(char* user,char* password,Node* next,Node* previous){
	Node* newNode = malloc(sizeof(Node));
	if (newNode==NULL){
		return NULL;
	}
	
	newNode->user = user;
	newNode->password = password;
	newNode->next = next;
	newNode->previous = previous;
	
	for (int i=0; i<BANKS_AMOUNT; i++){
		newNode->banksReadPermissions[i] = 0;
	}
	
	for (int i=0; i<BANKS_AMOUNT; i++){
		newNode->banksWritePermissions[i] = 0;
	}
	return newNode;
}

void destroyNode(Node* node){
	node->next=NULL;
	node->previous=NULL;
	free(node);
}

int getPasswordListSize(PasswordList* list){
	return list->size;
}


PasswordList* createPasswordList(){
	PasswordList* list = malloc(sizeof(PasswordList));
	if (list==NULL){
		free(list);
		return NULL;
	}
	
	list->size=0;
	list->head=NULL;
	list->tail=NULL;

    return list;
}

bool addNewUser(PasswordList* list,char* user,char* password){
	
	if (checkIfUserExists(list,user)){
		return false;
	}
	
	Node* newNode = createNode(user,password,list->head,NULL);
	if (newNode==NULL){
		return false;
	}
	
	if (getPasswordListSize(list)==0){
		list->head = newNode;
		list->tail = newNode;
		list->size = 1;
		return true;
	}
	
	Node* tail = list->tail;
	tail->next = newNode;
	list->tail = newNode;
	list->size+=1;
	return true;
}

bool checkIfUserExists(PasswordList* list,char* user){
	if (getPasswordListSize(list)==0){
		return false;
	}
	
	Node* iter;
	for ( iter = list->head ; iter != NULL ; iter = iter->next ){
		/*if ( strcmp(iter->user,user) == 0 ){
			return true;
		}*/
	}
	
	return false;
}

char** getUserPasswordPointer(PasswordList* list,char* user){
	if (getPasswordListSize(list)==0){
		return NULL;
	}
	
	Node* iter;
	for ( iter = list->head ; iter != NULL ; iter = iter->next ){
		/*if ( strcmp(iter->user,user) == 0 ){
			return &iter->password;
		}*/
	}
	
	return NULL;
}

int** getUserReadPermissionsPointer(PasswordList* list,char* user){
	if (getPasswordListSize(list)==0){
		return NULL;
	}
	
	Node* iter;
	for ( iter = list->head ; iter != NULL ; iter = iter->next ){
		/*if ( strcmp(iter->user,user) == 0 ){
			return &iter->banksReadPermissions;
		}*/
	}
	
	return NULL;
}

int** getUserWritePermissionsPointer(PasswordList* list,char* user){
	if (getPasswordListSize(list)==0){
		return NULL;
	}
	
	Node* iter;
	for ( iter = list->head ; iter != NULL ; iter = iter->next ){
		/*if ( strcmp(iter->user,user) == 0 ){
			return &iter->banksWritePermissions;
		}*/
	}
	
	return NULL;
}


void clearPasswordList(PasswordList* list){
	Node* iter = list->head ;
	while (iter != NULL){
		Node* tmp = iter->next;
		destroyNode(iter);
		iter=tmp;
	}
	list->size=0;
}

bool removeUser(PasswordList* list,char* user){
	if (getPasswordListSize(list)==0){
		return false;
	}
	
	Node* iter;
	for ( iter = list->head ; iter != NULL ; iter = iter->next ){
		/*if ( strcmp(iter->user,user) == 0 ){
			return false;
		}*/
	}
	

	if (iter != list->head){
		iter->previous->next = iter->next;
	}
	else {
		list->head = iter->next;
	}
	if (iter != list->tail){
		iter->next->previous = iter->previous;
	}
	else {
		list->tail = iter->previous;
	}
	list->size-=1;
	destroyNode(iter);
	
	return true;
}

bool addReadPermissions(PasswordList* list,char* user,int bank){
	if (!checkIfUserExists(list,user)){
		return false;
	}
	
	Node* iter;
	for ( iter = list->head ; iter != NULL ; iter = iter->next ){
		/*if ( strcmp(iter->user,user) == 0 ){
			iter->banksReadPermissions[bank] = 1;
			return true;
		}*/
	}
	
	return false;
}

bool addWritePermissions(PasswordList* list,char* user,int bank){
	if (!checkIfUserExists(list,user)){
		return false;
	}
	
	Node* iter;
	for ( iter = list->head ; iter != NULL ; iter = iter->next ){
		/*
		if ( strcmp(iter->user,user) == 0 ){
			iter->banksWritePermissions[bank] = 1;
			return true;
		}
		*/
	}
	
	return false;
}

bool removeReadPermissions(PasswordList* list,char* user,int bank){
	if (!checkIfUserExists(list,user)){
		return false;
	}
	
	Node* iter;
	for ( iter = list->head ; iter != NULL ; iter = iter->next ){
		/*
		if ( strcmp(iter->user,user) == 0 ){
			iter->banksReadPermissions[bank] = 0;
			return true;
		}
		*/
	}
	
	return false;
}

bool removeWritePermissions(PasswordList* list,char* user,int bank){
	if (!checkIfUserExists(list,user)){
		return false;
	}
	
	Node* iter;
	for ( iter = list->head ; iter != NULL ; iter = iter->next ){
		/*
		if ( strcmp(iter->user,user) == 0 ){
			iter->banksWritePermissions[bank] = 0;
			return true;
		}
		*/
	}
	
	return false;
}

int checkReadPermissions(PasswordList* list,char* user,int bank){
	if (!checkIfUserExists(list,user)){
		return 0;
	}
	
	Node* iter;
	for ( iter = list->head ; iter != NULL ; iter = iter->next ){
		/*
		if ( strcmp(iter->user,user) == 0 ){
			return iter->banksReadPermissions[bank];
		}
		*/
	}
	
	return 0;
}


int checkWritePermissions(PasswordList* list,char* user,int bank){
	if (!checkIfUserExists(list,user)){
		return 0;
	}
	
	Node* iter;
	for ( iter = list->head ; iter != NULL ; iter = iter->next ){
		/*
		if ( strcmp(iter->user,user) == 0 ){
			return iter->banksWritePermissions[bank];
		}
		*/
	}
	
	return 0;
}

int serializePasswordList(PasswordList* list, char** buffer){	
	int bufferIndex = 0; 
	uint8_t sizeOfKey = 0;
	uint8_t sizeOfData = 0;
/*
  
	Node* iter;
	for ( iter = list->head ; iter != NULL ; iter = iter->next ){
		sizeOfKey = strlen(iter->user)+1;
		sizeOfData = strlen(iter->password)+1;
		
		// save size of key
		mem_copy(&((*buffer)[bufferIndex]), &sizeOfKey, sizeof(sizeOfKey));
		bufferIndex += sizeof(sizeOfKey);
		
		// save key
		mem_copy(&((*buffer)[bufferIndex]),&iter->user,sizeOfKey);
		bufferIndex += sizeOfKey;

		//save size of data
		mem_copy(&((*buffer)[bufferIndex]), &sizeOfData, sizeof(sizeOfData));
		bufferIndex += sizeof(sizeOfData);
		
		//save data
		mem_copy(&((*buffer)[bufferIndex]),&iter->password,sizeOfData);
		bufferIndex += sizeOfData;		
		
		//save read permissions
		mem_copy(&((*buffer)[bufferIndex]),&iter->banksReadPermissions,BANKS_AMOUNT*sizeof(int));
		bufferIndex += BANKS_AMOUNT*sizeof(int);		
		
		//save write permissions
		mem_copy(&((*buffer)[bufferIndex]),&iter->banksWritePermissions,BANKS_AMOUNT*sizeof(int));
		bufferIndex += BANKS_AMOUNT*sizeof(int);	
	}
	
  
  return bufferIndex; // return to know the size of the buffer
  // maybe in the end  need to encrypt string? or not needed?
  */
  return 0;
}


/*
PasswordList* deserializePasswordList(char** buffer,int bufferSize){
	
	PasswordList* newPassowrdList = createPasswordList();
	
	int i = 0 ;
	
	int userSize;
	int passwordSize;
	char** user;
	char** password;
	bool** readPermissions;
	bool** writePermissions;
	
	while ( i < bufferSize){
		userSize = buffer[i];
		i += sizeof(uint8_t);
				
		user = &buffer[i];
		i += userSize;
				
		passwordSize=buffer[i];
		i += sizeof(uint8_t);
		
		password = &buffer[i];
		i+= passwordSize;
		
		addNewUser(newPassowrdList,user,password);
		
		readPermissions = &buffer[i];
		i+= BANKS_AMOUNT*sizeof(bool);
		
		for (int j=0;j<BANKS_AMOUNT;j++){
			if (readPermissions[j]==true){
				addReadPermissions(newPassowrdList,user,j);
			}
		}
	
		writePermissions = &buffer[i];
		i+= BANKS_AMOUNT*sizeof(bool);
		
		for (int j=0;j<BANKS_AMOUNT;j++){
			if (writePermissions[j]==true){
				addWritePermissions(newPassowrdList,user,j);
			}
		}
	}

	return newPassowrdList;
}*/
