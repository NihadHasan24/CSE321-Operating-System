#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define MAX_USERS 5
#define MAX_RESOURCES 5
#define MAX_NAME_LEN 20

typedef enum{
    read = 1,
    write = 2,
    execute = 4
} Permission;

typedef struct{
    char name[MAX_NAME_LEN];
} User;

typedef struct{
    char name[MAX_NAME_LEN];
} Resource;

typedef struct{
    char username[MAX_NAME_LEN];
    int permissions;
} ACLEntry;

typedef struct{
    Resource resource;
    ACLEntry acl[MAX_USERS];
    int aclCount;
} ACLControlledResource;

typedef struct{
    char resourceName[MAX_NAME_LEN];
    int permissions;
} Capability;

typedef struct{
    User user;
    Capability capabilities[MAX_RESOURCES];
    int capabilityCount;
} CapabilityUser;

void printPermissions(int perm){
    if (perm == read) printf("READ");
    else if (perm == write) printf("WRITE");
    else if (perm == execute) printf("EXECUTE");
    else if (perm == (read | write)) printf("READ and WRITE");
    else if (perm == (read | execute)) printf("READ and EXECUTE");
    else if (perm == (write | execute)) printf("WRITE and EXECUTE");
    else if (perm == (read | write | execute)) printf("READ, WRITE and EXECUTE");
}

int hasPermission(int userPerm, int requiredPerm){
    return (userPerm & requiredPerm) == requiredPerm;
}

void checkACLAccess(ACLControlledResource *res, const char *userName, int perm){
    for(int i = 0; i < res->aclCount; i++){
        if(strcmp(res->acl[i].username, userName) == 0){
            printf("ACL Check: User %s requests ", userName);
            printPermissions(perm);
            printf(" on %s: ", res->resource.name);
            if(hasPermission(res->acl[i].permissions, perm)){
                printf("Access GRANTED\n");
                return;
            } else {
                printf("Access DENIED\n");
                return;
            }
        }
    }
    printf("ACL Check: User %s has NO entry for resource %s: Access DENIED\n", userName, res->resource.name);
}

void checkCapabilityAccess(CapabilityUser *user, const char *resourceName, int perm){
    for(int i = 0; i < user->capabilityCount; i++){
        if(strcmp(user->capabilities[i].resourceName, resourceName) == 0){
            printf("Capability Check: User %s requests ", user->user.name);
            printPermissions(perm);
            printf(" on %s: ", resourceName);
            if(hasPermission(user->capabilities[i].permissions, perm)){
                printf("Access GRANTED\n");
                return;
            } else {
                printf("Access DENIED\n");
                return;
            }
        }
    }
    printf("Capability Check: User %s has NO capability for %s: Access DENIED\n", user->user.name, resourceName);
}

void addACLEntry(ACLControlledResource *res, const char *userName, int permissions){
    if(res->aclCount < MAX_USERS){
        strcpy(res->acl[res->aclCount].username, userName);
        res->acl[res->aclCount].permissions = permissions;
        res->aclCount++;
    }
}

void addCapability(CapabilityUser *user, const char *resourceName, int permissions){
    if(user->capabilityCount < MAX_RESOURCES){
        strcpy(user->capabilities[user->capabilityCount].resourceName, resourceName);
        user->capabilities[user->capabilityCount].permissions = permissions;
        user->capabilityCount++;
    }
}

