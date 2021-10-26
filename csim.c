#include "cachelab.h"
#include <stdlib.h>
#include <getopt.h>
#include <strings.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#define MEMADDR_BITSIZE 64
#define DEBUG_FILE_PATH "csim-debug.log"

#define ERROR_OPEN_FILE 2
#define ERROR_CACHE_LINE_NOT_FOUND 3

typedef struct
{
    int hitcount; 
    int misscount; 
    int evictcount; 
    int dirty_evicted;
    int dirty_active; 
    int double_accesses;
} metrics_t;

//Struct for cache parameters 
typedef struct 
{
    int s; //2^s cache sets 
    int b; //2^b bytes per line for cache block 
    int E; //cache lines per set 
    int S; //S = 2^s, number of cache sets 
    int B; //B = 2^b, cache line block size 
    int t; //number of bits in tag = 64 - s - b    

    metrics_t metrics;

    unsigned long long counter; // use for LRU
} param_t; 

typedef unsigned long long memaddr_t; 

//struct to represent all data in a set line in a cache 
typedef struct 
{
    int dirtybit;
    int validbit; 
    memaddr_t tag;
    char *block; 
    
    unsigned long long access; 
} cache_line_t; 

//pointer for lines in cache to singular line 
typedef struct 
{
    cache_line_t *lines;
    cache_line_t* last_accessed;
} cache_set_t; 

//pointer for sets in cache to lines 
typedef struct 
{
    cache_set_t *sets; 
} cache_t;

int verbose = 0; 

//usage 
void
printUsage()
{
    printf("Usage: ./csim [-h] [-v] -s <s> -E <E> -b <b> -t <tracefile>\n");
    printf("-s: set index number\n");
    printf("-v: verbose log\n");
    printf("-E: lines per set\n");
    printf("-b: block offset bits\n");
    printf("-t: trace file name\n");
}

int
init(
    long long num_sets,
    int num_lines,
    cache_t* cache
) {
    int result = 0;
    int i=0; //set index
 
    cache->sets = (cache_set_t *) malloc(sizeof(cache_set_t) * num_sets); 

    for (; i < num_sets; i++)
    {
        int j=0; //line index 
        cache->sets[i].lines =  
                (cache_line_t *) malloc(sizeof(cache_line_t) * num_lines);
        cache->sets[i].last_accessed = NULL;

        for (; j < num_lines; j++)
        {
            cache_line_t* line = &cache->sets[i].lines[j];
            line->access = 0; 
            line->validbit = 0; 
            line->tag = -1;
        }
    }

    return result; 
}

void 
free_cache(
    cache_t* cache, 
    long long num_sets,
    int num_lines
) {
    if (cache != NULL)
    {
        if (cache->sets != NULL)
        {
            int i = 0; //set index
            for (; i < num_sets; i++)
            {
                cache_set_t* set = &cache->sets[i];
                if (set != NULL)
                {
                   if (set->lines != NULL) 
                   {
                       free(set->lines);
                   }
                   free(set);
                }
            }
            free(cache->sets);
        }
        free(cache);
    }
}

memaddr_t 
getTag(
    memaddr_t value, 
    int s,
    int b
) {
    return (value >> (s + b));
}

memaddr_t 
getCacheSetIndex(
    memaddr_t value, 
    int s,
    int b
) {
    return (value << (MEMADDR_BITSIZE - s - b)) >> (MEMADDR_BITSIZE - s);
}

cache_set_t*
getCacheSet(
    memaddr_t memaddr, 
    param_t* params, 
    cache_t* cache
){
    memaddr_t index = getCacheSetIndex(memaddr,params->s,params->b);
    //printf("cache line: %llu\n",index);
    return &cache->sets[index];
}

void
setLastAccessed(
    cache_set_t* set,
    cache_line_t* cache_line
) {
    set->last_accessed = cache_line;
}

