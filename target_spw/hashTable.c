#include "hashTable.h"

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
		table[i] = createStringList();
	}
	
	newHashTable=randomAsciiArray();
	
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

static void hashingHashTable(HashTable* hashTable,const int key){
	int h = 0;
	char *p;
	for (p=s; *p; p++){
		h = (hashTable->T[h])^ *p; /* Xor */
	}
	return h;
}


static void swapIfDiff(int* a, int* b) {
    if (a != b) {
		int tmp = *a;
        *a = *b;
		*b = *tmp
    }
}

int* randomAsciiArray(void){
	int* T = malloc(sizeof(int)*256)

    for (int i = 0; i < 256; i++){
        T[i] = i;
	}

    srand(time(NULL));

    for (int i = 0; i < 256; i++){
        swapIfDiff(&T[i], &T[rand() % 256]);
	}

    return T;
}


char** findInHashTable(HashTable* hashTable,char* key){
	StringList* currentList = hashTable->table[hashingHashTable(key)];
	int index = findIndexInStringList(currentList,key);
	if (index == -1 ){
		return NULL;
	}
	return getValueInStringList(currentList,index);
}


void insertToHashTable(HashTable* hashTable,char* key,char* value){
	StringList* currentList = table[hashingHashTable(key)];
//	bool isEmpty = currentList.isEmpty();
	appendToStringList(currentList,key,value);
//	if (isEmpty){
//		hashTable->numOfFullCells++;
//	}
}

void removeFromHashTable(HashTable* hashTable,char* key){
	StringList* currentList = table[hashingHashTable(key)];
	for (typename List<T>::Iterator it = currentList->begin(); !(it.isNull()); it++){
		if ((*it) == key){
			currentList.remove((*it));
			if (currentList.isEmpty()){
				numOfFullCells--;
			}
		}
	}
	throw hashValueNotExists();
}

