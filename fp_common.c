/*
 * litmus/fp_common.c
 *
 * Common functions for fixed-priority scheduler.
 */

#include <linux/percpu.h>
#include <linux/sched.h>
#include <linux/list.h>

#include <litmus/litmus.h>
#include <litmus/sched_plugin.h>
#include <litmus/sched_trace.h>

#include <litmus/fp_common.h>


/////////////////


#include<linux/slab.h>




/* fp_higher_prio -  returns true if first has a higher static priority
 *                   than second. Ties are broken by PID.
 *
 * both first and second may be NULL
 */

LIST_HEAD(dp_ListHead);

int fp_higher_prio(struct task_struct* first,
		   struct task_struct* second)
{
	struct task_struct *first_task = first;
	struct task_struct *second_task = second;

	/* There is no point in comparing a task to itself. */
	if (unlikely(first && first == second)) {
		TRACE_TASK(first,
			   "WARNING: pointless FP priority comparison.\n");
		return 0;
	}

	/* check for NULL tasks */
	if (!first || !second)
		return first && !second;

	if (!is_realtime(second_task))
		return 1;

#ifdef CONFIG_LITMUS_LOCKING

	/* Check for inherited priorities. Change task
	 * used for comparison in such a case.
	 */
	if (unlikely(first->rt_param.inh_task))
		first_task = first->rt_param.inh_task;
	if (unlikely(second->rt_param.inh_task))
		second_task = second->rt_param.inh_task;

	/* Comparisons to itself are only possible with
	 * priority inheritance when svc_preempt interrupt just
	 * before scheduling (and everything that could follow in the
	 * ready queue). Always favour the original job, as that one will just
	 * suspend itself to resolve this.
	 */
	if(first_task == second_task)
		return first_task == first;

	/* Check for priority boosting. Tie-break by start of boosting.
	 */
	if (unlikely(is_priority_boosted(first_task))) {
		/* first_task is boosted, how about second_task? */
		if (is_priority_boosted(second_task))
			/* break by priority point */
			return lt_before(get_boost_start(first_task),
					 get_boost_start(second_task));
		else
			/* priority boosting wins. */
			return 1;
	} else if (unlikely(is_priority_boosted(second_task)))
		/* second_task is boosted, first is not*/
		return 0;

#else
	/* No locks, no priority inheritance, no comparisons to itself */
	BUG_ON(first_task == second_task);
#endif

	if (get_priority(first_task) < get_priority(second_task))
		return 1;
	else if (get_priority(first_task) == get_priority(second_task))
		/* Break by PID. */
		return first_task->pid < second_task->pid;
	else
		return 0;
}

int fp_ready_order(struct bheap_node* a, struct bheap_node* b)
{
	return fp_higher_prio(bheap2task(a), bheap2task(b));
}

void fp_domain_init(rt_domain_t* rt, check_resched_needed_t resched,
		    release_jobs_t release)
{
	rt_domain_init(rt,  fp_ready_order, resched, release);

}

/* need_to_preempt - check whether the task t needs to be preempted
 */
int fp_preemption_needed(struct fp_prio_queue *q, struct task_struct *t)
{
	struct task_struct *pending;

	pending = fp_prio_peek(q);

	if (!pending)
		return 0;
	if (!t)
		return 1;

	/* make sure to get non-rt stuff out of the way */
	return !is_realtime(t) || fp_higher_prio(pending, t);
}

void fp_prio_queue_init(struct fp_prio_queue* q)
{
	int i;

	for (i = 0; i < FP_PRIO_BIT_WORDS; i++)
		q->bitmask[i] = 0;
	for (i = 0; i < LITMUS_MAX_PRIORITY; i++)
		bheap_init(&q->queue[i]);
}



