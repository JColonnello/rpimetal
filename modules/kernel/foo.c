////////////////////////////////////////////////////////////////////////////////
//  change these a little bit for different behavior
//
////////////////////////////////////////////////////////////////////////////////
#include <stdio.h>
#include <stdlib.h>
#include <sys/reent.h>

//// callbacks

// a function to be used as callback
int my_callback_01(int a)
{
	// printf("my_callback_01 called!\n");
	return a * 2;
}

int my_callback_02(int a)
{
	// printf("my_callback_02 called!\n");
	return a * 4;
}

__thread int tls_int = 3, *module_data_ptr;
int my_callback_03(int a)
{
	// printf("my_callback_03 called! Using %p\n", module_data_ptr);
	return a * *module_data_ptr;
}

typedef int (*t_callback)(int);
typedef void (*t_test_function)(int, int *);

// test_unit.o is expected to call a function with the name "callback";
// we will relocate those calls to the address in the my_callback variable
__attribute__((alias("my_callback_03")))
int callback(int);
// our job is to load the binary code of the object file into memory,
// then find the address of the function with the following name
const char *test_function_name = "test_function_02";
// store it on the following pointer:
// and then execute it on the two arguments "in" and "out":
int in = 10;
int out[4];

// this pointer will eventually store the address of the function in test_unit.o with the name test_function_name
t_test_function test_function;

int main()
{
	// module_data_ptr = module;
	// test(in, out);
	printf("out = { %d, %d, %d, %d }\n", out[0], out[1], out[2], out[3]);
	printf("Done!\n");
}