/*
Reads a file of lines that contain "address, instruction, size" and simulates a
cache, recording the amount of hits, misses, and convictions

@author Casey Goldberg
*/

#include "cachelab.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <stdlib.h>
#include <getopt.h>

/*
Defines a cache_line_t structure, which contains data and various information
associated with the cache line.
*/
typedef struct{
    int valid;
    int dirty;
    unsigned long long tag;
    unsigned int age;
    uint8_t *data; 
} cache_line_t;

/*
Defines a cache_set structure, which containes an array of cache lines.  
*/
typedef struct{
    cache_line_t *lines; 
} cache_set_t;

/*
Defines a cache_t structure, which contains an array of sets and various information
associated with the cache.
*/
typedef struct{
    int set_bits;
    int associativity;
    int block_bits;
    
    int num_sets;
    int num_blocks;

    int miss_count;
    int hit_count;
    int evict_count;

    int flag_v_set;
    int instr_count;

    cache_set_t *set_arry;
} cache_t;

/*
This function takes in a cache line and initializes all information associated with a cache line 
structure to 0. It also initialzes the data to NULL (we're not actually working wiith data in
this simulation).
*/
void make_cache_line (cache_line_t *line){
    line -> valid = 0;
    line -> dirty = 0;
    line -> tag = 0; 
    line -> age = 0;
    line -> data = NULL;
}

/*
This function takes in a cache set and the number of lines per set for our cache. If the set exists,
it iterates through each line in the set, populating it by calling make_cache_line.
*/
void make_cache_set(cache_set_t *set, int associativity){
    if (set != NULL){
        set -> lines = malloc(sizeof(cache_line_t) * associativity);
        for(int line_n = 0; line_n < associativity; line_n++){
            make_cache_line(&(set->lines[line_n]));
        }
    }
}

/*
This function takes in the number of set bits, the number of lines per sets, the number of block
bits, and whether the verbose flag was set. It creates a cache structure, initializes information
associated with the cache based on the arguments. Then, it iterates through each set in the cache,
populating it by called make_cache_set. It then returns this newly populated cache.
*/
cache_t *make_cache(int set_bits, int associativity, int block_bits, int flag_v_set){
    cache_t *cache = (cache_t *)malloc(sizeof(cache_t));
    cache -> set_bits = set_bits;
    cache -> associativity = associativity;
    cache -> block_bits = block_bits;
    cache -> miss_count = 0;
    cache -> hit_count = 0;
    cache -> evict_count = 0;
    cache -> num_sets = pow(2, set_bits) - 1;
    cache -> num_blocks = pow(2, block_bits) - 1;
    cache -> flag_v_set = flag_v_set;
    cache -> instr_count = 0;
    cache -> set_arry = (cache_set_t *)malloc(sizeof(cache_set_t) * (cache -> num_sets + 1));
    if (cache -> set_arry != NULL){
        for(int set_n = 0; set_n <= cache -> num_sets; set_n++){
            make_cache_set(&(cache -> set_arry[set_n]), associativity);
        }
    }
    return cache;
}

/*
This function takes in a cache and prints out the cache's relevant information in a
convenient format.
*/
void show_cache(cache_t *cache){
    for (int set_n = 0; set_n <= cache -> num_sets; set_n++){
        cache_set_t *set = &(cache -> set_arry[set_n]);
        printf("set %d:   ", set_n);
        if(set_n < 10){
            printf(" ");
        }
        for (int line_n = 0; line_n < cache -> associativity; line_n++){
            cache_line_t *line = &(set -> lines[line_n]);
            printf("v = %d, t = %llx, a = %d   |   ", line -> valid, line -> tag, line -> age);
        }
        printf("\n");
    }
}

/*
This function takes in an address and a cache. It uses user input about the size of the cache 
and logical operators to return the set number associated with the address.
*/
int set_for(int address, cache_t *cache){
    int set_mask = (cache -> num_sets) << (cache -> block_bits);
    int result = (set_mask & address) >> (cache -> block_bits);
    return result;
}

/*
This function takes in an address and a cache. It uses user input about the size of the cache 
and logical operators to return the tag associated with the address.
*/
int tag_for(int address, cache_t *cache){
    int sets_and_blocks = (cache -> num_blocks) | ((cache -> num_sets) << cache -> block_bits);
    int tag_mask = ~sets_and_blocks;
    return (tag_mask & address) >> (cache -> set_bits + cache -> block_bits);
}

/*
Given a cache and the set and tag of the address we're looking up, this function iterates through
the lines in the set, looking for the correct tag. If the tag matches and the data is valid, the
function increments the hit count and returns 1. If no tag match and valid data are found, the 
function returns 0.
*/
int check_for_hit(cache_t *cache, cache_set_t *correct_set, int my_tag){
    for(int line_n = 0; line_n < cache -> associativity; line_n++){
        cache_line_t *line_of_int = &(correct_set -> lines[line_n]);
        if(line_of_int -> tag == my_tag && line_of_int -> valid){
            line_of_int -> age = cache -> instr_count;
            cache -> hit_count++;
            if(cache -> flag_v_set){
                printf("hit ");
            }
            return 1;
        }
    }
    return 0;
}