int main(){
    User users[MAX_USERS] = {{"Alice"}, {"Bob"}, {"Charlie"}, {"Nihad"}, {"Hasan"}};
    Resource resources[MAX_RESOURCES] = {{"File1"}, {"File2"}, {"File3"}, {"File4"}, {"File5"}};
    ACLControlledResource aclResources[MAX_RESOURCES];
    
    aclResources[0].resource = resources[0];
    strcpy(aclResources[0].acl[0].username, "Alice");
    aclResources[0].acl[0].permissions = read | write;
    strcpy(aclResources[0].acl[1].username, "Bob");
    aclResources[0].acl[1].permissions = read;
    aclResources[0].aclCount = 2;
    
    aclResources[1].resource = resources[1];
    strcpy(aclResources[1].acl[0].username, "Alice");
    aclResources[1].acl[0].permissions = read | write | execute;
    strcpy(aclResources[1].acl[1].username, "Charlie");
    aclResources[1].acl[1].permissions = read | execute;
    aclResources[1].aclCount = 2;
    
    aclResources[2].resource = resources[2];
    strcpy(aclResources[2].acl[0].username, "Bob");
    aclResources[2].acl[0].permissions = write | execute;
    strcpy(aclResources[2].acl[1].username, "Charlie");
    aclResources[2].acl[1].permissions = read;
    aclResources[2].aclCount = 2;
    
    aclResources[3].resource = resources[3];
    strcpy(aclResources[3].acl[0].username, "Nihad");
    aclResources[3].acl[0].permissions = read | write;
    strcpy(aclResources[3].acl[1].username, "Hasan");
    aclResources[3].acl[1].permissions = read;
    aclResources[3].aclCount = 2;
    
    aclResources[4].resource = resources[4];
    strcpy(aclResources[4].acl[0].username, "Hasan");
    aclResources[4].acl[0].permissions = read | write | execute;
    aclResources[4].aclCount = 1;

    CapabilityUser capUsers[MAX_USERS];
    
    capUsers[0].user = users[0];
    strcpy(capUsers[0].capabilities[0].resourceName, "File1");
    capUsers[0].capabilities[0].permissions = read | write;
    strcpy(capUsers[0].capabilities[1].resourceName, "File2");
    capUsers[0].capabilities[1].permissions = read | write | execute;
    capUsers[0].capabilityCount = 2;
    
    capUsers[1].user = users[1];
    strcpy(capUsers[1].capabilities[0].resourceName, "File1");
    capUsers[1].capabilities[0].permissions = read;
    strcpy(capUsers[1].capabilities[1].resourceName, "File3");
    capUsers[1].capabilities[1].permissions = write | execute;
    capUsers[1].capabilityCount = 2;

    capUsers[2].user = users[2];
    strcpy(capUsers[2].capabilities[0].resourceName, "File2");
    capUsers[2].capabilities[0].permissions = read | execute;
    strcpy(capUsers[2].capabilities[1].resourceName, "File3");
    capUsers[2].capabilities[1].permissions = read;
    capUsers[2].capabilityCount = 2;
    
    capUsers[3].user = users[3];
    strcpy(capUsers[3].capabilities[0].resourceName, "File4");
    capUsers[3].capabilities[0].permissions = read | write;
    strcpy(capUsers[3].capabilities[1].resourceName, "File5");
    capUsers[3].capabilities[1].permissions = read;
    capUsers[3].capabilityCount = 2;
    
    capUsers[4].user = users[4];
    strcpy(capUsers[4].capabilities[0].resourceName, "File4");
    capUsers[4].capabilities[0].permissions = read;
    strcpy(capUsers[4].capabilities[1].resourceName, "File5");
    capUsers[4].capabilities[1].permissions = read | write | execute;
    capUsers[4].capabilityCount = 2;

    checkACLAccess(&aclResources[0], "Alice", read);           
    checkACLAccess(&aclResources[0], "Bob", write);            
    checkACLAccess(&aclResources[0], "Charlie", read);        
    
    checkCapabilityAccess(&capUsers[0], "File1", write);     
    checkCapabilityAccess(&capUsers[1], "File1", write);      
    checkCapabilityAccess(&capUsers[2], "File1", write);      
    
    checkACLAccess(&aclResources[1], "Alice", execute);        
    checkACLAccess(&aclResources[1], "Charlie", write);       
    checkACLAccess(&aclResources[3], "Nihad", read);           
    checkACLAccess(&aclResources[3], "Hasan", write);           
    checkACLAccess(&aclResources[4], "Nihad", read);           
    checkACLAccess(&aclResources[2], "Alice", read);        
    
    checkCapabilityAccess(&capUsers[2], "File3", read);        
    checkCapabilityAccess(&capUsers[3], "File4", write);    
    checkCapabilityAccess(&capUsers[4], "File5", execute);   
    checkCapabilityAccess(&capUsers[3], "File5", write);      
    checkCapabilityAccess(&capUsers[0], "File3", read);        
    checkCapabilityAccess(&capUsers[4], "File1", read);        
    
    return 0;
}
