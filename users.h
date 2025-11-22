// defining a User
typedef struct User {
    char username[24];
    struct User* next;
    struct User* prev;
} User;

User* createUser(char username[24]);
void insertAtBeginning(User** head, char username[24]);
void insertAtEnd(User** head, char username[24]);
void insertAtPosition(User** head, char username[24], int position);
void deleteAtBeginning(User** head);
void deleteAtEnd(User** head);
void deleteAtPosition(User** head, int position);
void printListForward(User* head);
void printListReverse(User* head);