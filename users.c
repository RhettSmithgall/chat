#include "headers.h"



// Function to create a new User with given value as username
User* createUser(char username[24])
{
    User* newUser = (User*)malloc(sizeof(User));
    strcpy(newUser->username, username);
    newUser->next = NULL;
    newUser->prev = NULL;
    return newUser;
}

// Function to insert a User at the beginning
void insertAtBeginning(User** head, char username[24])
{
    // creating new User
    User* newUser = createUser(username);

    // check if DLL is empty
    if (*head == NULL) {
        *head = newUser;
        return;
    }
    newUser->next = *head;
    (*head)->prev = newUser;
    *head = newUser;
}

// Function to insert a User at the end
void insertAtEnd(User** head, char username[24])
{
    // creating new User
    User* newUser = createUser(username);

    // check if DLL is empty
    if (*head == NULL) {
        *head = newUser;
        return;
    }

    User* temp = *head;
    while (temp->next != NULL) {
        temp = temp->next;
    }
    temp->next = newUser;
    newUser->prev = temp;
}

// Function to insert a User at a specified position
void insertAtPosition(User** head, char username[24], int position)
{
    if (position < 1) {
        printf("Position should be >= 1.\n");
        return;
    }

    // if we are inserting at head
    if (position == 1) {
        insertAtBeginning(head, username);
        return;
    }
    User* newUser = createUser(username);
    User* temp = *head;
    for (int i = 1; temp != NULL && i < position - 1; i++) {
        temp = temp->next;
    }
    if (temp == NULL) {
        printf(
            "Position greater than the number of Users.\n");
        return;
    }
    newUser->next = temp->next;
    newUser->prev = temp;
    if (temp->next != NULL) {
        temp->next->prev = newUser;
    }
    temp->next = newUser;
}

// Function to delete a User from the beginning
void deleteAtBeginning(User** head)
{
    // checking if the DLL is empty
    if (*head == NULL) {
        printf("The list is already empty.\n");
        return;
    }
    User* temp = *head;
    *head = (*head)->next;
    if (*head != NULL) {
        (*head)->prev = NULL;
    }
    free(temp);
}

// Function to delete a User from the end
void deleteAtEnd(User** head)
{
    // checking if DLL is empty
    if (*head == NULL) {
        printf("The list is already empty.\n");
        return;
    }

    User* temp = *head;
    if (temp->next == NULL) {
        *head = NULL;
        free(temp);
        return;
    }
    while (temp->next != NULL) {
        temp = temp->next;
    }
    temp->prev->next = NULL;
    free(temp);
}

void removeUser(User** head, User* delUser)
{
    //checks whether a symbol is in the table already or not
    User* temp = *head;
    while (temp != NULL) {
        if (strcmp(temp->username, delUser->username) == 0) {
            temp->prev->next = temp->next;
            if (temp->next != NULL) {
                temp->next->prev = temp->prev;
            } 
        }
        temp = temp->next;
    }
}

// Function to delete a User from a specified position
void deleteAtPosition(User** head, int position)
{
    if (*head == NULL) {
        printf("The list is already empty.\n");
        return;
    }
    User* temp = *head;
    if (position == 1) {
        deleteAtBeginning(head);
        return;
    }
    for (int i = 1; temp != NULL && i < position; i++) {
        temp = temp->next;
    }
    if (temp == NULL) {
        printf("Position is greater than the number of "
               "Users.\n");
        return;
    }
    if (temp->next != NULL) {
        temp->next->prev = temp->prev;
    }
    if (temp->prev != NULL) {
        temp->prev->next = temp->next;
    }
    free(temp);
}

// Function to print the list in forward direction
void printListForward(User* head)
{
    User* temp = head;
    printf("Forward List: ");
    while (temp != NULL) {
        printf("%d ", temp->username);
        temp = temp->next;
    }
    printf("\n");
}

// Function to print the list in reverse direction
void printListReverse(User* head)
{
    User* temp = head;
    if (temp == NULL) {
        printf("The list is empty.\n");
        return;
    }
    // Move to the end of the list
    while (temp->next != NULL) {
        temp = temp->next;
    }
    // Traverse backwards
    printf("Reverse List: ");
    while (temp != NULL) {
        printf("%d ", temp->username);
        temp = temp->prev;
    }
    printf("\n");
}