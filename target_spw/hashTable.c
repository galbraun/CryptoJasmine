#include "hashTable.h"

int hashingHashTable(HashTable* hashTable,char* key){
	int h = 0;
	char *p;
	for (p=key; *p; p++){
		h = (hashTable->T[h])^ *p; /* Xor */
	}
	return h % hashTable->currentSize;
}


void swapIfDiff(int* a, int* b) {
    if (a != b) {
		int tmp = *a;
        *a = *b;
		*b = tmp;
    }
}


int* randomAsciiArray(){
	int* T = malloc(sizeof(int)*256);

    for (int i = 0; i < 256; i++){
        T[i] = i;
	}

    srand(time(NULL));

    for (int i = 0; i < 256; i++){
        swapIfDiff(&T[i], &T[rand() % 256]);
	}

    return T;
}


HashTable* createHashTable(){
	HashTable* newHashTable = malloc(sizeof(HashTable));
	if (newHashTable==NULL){
		return NULL;
	}
	
	newHashTable->currentSize = HASH_INIT_SIZE;
	newHashTable->numOfFullCells = 0;
	
	newHashTable->table = malloc(sizeof(StringList*)*newHashTable->currentSize);
	if (newHashTable->table==NULL){
		return NULL;
	}
	
	for (int i = 0; i < newHashTable->currentSize ; i++){
		newHashTable->table[i] = createStringList();
	}
	
	newHashTable->T=randomAsciiArray();
	
	return newHashTable;
}

bool isTableFull(HashTable* hashTable){
	return hashTable->currentSize == hashTable->numOfFullCells;
}

/*
bool enlargeTable(HashTable* hashTable){
	newTable = malloc(sizeof(StringList*)*newHashTable->currentSize*2);
	if (newHashTable->table==NULL){
		return false;
	}
	
	for (int i=0 ; i < hashTable->currentSize*2 ; i++ ){
		if ( i < hashTable->currentSize){
			newTable[i] = hashTable->table[i];
			hashTable->table[i] = NULL;
		}
		else {
			newTable[i] = NULL;
		}
	}
	
	free(hashTable->table);
	hashTable->table=newTable;
	hashTable->currentSize *= 2;
	
	return true;
}*/

// TODO: build reduce ?

char** findInHashTable(HashTable* hashTable,char* key){
	StringList* currentList = hashTable->table[hashingHashTable(hashTable,key)];
	int index = findIndexInStringList(currentList,key);
	if (index == -1 ){
		return NULL;
	}
	char** value;
	getKeyValueInStringList(currentList,index,NULL,&value);
	return value;
}


void insertToHashTable(HashTable* hashTable,char* key,char* value){
	StringList* currentList = hashTable->table[hashingHashTable(hashTable,key)];
//	bool isEmpty = currentList.isEmpty();
	appendToStringList(currentList,key,value);
//	if (isEmpty){
//		hashTable->numOfFullCells++;
//	}
}

int removeFromHashTable(HashTable* hashTable,char* key){
	StringList* currentList = hashTable->table[hashingHashTable(hashTable,key)];
	int index = findIndexInStringList(currentList,key);
	if (index != -1){
		removeFromStringList(currentList,index);
	}
	return index;
}

void clearHashTable(HashTable* hashTable){
	for (int i = 0; i < hashTable->currentSize ; i++){
		clearStringList(hashTable->table[i]);
		free(hashTable->table[i]);
	}
	
	free(hashTable->T);
	free(hashTable);
}

int serializeTable(HashTable* hashTable, char** buffer){	
  int bufferIndex = 0; 
  uint8_t sizeOfKey = 0;
  uint8_t sizeOfData = 0;
  char** key;
  char** data;
  
  for (int i=0;i<hashTable->currentSize;i++){
		for (int j=0;j<getStringListSize(hashTable->table[i]);j++){
			getKeyValueInStringList(hashTable->table[i],j,&key,&data);
			sizeOfKey = strlen(*key);
			sizeOfData = strlen(*data);
			
			memcpy(buffer[bufferIndex], &sizeOfKey, sizeof(sizeOfKey));
			bufferIndex += sizeof(uint8_t);
			memcpy(buffer[bufferIndex],key,sizeOfKey);
			bufferIndex += sizeOfKey;
			memcpy(buffer[bufferIndex], &sizeOfData, sizeof(sizeOfData));
			bufferIndex += sizeof(uint8_t);
			memcpy(buffer[bufferIndex],data,sizeOfData);
			bufferIndex += sizeOfData;
		}
  }
  
  return bufferIndex; // return to know the size of the buffer
  // maybe in the end  need to encrypt string? or not needed?
  
}



/*
HashTable* deserializeTable(char* buffer,int bufferSize){
	
	HashTable* newHashTable = createHashTable();
	
	while ()
	
	for (int i=0;i<bufferSize;i++){
		char** 
	}
}
*/
