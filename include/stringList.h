#ifndef STRING_LIST
#define STRING_LIST

#include <stdbool.h>
#include <stdlib.h>

typedef struct node {
    char* key;
	char* data;
    struct node* next;
	struct node* previous;
} Node;

typedef struct stringList {
    struct Node* head;
	struct Node* tail;
	int size;
} StringList;

Node* createNode(char* key,char* data,Node* next,Node* previous);
void destroyNode(Node* node);




StringList* createStringList();
bool appendToStringList(StringList* list,char* key,char* value);
void removeFromStringList(StringList* list,int index);
void clearStringList(StringList* list);
bool isStringListEmpty(StringList* list);
int findIndexInStringList(StringList* list, char* key);
void getKeyValueInStringList(StringList* list, int index,char*** key,char*** value);
int getStringListSize(StringList* list);

#endif