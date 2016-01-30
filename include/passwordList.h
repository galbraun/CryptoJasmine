#ifndef PASSWORD_LIST
#define PASSWORD_LIST

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>

#define BANKS_AMOUNT 32

typedef struct node {
    char* user;
	char* password;
	bool banksReadPermissions[BANKS_AMOUNT];
	bool banksWritePermissions[BANKS_AMOUNT];
    struct node* next;
	struct node* previous;
} Node;

typedef struct passwordList {
    struct Node* head;
	struct Node* tail;
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
bool checkReadPermissions(PasswordList* list,char* user,int bank);
bool checkWritePermissions(PasswordList* list,char* user,int bank);

char** getUserPasswordPointer(PasswordList* list,char* user);
bool** getUserReadPermissionsPointer(PasswordList* list,char* user);
bool** getUserWritePermissionsPointer(PasswordList* list,char* user);


#endif