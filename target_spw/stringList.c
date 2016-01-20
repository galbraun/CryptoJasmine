#include "stringList.h"

Node* createNode(char* key,char* data,Node* next,Node* previous){
	Node* newNode = malloc(sizeof(Node))
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


StringList* createStringList(){
	StringList* list = malloc(sizeof(StringList));
	if (list==NULL){
		free(list);
		return NULL;
	}
	
	list->head=NULL:
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
	list->tail->next = newNode;
	list->tail = newNode;
}

int findIndexInStringList(StringList* list, char* key){
	if (list == NULL || str == NULL){
		return -1;
	}
	
	for (int i = 0 , Node* iter = list->head ; iter != NULL ; iter = iter->next,i++ ){
		if ( strcmp(iter->key,key) ){
			return i;
		}
	}
	
	return -1;
}

char** getValueInStringList(StringList* list, int index){
	if (list == NULL){
		return -1;
	}
	
	for (int i = 0, Node* ptr = list->head ; i<index ; i++ ){
		ptr = ptr->next;
	}
	
	return &ptr->data;
}

void clearStringList(StringList* list){
	if (list == NULL){
		return;
	}
	
	Node* iter = list->head ;
	while (iter != NULL){
		tmp = iter->next;
		destroyNode(iter);
		iter=tmp;
	}
}

void removeFromStringList(StringList* list,int index){
	if (list == NULL){
		return -1;
	}
	
	for (int i = 0, Node* ptr = list->head ; i<index ; i++ ){
		ptr = ptr->next;
	}
	

	if (ptr != head){
		ptr->previous->next = ptr->next;
	}
	else {
		list->head = ptr->next;
	}
	if (ptr != tail){
		ptr->next->previous = ptr->previous;
	}
	else {
		list->tail = ptr->previous;
	}
	
	destroyNode(ptr);
}

