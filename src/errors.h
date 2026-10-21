#ifndef ERRORS_H
#define ERRORS_H



#include <stdio.h>



// #define ENABLE_PRINT_MSG
#define ENABLE_PRINT_ERR

#ifdef ENABLE_PRINT_MSG
#define PRINT_MSG(...) fprintf(stdout, __VA_ARGS__)
#else
#define PRINT_MSG(...)
#endif

#ifdef ENABLE_PRINT_ERR
#define PRINT_ERR(...) fprintf(stderr, __VA_ARGS__)
#else
#define PRINT_ERR(...)
#endif

#define ERR_CODE_CREATION_FAILED 1
#define ERR_CODE_CONNECTION_FAILED 2
#define ERR_CODE_ACCESS_FAILED 3
#define ERR_CODE_RUNTIME 4

#define PRINT_ERR_CREATION_FAILED(target_description) PRINT_ERR("Error: Creation of %s failed!\n", (target_description))
#define PRINT_ERR_CONNECTION_FAILED(target_description) PRINT_ERR("Error: Connection to %s failed!\n", (target_description))
#define PRINT_ERR_ACCESS_FAILED(target_description) PRINT_ERR("Error: Access of %s failed!\n", (target_description))
#define PRINT_ERR_RUNTIME(error_description) PRINT_ERR("Error: %s!\n", (error_description))



#endif