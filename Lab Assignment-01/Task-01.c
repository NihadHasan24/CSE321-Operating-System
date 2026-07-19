#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {

    if (argc != 2) {
        printf("Usage: %s <filename>\n", argv[0]);
        return 1;
    }
    
    char *filename = argv[1];
    FILE *file;
    char input[1000];
    
    file = fopen(filename, "a");
    
    if (file == NULL) {
        printf("Error: Unable to open or create file '%s'\n", filename);
        return 1;
    }
    
    printf("File '%s' opened successfully.\n", filename);
    printf("Enter strings to write to the file (enter '-1' to exit):\n");
    
    while (1) {
        printf("Enter a string or -1 to quit : ");
        
        if (fgets(input, sizeof(input), stdin) != NULL) {

            input[strcspn(input, "\n")] = '\0';
            

            if (strcmp(input, "-1") == 0) {
                break;
            }
            

            fprintf(file, "%s\n", input);
            printf("String written to file.\n");
        } else {
            printf("Error reading input.\n");
            break;
        }
    }
    
    fclose(file);
    printf("File closed. Program terminated.\n");
    
    return 0;
}
