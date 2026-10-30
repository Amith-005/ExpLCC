%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "translate.c"

FILE *input_file;
FILE *output_file;

int address;
int pass;

char instruction[50];
char reg[50];
char label[100];
%}

%%

^[A-Za-z][A-Za-z0-9]*:[ \t\r]*\n {
    /* label line: record its address, emit nothing (a blank line would be loaded as an instruction) */
    if (pass == 1)
    {
        yytext[strcspn(yytext, ":")] = '\0';
        addLabel(yytext, address);
    }
}

JZ[ \t]+R[0-9]+,[ \t]*[A-Za-z][A-Za-z0-9]* {
    if (pass == 2)
    {
        sscanf(yytext, "%s %[^,], %s", instruction, reg, label);

        fprintf(output_file, "%s %s, %d",
                instruction,
                reg,
                getAddress(label));
    }

    address += 2;
}

MOV[ \t]+R[0-9]+,[ \t]*F[0-9]+ {
    /* method label loaded into a register (virtual function table set-up) */
    if (pass == 2)
    {
        sscanf(yytext, "%s %[^,], %s", instruction, reg, label);

        fprintf(output_file, "%s %s, %d",
                instruction,
                reg,
                getAddress(label));
    }

    address += 2;
}

JMP[ \t]+[A-Za-z][A-Za-z0-9]* {
    if (pass == 2)
    {
        sscanf(yytext, "%s %s", instruction, label);

        fprintf(output_file, "%s %d",
                instruction,
                getAddress(label));
    }

    address += 2;
}

CALL[ \t]+R[0-9]+ {
    /* indirect call through a register (virtual function table dispatch) */
    if (pass == 2)
    {
        fprintf(output_file, "%s", yytext);
    }

    address += 2;
}

CALL[ \t]+[A-Za-z][A-Za-z0-9]* {
    if (pass == 2)
    {
        sscanf(yytext, "%s %s", instruction, label);

        fprintf(output_file, "%s %d",
                instruction,
                getAddress(label));
    }

    address += 2;
}

CALL[ \t]+[0-9]+ {
    if (pass == 2)
    {
        fprintf(output_file, "%s", yytext);
    }

    address += 2;
}

[^\n]+ {
    if (pass == 2)
    {
        fprintf(output_file, "%s", yytext);
    }

    address += 2;
}

\n {
    if (pass == 2)
    {
        fprintf(output_file, "\n");
    }
}

%%

void firstPass()
{
    address = 2056;
    pass = 1;

    yyin = input_file;

    yylex();
}

void secondPass()
{
    address = 2056;
    pass = 2;

    yyin = input_file;

    yylex();
}

int main(int argc, char *argv[])
{
    char line[500];
    int i;

    if (argc != 2)
    {
        printf("Usage: %s <input-file>\n", argv[0]);
        return 1;
    }

    input_file = fopen(argv[1], "r");

    if (input_file == NULL)
    {
        perror("Cannot open input file");
        return 1;
    }

    output_file = fopen("output.xsm", "w");

    if (output_file == NULL)
    {
        perror("Cannot open output file");
        fclose(input_file);
        return 1;
    }

    for (i = 0; i < 8; i++)
    {
        if (fgets(line, sizeof(line), input_file) == NULL)
        {
            printf("Invalid input file\n");
            fclose(input_file);
            fclose(output_file);
            return 1;
        }
    }

    firstPass();

    rewind(input_file);

    for (i = 0; i < 8; i++)
    {
        if (fgets(line, sizeof(line), input_file) == NULL)
        {
            printf("Invalid input file\n");
            fclose(input_file);
            fclose(output_file);
            return 1;
        }

        fprintf(output_file, "%s", line);
    }

    secondPass();

    fclose(input_file);
    fclose(output_file);

    freeLabels();
    return 0;
}

int yywrap()
{
    return 1;
}