int
isDirty(
    cache_t* cache,
    param_t* params
) {
    int i = 0; //set index
    for (; i < params->S; i++)
    {
        cache_set_t* set = &cache->sets[i];
        for (int j = 0; j < params->E;j++)
        {
            cache_line_t* cache_line = &set->lines[j];
            if(cache_line->dirtybit == 1) {
            return 1;
            }
        }
    }
    return 0;
}

void
addMetrics(
     metrics_t* total,
     metrics_t* metrics
) {
    total->hitcount += metrics->hitcount;
    total->misscount += metrics->misscount; 
    total->evictcount += metrics->evictcount; 
    total->dirty_evicted += metrics->dirty_evicted;
    total->dirty_active += metrics->dirty_active; 
    total->double_accesses += metrics->double_accesses;
}

int
loadCache(
    cache_t* cache, 
    param_t* params, 
    memaddr_t memaddr,
    metrics_t* metrics
){
    int result = 0;
    cache_line_t* match = NULL;
    memaddr_t tag = getTag(memaddr,params->s,params->b);  
    cache_set_t* set = getCacheSet(memaddr, params, cache);
    //printf("Tag value: %llu\n",tag);
    //check for hit
    //locate a cache line with matching tag
    for (int i = 0; i < params->E; i++) {
        //go to index, see if tag bit is there 
        cache_line_t* cache_line = &set->lines[i];
        if (cache_line->validbit && cache_line->tag == tag) {
           // found 
           metrics->hitcount++;
           //printf("Hit\n");
           cache_line->access = params->counter;
           if (set->last_accessed == cache_line) {
              metrics->double_accesses++;
              //printf("double-ref\n");
           }
           setLastAccessed(set, cache_line);
           return 0;
         }
    }
    //it is a miss
    metrics->misscount++; 
    //printf("miss\n");
    // Look for an entry to load into
    match = NULL;
    for (int i = 0; i < params->E; i++) {
        cache_line_t* cache_line = &set->lines[i];
        if (cache_line->validbit == 0) {
           match = cache_line;
           break;
        }
    }
    if (match) {
       match->validbit = 1;
       match->tag = tag;
       match->access = params->counter;
       setLastAccessed(set, match);
       return result;
    }
    // Need to evict
    metrics->evictcount++;
    match = NULL;
    for (int i = 0; i < params->E; i++) {
        cache_line_t* cache_line = &set->lines[i];
        if (cache_line->validbit) {
           if (!match) {
              match = cache_line;
           }
           else if (cache_line->access < match->access) {
                   match = cache_line;
           }
        }
    }
    if (match) { // found a lru entry to be evicted
       match->tag = tag;
       match->access = params->counter; // new item is most recently used
       if (match->dirtybit) {
          match->dirtybit=0;  ///since we loaded, dirtybit=0
          metrics->dirty_evicted+=(1<<params->b);
          metrics->dirty_active -=(1<<params->b);
          //printf("dirty-eviction\n");
       } else {
          //printf("eviction\n");
       }
       setLastAccessed(set, match);
     } else {
       printf("Error: no cache_line found\n");
       return ERROR_CACHE_LINE_NOT_FOUND;
     }
    return result;
}

