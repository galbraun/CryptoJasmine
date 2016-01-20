#ifndef STRING_LIST
#define STRING_LIST

#include "Node.h"

typedef struct node {
    char* key;
	char* data;
    struct node* next;
	struct node* previous;
} Node;

Node* createNode(char* data,struct node* next,struct node* previous);
void destroyNode(Node* node);

typedef struct stringList {
    struct Node* head;
	struct Node* tail;
} StringList;


StringList* createStringList();
void appendToStringList(StringList* list,char* key,char* value);
void removeFromStringList(StringList* list,int index);
void clearStringList(StringList* list);
bool isStringListEmpty(StringList* list);
int findIndexInStringList(StringList* list, char* key);
char** getValueInStringList(StringList* list, int index);

#endif