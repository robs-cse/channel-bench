#include <autoconf.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <sel4/sel4.h>
#include "vlist.h"
#include "cachemap.h"
#include "pp.h"
#include "low.h"
#include "search.h"

#include "bench_common.h"
#include "bench_types.h"


static void visit_page(cachemap_t cm, int secret) {
    
    for (int i = 0; i < secret; i++) {

        /*the upper bits select the page*/
        vlist_t vl = cm->sets[i >> 6]; 
        
        /*16 is the associativity*/
        int l = 16; 
        if (l > vl_len(vl)) 
            l = vl_len(vl); 

        /*touch the page*/
        for (int j = 0; j < l; j++) {

            char *p = (char *)vl_get(vl, j); 
            /*the lower bits slect the set within a page*/
            p[(i & 0x3f) << 6] = 0; 
        }
    }
}

      
static void warmup(cachemap_t cm) {

    /*warming up the testing platform with randomized 
      cache accessing sequence*/
    int total_sec = cm->nsets * 64 + 1;
    int secret; 
   
    for (int i = 0; i < NUM_KD_WARMUP_ROUNDS; i++) {
        secret = random() % total_sec; 

        visit_page(cm, secret);
    }
}




int l3_kd_trojan(bench_env_t *env) {
    seL4_MessageInfo_t info;
    int total_sec, secret;
    bench_args_t *args = env->args;

    uint32_t volatile *share_vaddr = (uint32_t *)args->shared_vaddr; 


    cachemap_t cm = map();
    assert(cm); 

//    printf("cm nsets %d\n", cm->nsets);
    /*to root task*/
    info = seL4_MessageInfo_new(seL4_Fault_NullFault, 0, 0, 1);
    seL4_SetMR(0, 0); 
    seL4_Send(args->r_ep, info);

    warmup(cm);

    /*to SPY*/
    seL4_Send(args->ep, info);

    /*nprobing sets is 64 (16 colour * 4 cores) on a coloured platform
     and 128 (32 colour * 4 cores) on a non-coloured platform
     secret represents the number of cache sets in total, 64 sets in a 
     page, all together nsets * 64.*/

    total_sec = cm->nsets * 64 + 1;


    for (int n = 0; n < CONFIG_BENCH_DATA_POINTS; n++) {

        newTimeSlice();

        secret = random() % total_sec;  
        visit_page(cm, secret);

        /*update the secret read by low*/ 
        *share_vaddr = secret; 

    }

    for(;;) {
        /*never return*/

    }
 
}



