#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include "stringList.h"
#include "math.h"
#include <stdlib.h>
#include <stdint.h>
#include <time.h>  
#include <string.h>
#include <stdio.h>

#define HASH_INIT_SIZE 11
#define HASH_MULT 10000
#define HASH_FACTOR (sqrt(5)-1)/2

typedef struct hashTable {
   StringList** table;
   int currentSize;
   int numOfFullCells;
   int* T;
} HashTable;

HashTable* createHashTable();
bool isTableFull(HashTable* hashTable);
char** findInHashTable(HashTable* hashTable,char* key);
void insertToHashTable(HashTable* hashTable,char* key,char* value);
int removeFromHashTable(HashTable* hashTable,char* key);
void clearHashTable(HashTable* hashTable);

#endif