/*
To compile:
gcc -Wall -Wextra -g Schemas/tests.c Schemas/set_variables.c -o build/tests
*/

#include "set_variables.h"
#include "arraylist.h"
#include <stdio.h>

void test_set_variables(){
    SetVariables set = create_set_defsize();

    for(Variable v = 1; v <= 10; ++v){
        insert_to_set(&set, v);
    }
    print_set(set);

    for(Variable v = 1; v <= 20; ++v){
        char *inSet = lookup_set(set, v) ? "True" : "False";
        printf("%d in set = %s\n", v, inSet);
    }

    clear_set(set);
    print_set(set);

    free_set(set);
}

void test_typeof_or_auto_type(){
    int x = 10;
    __auto_type y = x;
    int *xptr = &x;
    __auto_type yptr = &y; // NOTE: there is no * before yptr
    printf("x=%d - y=%d\n", x, y);
    printf("&x=%p\n&y=%p\n", xptr, yptr);
}

void test_arraylist_ints(){
    ArrayListInt list = create_array_list_int_defsize();

    print_array_list_int(list);

    for(int i = 0; i < 20; ++i){
        add_to_array_list_int(&list, i);
    }

    print_array_list_int(list);

    int first, median, last;
    get_from_array_list_int(list, 0, &first);
    get_from_array_list_int(list, list.size / 2, &median);
    get_from_array_list_int(list, list.size - 1, &last);
    printf("%d - %d - %d\n", first, median, last);

    remove_index_from_array_list_int(&list, 0);
    remove_index_from_array_list_int(&list, list.size / 2);
    remove_index_from_array_list_int(&list, list.size - 1);

    print_array_list_int(list);

    remove_element_from_array_list_int(&list, 7);

    print_array_list_int(list);
}

int main(/*int argc, char const *argv[]*/)
{
    //test_set_variables();
    //test_typeof_or_auto_type();
    test_arraylist_ints();
    return 0;
}