int
updateCache(
    cache_t* cache, 
    param_t* params, 
    memaddr_t memaddr,
    metrics_t* metrics
){
    int result = 0;
    cache_line_t* match = NULL;
    memaddr_t tag = getTag(memaddr,params->s,params->b);  
    //printf("Tag value: %llu\n",tag);
    cache_set_t* set = getCacheSet(memaddr, params, cache);

    // If tag matches and validbit is set, nothing to do
    // Its a hit
    for (int i = 0; i < params->E; i++) {
        cache_line_t* cache_line = &set->lines[i];
        if (cache_line->tag == tag && cache_line->validbit)
        {
           match = cache_line;
           break;
        }
    }
    if (match) {
       metrics->hitcount++;
       //printf("hit\n");
       if (!match->dirtybit)
       {
           match->dirtybit = 1;
           metrics->dirty_active += (1 << params->b);
       }
       if (set->last_accessed == match)
       {
          metrics->double_accesses++;
          //printf("double-ref\n");
       }
       match->access = params->counter;
       setLastAccessed(set,match);
       return 0;
    }
    //miss
    metrics->misscount++;
    // no existing entry found, find an available spot
    for (int i = 0; i < params->E; i++) {
        cache_line_t* cache_line =&set->lines[i];
        if (!cache_line->validbit) {
           match = cache_line;
           break;
        }
    }
    if (match != NULL) {  //miss
       //printf("miss\n");
       match->tag = tag;
       match->validbit = 1;
       match->dirtybit = 1;
       metrics->dirty_active += (1 << params->b);
       match->access = params->counter;
       setLastAccessed(set,match);
       return 0;
    }
    // no available spot. evict someone
    metrics->evictcount++;   //dirty eviction is also a regular eviction
    match = NULL;
    for (int i = 0; i < params->E; i++) {
        cache_line_t* cache_line = &set->lines[i];
        if (cache_line->validbit) {
           if (!match) {
              match = cache_line;
           } else if (cache_line->access < match->access) {
              match = cache_line;
           }
        }
    }
    if (match) { // found a lru entry to be evicted
       match->tag = tag;
       match->access = params->counter; // most recently used 
       if (match->dirtybit) {
          metrics->dirty_evicted+=(1<<params->b);
          //don't update dirty_active because remains dirty
          //printf("dirty-eviction\n");
       } else {
          metrics->dirty_active +=(1<<params->b);
          match->dirtybit = 1;   //make dirty, because it's a STORE
          //printf("eviction\n");
       }
       setLastAccessed(set,match);
    } else {
       printf("Error: no cache_line found\n");
       return ERROR_CACHE_LINE_NOT_FOUND;
    }
    return result;
}

void
printMetrics(
     char action,
     memaddr_t memaddr,
     int size,
     metrics_t* metrics
) {
     printf("%c %llx,%d ", action, memaddr, size);
     if (metrics->misscount) {
        printf(" Miss ");
     }
     if (metrics->hitcount) {
        printf(" Hit ");
     }
     if (metrics->double_accesses) {
        printf(" Double-Ref ");
     }
     if (metrics->dirty_evicted){
        printf(" Dirty-Evicted ");
     } else if (metrics->evictcount) {
        printf(" Eviction ");
     }
     printf("\n");      
}

void
printCacheSets(
    FILE* fp,
    char action,
    int  serial,
    memaddr_t memaddr,
    cache_t* cache,
    param_t* params
) {
    fprintf(fp, "%d %c %llx (tag: %llu, s: %llu)\n",
        serial,
        action,
        memaddr,
        getTag(memaddr, params->s, params->b),
        getCacheSetIndex(memaddr, params->s, params->b));
    fprintf(fp, "\tSet\t|#\t|T\t|V\t|D\t|lru\n");
    for (int i = 0; i < params->S; i++) {
        cache_set_t* set = &cache->sets[i];
        fprintf(fp, "\t======\n");
        for (int j = 0; j < params->E; j++) {
            cache_line_t* cache_line = &set->lines[j];
            fprintf(fp, "\t----------\n");
			fprintf(fp, "\t%d\t|%d\t|%llu\t|%d\t|%d\t|%llu\n", i, j, cache_line->tag, cache_line->validbit, cache_line->dirtybit, cache_line->access);
        }
    }
}