//////////////////
/*   Dual Priority   */
void add_dp_node(int PID, lt_t tsk_C, lt_t tsk_D, int pfp_priority, int dp_priority, lt_t dp_PrPoint, int tmrStatus, lt_t time_to_fired)
{
    struct dp_list *dp_listPtr; 
	dp_listPtr = kmalloc(sizeof(struct dp_list *),GFP_KERNEL);
    
    dp_listPtr->dp_pid= PID;
	dp_listPtr->tsk_C= tsk_C;
	dp_listPtr->tsk_D= tsk_D;
	dp_listPtr->pfp_priority= pfp_priority;
	dp_listPtr->dp_priority= dp_priority;
	dp_listPtr->dp_PrPoint= dp_PrPoint;
	dp_listPtr->dp_timerStatus= tmrStatus;
	dp_listPtr->dp_time_to_fired= time_to_fired;

	
    INIT_LIST_HEAD(&dp_listPtr->dp_data_list);
    list_add(&dp_listPtr->dp_data_list, &dp_ListHead);
}

void dp_display(void)
{
	struct list_head *iter;
    struct dp_list *objPtr;

    __list_for_each(iter, &dp_ListHead) {
        objPtr = list_entry(iter, struct dp_list, dp_data_list);
        //printf("%d ", objPtr->dp_pid);
		TRACE_CUR("Display: %d,%llu,%llu,%d,%d,%llu, %d, %llu\n", objPtr->dp_pid, objPtr->tsk_C, objPtr->tsk_D,objPtr->pfp_priority,objPtr->dp_priority,objPtr->dp_PrPoint, objPtr->dp_timerStatus, objPtr->dp_time_to_fired);
    }

}


int dp_Size(void)
{
	struct list_head *iter;
    struct dp_list *objPtr;
	int sizeOfData = 0;

    __list_for_each(iter, &dp_ListHead) {
        objPtr = list_entry(iter, struct dp_list, dp_data_list);
        sizeOfData++;
    }

	return sizeOfData;
}


void dp_delete_all(void)
{
	struct list_head *iter;
    struct dp_list *objPtr;
    
  redo:
    __list_for_each(iter, &dp_ListHead) {
        objPtr = list_entry(iter, struct dp_list, dp_data_list);
        list_del(&objPtr->dp_data_list);
        //kfree(objPtr);
        goto redo;
    }
}

int dp_find_first_and_delete(int PID)
{
	struct list_head *iter;
    struct dp_list *objPtr;

    __list_for_each(iter, &dp_ListHead) {
        objPtr = list_entry(iter, struct dp_list, dp_data_list);
        if(objPtr->dp_pid== PID) {
            list_del(&objPtr->dp_data_list);
            //kfree(objPtr);
            return 1;
        }
    }

    return 0;
}

int dp_get_Pri2(int PID)
{
	struct list_head *iter;
    struct dp_list *objPtr;
	int Pri2Value = 0;

    __list_for_each(iter, &dp_ListHead) {
        objPtr = list_entry(iter, struct dp_list, dp_data_list);
        if(objPtr->dp_pid== PID) {
            Pri2Value = objPtr->dp_priority;
            //kfree(objPtr);
            return Pri2Value;
        }
    }

    return 0;
}

int dp_get_Pri1(int PID)
{
	struct list_head *iter;
    struct dp_list *objPtr;
	int Pri1Value = 0;

    __list_for_each(iter, &dp_ListHead) {
        objPtr = list_entry(iter, struct dp_list, dp_data_list);
        if(objPtr->dp_pid== PID) {
            Pri1Value = objPtr->pfp_priority;
            return Pri1Value;
        }
    }

    return 0;
}

void dp_set_Pri2(int PID, int Pri2)
{
	struct list_head *iter;
    struct dp_list *objPtr;

    __list_for_each(iter, &dp_ListHead) {
        objPtr = list_entry(iter, struct dp_list, dp_data_list);
        if(objPtr->dp_pid== PID) 
		{
            objPtr->dp_priority = Pri2;
            //kfree(objPtr);
            
        }
    }

}


