#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Label
{
    char name[50];
    int address;
    struct Label *next;
} Label;

Label *head = NULL;

void addLabel(char *name, int address)
{
    Label *temp;

    temp = (Label *)malloc(sizeof(Label));

    if (temp == NULL)
    {
        printf("Memory allocation failed\n");
        exit(1);
    }

    strcpy(temp->name, name);
    temp->address = address;
    temp->next = head;
    head = temp;
}

int getAddress(char *name)
{
    Label *temp;

    temp = head;

    while (temp != NULL)
    {
        if (strcmp(temp->name, name) == 0)
            return temp->address;

        temp = temp->next;
    }

    printf("Error: Label %s not found\n", name);
    exit(1);
}

void freeLabels()
{
    Label *temp;

    while (head != NULL)
    {
        temp = head;
        head = head->next;
        free(temp);
    }
}