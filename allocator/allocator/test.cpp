#define _CRT_SECURE_NO_WARNINGS 1

#include"stl_alloc.h"

using namespace my_stl;
int main()
{
	__default_alloc_template<false, 0> alloctor;
	char* p1 = (char*)alloctor.allocate(129);
	char* p2 = (char*)alloctor.allocate(12);
	return 0;
}