lt_t dp_get_PrPoint(int PID)
{
	struct list_head *iter;
    struct dp_list *objPtr;
	int PrPoint = 0;

    __list_for_each(iter, &dp_ListHead) {
        objPtr = list_entry(iter, struct dp_list, dp_data_list);
        if(objPtr->dp_pid== PID) {
            PrPoint = objPtr->dp_PrPoint;
            //kfree(objPtr);
            return PrPoint;
        }
    }

    return 0;
}

void dp_set_PrPoint(int PID, lt_t PrPoint)
{
	struct list_head *iter;
    struct dp_list *objPtr;

    __list_for_each(iter, &dp_ListHead) {
        objPtr = list_entry(iter, struct dp_list, dp_data_list);
        if(objPtr->dp_pid== PID) 
		{
            objPtr->dp_PrPoint = PrPoint;
            //kfree(objPtr);
            
        }
    }
}

int dp_Find_PID(int PID)
{
	struct list_head *iter;
    struct dp_list *objPtr;
	int status=0;

	__list_for_each(iter, &dp_ListHead) {
    	objPtr = list_entry(iter, struct dp_list, dp_data_list);
        if(objPtr->dp_pid==PID) {          
            //kfree(objPtr);
            status = 1;
        }
	}

	return status;
   
}

void calculate_Pri2_RM(void)
{
	struct list_head *iter1, *iter2;
    struct dp_list *objPtr1, *objPtr2;

	int PID_No = 0;
	lt_t DeadLine_Val = 0;
	int pri2 = 1;

    __list_for_each(iter1, &dp_ListHead) {
        objPtr1 = list_entry(iter1, struct dp_list, dp_data_list);
		PID_No = objPtr1->dp_pid;
		DeadLine_Val = objPtr1->tsk_D;
		pri2 = 1; //reset it to 1 again

		__list_for_each(iter2, &dp_ListHead) {
			objPtr2 = list_entry(iter2, struct dp_list, dp_data_list);

				if(DeadLine_Val > objPtr2->tsk_D)
				{
					pri2++;
				}
			}
		
		dp_set_Pri2(PID_No,pri2);

    }
}

void calculate_ProPoint(void)
{
	
	struct list_head *iter1, *iter2;
	struct dp_list *objPtr1, *objPtr2;

	int PID_No = 0;
	lt_t DeadLine_Val = 0;
	lt_t WCET_Val = 0;
	lt_t HWCET_Val = 0;
	lt_t ProPoint_Val = 0;

	 __list_for_each(iter1, &dp_ListHead) {
        objPtr1 = list_entry(iter1, struct dp_list, dp_data_list);
		PID_No = objPtr1->dp_pid;
		WCET_Val = objPtr1->tsk_C;
		DeadLine_Val = objPtr1->tsk_D;

		HWCET_Val = 0; //Reset Value Again

		__list_for_each(iter2, &dp_ListHead) {
			objPtr2 = list_entry(iter2, struct dp_list, dp_data_list);

				if(objPtr1->pfp_priority > objPtr2->pfp_priority)
				{
					HWCET_Val +=  objPtr2->tsk_C;
				}
			}
		
		ProPoint_Val = DeadLine_Val - WCET_Val - HWCET_Val;
		dp_set_PrPoint(PID_No,ProPoint_Val);

		TRACE_CUR("Display: %d,%llu,%llu,%d,%d,%llu, %d, %llu\n", objPtr1->dp_pid, objPtr1->tsk_C, objPtr1->tsk_D,objPtr1->pfp_priority,objPtr1->dp_priority,objPtr1->dp_PrPoint, objPtr1->dp_timerStatus, objPtr1->dp_time_to_fired);

	
    }
	
}