int
parseTraceFile(
    char* file_path,
    param_t* params,
    cache_t* cache
) {
    int result = 0;
    FILE *tmp = NULL; //trace file 
    // FILE *fp = NULL; // Used for debugging
    char action;
    memaddr_t memaddr;
    int size;

    // fp = fopen(DEBUG_FILE_PATH, "w");
    // if (fp == NULL) {
    //    printf("Error: failed to open debug file - %s\n", DEBUG_FILE_PATH);
    //    return ERROR_OPEN_FILE;
    // }
    
    tmp = fopen(file_path, "r"); 
    if (tmp == NULL) {
        printf("Error: failed to open file - %s\n", file_path);
        return ERROR_OPEN_FILE;
    }

    while (fscanf(tmp, " %c %llx,%d\n", &action, &memaddr, &size) == 3)
    {
        metrics_t metrics = {0};
        params->counter++;
        switch(action)
        {
        case 'I':
            //printf("Skipping I\n");
            // printCacheSets(fp, 'I', params->counter, memaddr, cache, params);
            break; 
        
        case 'L':
            // printCacheSets(fp, 'L', params->counter, memaddr, cache, params);
            result = loadCache(cache, params, memaddr,&metrics); 
            if (verbose) {
               printMetrics('L',memaddr,size,&metrics);
            }
            addMetrics(&params->metrics, &metrics);
            // printCacheSets(fp, 'L', params->counter, memaddr, cache, params);
            break; 

        case 'S':
            // printCacheSets(fp, 'S', params->counter, memaddr, cache, params);
            result = updateCache(cache, params, memaddr, &metrics);
            if (verbose) {
               printMetrics('S',memaddr,size,&metrics);
            }
            addMetrics(&params->metrics, &metrics);
            // printCacheSets(fp, 'S', params->counter, memaddr, cache, params);
            break; 
        
        case 'M':
            // printCacheSets(fp, 'M', params->counter, memaddr, cache, params);
            result = loadCache(cache, params, memaddr, &metrics);
            if (result != 0) { 
               printf("Error - loadCache failed\n");
               return result;
            }
            result = updateCache(cache, params, memaddr, &metrics); 
            if (verbose) {
               printMetrics('M',memaddr,size,&metrics);
            }
            addMetrics(&params->metrics, &metrics);
            // printCacheSets(fp, 'M', params->counter, memaddr, cache, params);
            break; 
        
        default: 
            break; 
        }
        if (result != 0)  {
           break;
        }
    }
    
    fclose(tmp);

    // Used for debugging
    // 
    // if (fp != NULL) {
    //     fclose(fp);
    // }

    return result;
}

int
main(
    int argc,
    char* argv[]
) {
    int result = 0;
    cache_t current_cache = {0};
    param_t cache_param = {0}; 
    char* trace_file = NULL; 
    char input; 
    
    while((input = getopt(argc, argv, "s:E:b:t:vh")) != -1)
    {
        switch(input)
        {
        case 's': //number of cache sets
            cache_param.s = atoi(optarg);
            break; 
        
        case 'E': //cache lines per set 
            cache_param.E = atoi(optarg);
            break; 
        
        case 'b': //bytes per line for cache block
            cache_param.b = atoi(optarg);
            break; 

        case 't':
            trace_file = optarg; 
            break; 
        
        case 'v': 
            verbose = 1;
            break; 

        case 'h': 
            printUsage();
            exit(0); 
        
        default:
            printUsage();
            exit(-1); 
        }
    }

    if (trace_file == NULL) {
        printf("Error: no trace file specified.\n");
        printUsage();
        exit(-1);
    }

    cache_param.S = pow(2.0, cache_param.s); //S = 2^s
    cache_param.B = pow(2.0, cache_param.b); //B = 2^b
    cache_param.t = 64 - cache_param.s - cache_param.b; 

    result = init(
                cache_param.S,
                cache_param.E,
                &current_cache); //initialize cache
    if (result != 0) {
        printf("Error: failed to initialize cache\n");
        exit(result);
    }

    result = parseTraceFile(trace_file, &cache_param, &current_cache); 
    if (result != 0) {
        exit(result);
    }

    printSummary(
        cache_param.metrics.hitcount,
        cache_param.metrics.misscount,
        cache_param.metrics.evictcount,
        cache_param.metrics.dirty_evicted,
        cache_param.metrics.dirty_active,
        cache_param.metrics.double_accesses
        );

    return result;
}
