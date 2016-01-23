#include "stringList.h"

Node* createNode(char* key,char* data,Node* next,Node* previous){
	Node* newNode = malloc(sizeof(Node));
	if (newNode==NULL){
		return NULL;
	}
	
	newNode->key = key;
	newNode->data = data;
	newNode->next = next;
	newNode->previous = previous;
	return newNode;
}

void destroyNode(Node* node){
	free(node->data);
	node->next=NULL;
	node->previous=NULL;
	free(node);
}

int getStringListSize(StringList* list){
	return list->size;
}


StringList* createStringList(){
	StringList* list = malloc(sizeof(StringList));
	if (list==NULL){
		free(list);
		return NULL;
	}
	
	list->size=0;
	list->head=NULL;
	list->tail=NULL;

    return list;
}

bool isStringListEmpty(StringList* list){
	return list->head == NULL;
}

void appendToStringList(StringList* list,char* key,char* value){
	Node* newNode = createNode(key,value,list->head,NULL);
	if (newNode==NULL){
		return false;
	}
	if (isStringListEmpty(list)){
		list->head = newNode;
		list->tail = newNode;
		return;
	}
	Node* tail = list->tail;
	tail->next = newNode;
	list->tail = newNode;
	list->size+=1;
}

int findIndexInStringList(StringList* list, char* key){
	if (list == NULL || key == NULL){
		return -1;
	}
	
	Node* iter = list->head ;
	for (int i = 0 ; iter != NULL ; iter = iter->next,i++ ){
		if ( strcmp(iter->key,key) ){
			return i;
		}
	}
	
	return -1;
}

void getKeyValueInStringList(StringList* list, int index,char*** key,char*** value){
	if (list == NULL){
		return -1;
	}
	
	
	Node* ptr = list->head;
	for (int i = 0 ; i<index ; i++ ){
		ptr = ptr->next;
	}
	
	if (key != NULL){
		*key = &ptr->key;
	}
	
	if (value != NULL){
		*value = &ptr->data;
	}
}

void clearStringList(StringList* list){
	if (list == NULL){
		return;
	}
	
	Node* iter = list->head ;
	while (iter != NULL){
		Node* tmp = iter->next;
		destroyNode(iter);
		iter=tmp;
	}
	list->size=0;
}

void removeFromStringList(StringList* list,int index){
	if (list == NULL){
		return -1;
	}
	
	
	Node* ptr = list->head;
	for ( int i = 0; i<index ; i++ ){
		ptr = ptr->next;
	}
	

	if (ptr != list->head){
		ptr->previous->next = ptr->next;
	}
	else {
		list->head = ptr->next;
	}
	if (ptr != list->tail){
		ptr->next->previous = ptr->previous;
	}
	else {
		list->tail = ptr->previous;
	}
	list->size-=1;
	destroyNode(ptr);
}