int dp_get_tmrStatus(int PID)
{
	struct list_head *iter;
    struct dp_list *objPtr;
	int tmrStatus = 0;

    __list_for_each(iter, &dp_ListHead) {
        objPtr = list_entry(iter, struct dp_list, dp_data_list);
        if(objPtr->dp_pid== PID) {
            tmrStatus = objPtr->dp_timerStatus;
            return tmrStatus;
        }
    }

    return 0;
}
void dp_set_tmrStatus(int PID , int tmrStatus)
{
	struct list_head *iter;
    struct dp_list *objPtr;

    __list_for_each(iter, &dp_ListHead) {
        objPtr = list_entry(iter, struct dp_list, dp_data_list);
        if(objPtr->dp_pid== PID) 
		{
            objPtr->dp_timerStatus = tmrStatus;
            
        }
    }
	
}


int dp_get_Armed_PID(void)
{
	struct list_head *iter;
    struct dp_list *objPtr;
	int pid = 0;

    __list_for_each(iter, &dp_ListHead) {
        objPtr = list_entry(iter, struct dp_list, dp_data_list);
        if(objPtr->dp_timerStatus ==2) {
            pid = objPtr->dp_pid;
            return pid;
        }
    }

    return 0;

}


int dp_get_PID_from_fireTask(void)
{
	struct list_head *iter;
    struct dp_list *objPtr;
	int pid = 0;

    __list_for_each(iter, &dp_ListHead) {
        objPtr = list_entry(iter, struct dp_list, dp_data_list);
        if(objPtr->dp_timerStatus == 3) {
            pid = objPtr->dp_pid;
            return pid;
        }
    }

    return 0;

}

int dp_is_TimerARM(void)
{
	struct list_head *iter;
    struct dp_list *objPtr;
	//int pid = 0;

    __list_for_each(iter, &dp_ListHead) {
        objPtr = list_entry(iter, struct dp_list, dp_data_list);

        if(objPtr->dp_timerStatus>1 &&  objPtr->dp_timerStatus<4) {
 
            return 1;
        }
    }

    return 0;

}



lt_t dp_get_time_to_Fire(int PID)
{
	struct list_head *iter;
    struct dp_list *objPtr;
	lt_t time_to_Fire_Value = 0;

    __list_for_each(iter, &dp_ListHead) {
        objPtr = list_entry(iter, struct dp_list, dp_data_list);
        if(objPtr->dp_pid== PID) {
            time_to_Fire_Value = objPtr->dp_time_to_fired + (lt_t)(objPtr->dp_PrPoint);
            return time_to_Fire_Value;
        }
    }

    return 0;
}

void dp_set_time_Released(int PID, lt_t time_to_Fire_Value)
{
	struct list_head *iter;
    struct dp_list *objPtr;

    __list_for_each(iter, &dp_ListHead) {
        objPtr = list_entry(iter, struct dp_list, dp_data_list);
        if(objPtr->dp_pid== PID) 
		{
            objPtr->dp_time_to_fired = time_to_Fire_Value;
            
        }
    }
	
}
int dp_get_PID_earliest_trigger_time(void)
{
	struct list_head *iter1;
    struct dp_list *objPtr1;

	lt_t earliest_Prom_Point;
	int dp_PID_No;
	earliest_Prom_Point = 0;
	dp_PID_No = 0;



    __list_for_each(iter1, &dp_ListHead) {
        objPtr1 = list_entry(iter1, struct dp_list, dp_data_list);

		if((earliest_Prom_Point==0) && (dp_get_tmrStatus(objPtr1->dp_pid)==1))
		{
			dp_PID_No  = objPtr1->dp_pid;
			earliest_Prom_Point  = dp_get_time_to_Fire(dp_PID_No);	
		}
		else if((earliest_Prom_Point > dp_get_time_to_Fire(objPtr1->dp_pid)) && (dp_get_tmrStatus(objPtr1->dp_pid)==1))
		{
			earliest_Prom_Point = dp_get_time_to_Fire(objPtr1->dp_pid);
			dp_PID_No  = objPtr1->dp_pid;
			
		}
    }

	return dp_PID_No;
}









/*   Dual Priority   */
//////////////////





