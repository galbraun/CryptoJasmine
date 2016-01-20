#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include "stringList.h"
#include "math.h"

#define HASH_INIT_SIZE 11
#define HASH_MULT 10000
#define HASH_FACTOR (sqrt(5)-1)/2

typedef struct hashTable {
   StringList* table;
   int size;
   int numOfFullCells;
   int* T;
} HashTable;



template<typename T>
class HashTable {

	List<T>* table;
	int currentSize;
	int numOfFullCells;
	int hashing(const int key);
	
public:
	HashTable() :currentSize(INIT_SIZE), numOfFullCells(0){
		table = new List<T>[currentSize];
		for (int i = 0; i < currentSize; i++){
			table[i] = List<T>();
		}
	}
	~HashTable(){
		table->clear();
	}
	void enlargeTable();
	bool isTableFull();
	T& find(const int key);
	void insert(const int key,const T& node);
	void remove(const int key);
};



template<typename T>
T& HashTable<T>::find(const int key){
	List<T> currentList = table[this->hashing(key)];
	for (typename List<T>::Iterator it = currentList->begin(); !(it.isNull()); it++){
		if ((*it) == key){
			return (*it);
		}
	}
	throw hashValueNotExists();
}

template<typename T>
void HashTable<T>::insert(const int key,const T& node){
	try{
		List<T> currentList = table[this->hashing(key)];
		bool wasEmpty = currentList.isEmpty();
		currentList.insertAfterTail(node);
		if (wasEmpty){
			numOfFullCells++;
		}
	}
	catch (const valueAlreadyExists& e){
		throw hashValueAlreadyExists();
	}
}

template<typename T>
void HashTable<T>::remove(const int key){
	List<T> currentList = table[this->hashing(key)];
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

#endif