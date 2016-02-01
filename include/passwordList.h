#ifndef PASSWORD_LIST
#define PASSWORD_LIST

#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include "jasmine.h"

#define BANKS_AMOUNT 32

typedef struct node {
    char* user;
	char* password;
	int banksReadPermissions[BANKS_AMOUNT];
	int banksWritePermissions[BANKS_AMOUNT];
    struct node* next;
	struct node* previous;
} Node;

typedef struct passwordList {
    Node* head;
	Node* tail;
	int size;
} PasswordList;

Node* createNode(char* user,char* password,Node* next,Node* previous);
void destroyNode(Node* node);


PasswordList* createPasswordList();
int getPasswordListSize(PasswordList* list);
void clearPasswordList(PasswordList* list);

bool addNewUser(PasswordList* list,char* user,char* password);
bool removeUser(PasswordList* list,char* user);
bool checkIfUserExists(PasswordList* list,char* user);
bool addReadPermissions(PasswordList* list,char* user,int bank);
bool addWritePermissions(PasswordList* list,char* user,int bank);
bool removeReadPermissions(PasswordList* list,char* user,int bank);
bool removeWritePermissions(PasswordList* list,char* user,int bank);
int checkReadPermissions(PasswordList* list,char* user,int bank);
int checkWritePermissions(PasswordList* list,char* user,int bank);

char** getUserPasswordPointer(PasswordList* list,char* user);
int** getUserReadPermissionsPointer(PasswordList* list,char* user);
int** getUserWritePermissionsPointer(PasswordList* list,char* user);


#endif