/*
The function takes in a cache, the oldest line in the set of the address 
of interest, and the tag associated with the address of interest. It then 
replaces the oldest line's tag with the new tag and increments the eviction
count.
*/
void evict(cache_t *cache, cache_line_t *oldest_line, int my_tag){
    cache -> evict_count++;
    if(cache -> flag_v_set){
        printf("eviction ");
    }
    oldest_line -> tag = my_tag;
    oldest_line  -> age = cache -> instr_count;
}

/*
This function takes in a cache and the set and tag of the address we're looking up. 
It increments the miss count. Then, it iterates through the lines in the set, keeping
track of the oldest line in the set in case an eviction is necessary. If the function
finds a line that hasn't had data in it yet (isn't valid), it puts the tag in that line
of the set and returns. If not, it calls the evict function.
*/
void miss(cache_t *cache, cache_set_t *correct_set, int my_tag){
    cache -> miss_count++;
    if(cache -> flag_v_set){
    printf("miss ");
    }
    cache_line_t *oldest_line = &(correct_set -> lines[0]); 
    for(int line_n = 0; line_n < cache -> associativity; line_n++){
        cache_line_t *line_of_int = &(correct_set -> lines[line_n]);
        //checks for oldest line
        if(line_of_int -> age < oldest_line -> age){
            oldest_line = line_of_int;
        }
        //no eviction
        if(!(line_of_int -> valid)){
            line_of_int -> tag = my_tag;
            line_of_int -> valid = 1;
            line_of_int -> age = cache -> instr_count;
            return;
        }
    }
    evict(cache, oldest_line, my_tag);
}

/*
This function takes in the cache and the address that we're looking for. It calls helper
functions to extract the tag and set associated with the address. First, it checks for a
hit. If the address isn't already in the cache (check_for_hit returns 0), it calls the 
helper function that is run when there's a cache miss.
*/
void lookup(int address, cache_t *cache){
    cache -> instr_count++;
    int my_tag = tag_for(address, cache);
    int my_set = set_for(address, cache);
    cache_set_t *correct_set = &(cache -> set_arry[my_set]);

    int is_hit = check_for_hit(cache, correct_set, my_tag);
    if (!is_hit){
        miss(cache, correct_set, my_tag);
    }
}

/*
Given a cache, this function frees the allocated data that makes up the cache.
*/
void free_cache(cache_t *cache){
    for (int set_n = 0; set_n <= cache -> num_sets; set_n++){
        cache_set_t *set = &(cache -> set_arry[set_n]);
        free(set -> lines);
    }
    free(cache -> set_arry);
    free(cache);
}

/*
Given the cache and a filename, this function tries to open the user-inputted
file. If it cannot, it prints an errort message and frees the cache.
*/
FILE* open_file(cache_t *cache, char *filename){
    FILE* file;
    file = fopen(filename, "r");
    if (!file) {
        printf("File can't be opened \n");
        free_cache(cache);
        exit(1);
    }
    return file;
}

/*
Given an address, an operation, and the cache, the function calls lookup twice if
the operation is modify, and calls lookup once otherwise.
*/
void call_lookup(int int_address, char operation, cache_t *cache){
    lookup(int_address, cache);
    if(operation == 'M'){
        lookup(int_address, cache);
    }
    if(cache -> flag_v_set){
        printf("\n");
    } 
}

/*
Given a cache and a filename, this function reads a file of cache access operations
and calls a function to look them up.
*/
void read_file(cache_t *cache, char *filename){
    FILE* file = open_file(cache, filename);
    char address_line[256]; 
    while(fgets(address_line, sizeof(address_line), file)){
        if(address_line[0] == ' '){
            char operation = address_line[1];
            if(cache -> flag_v_set){
                printf("%c ", address_line[1]);
            }
            int idx = 2;
            char address[] = "";
            while(address_line[idx] != ','){
                strncat(address, &address_line[idx], 1);
                idx++;
            }
            int int_address;
            sscanf(address, "%x", &int_address);
            if(cache -> flag_v_set){
                printf("%x%c%c ", int_address, address_line[idx], address_line[idx + 1]);
            }
            call_lookup(int_address, operation, cache);
        }
    } 
    fclose(file);
}

/*
Prints an error message if the user inputs the wrong flags and/or arguments.
*/
void printUsageMsg(){
    printf("Error - Expected Arguments: verbose(optional), set blocks, associativity, \
    block bits, trace file name\n");
    exit(1);
}


int main(int argc, char **argv){
    int flag_v_set = 0;
    int flag_s_value = 0;
    int flag_E_value = 0;
    int flag_b_value = 0;
    char *flag_t_str = "";
    char c;
    while ((c = getopt(argc, argv, "vs:E:b:t:")) != -1) { 
        switch (c) {
        case 'v':
            flag_v_set = 1;
            break;
        case 's':
            flag_s_value = atoi(optarg);
            break;
        case 'E':
            flag_E_value = atoi(optarg);
            break;
        case 'b':
            flag_b_value = atoi(optarg);
            break;
        case 't':
            flag_t_str = optarg;
            break;
        default:
            printUsageMsg();
        }
    }
    
    cache_t *mycache = make_cache(flag_s_value, flag_E_value, flag_b_value, flag_v_set);
    read_file(mycache, flag_t_str);
    //show_cache(mycache); uncomment to show cache
    printSummary(mycache -> hit_count, mycache -> miss_count, mycache -> evict_count);
    free_cache(mycache);
    return 0;
}