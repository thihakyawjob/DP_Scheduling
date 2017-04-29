#include <linux/sched.h>
#include <linux/percpu.h>
#include <linux/hrtimer.h>

#include <litmus/litmus.h>
#include <litmus/preempt.h>

#include <litmus/budget.h>
#include <litmus/fp_common.h>


struct enforcement_timer {
	/* The enforcement timer is used to accurately police
	 * slice budgets. */
	struct hrtimer		timer;
	int			armed;
};

int PID_No_To_Trigger;

DEFINE_PER_CPU(struct enforcement_timer, budget_timer);

static enum hrtimer_restart on_enforcement_timeout(struct hrtimer *timer)
{
	struct enforcement_timer* et = container_of(timer,
						    struct enforcement_timer,
						    timer);
	unsigned long flags;

	local_irq_save(flags);
	et->armed = 0;
	/* activate scheduler */	
	
	litmus_reschedule_local();
	//Set Timer is fired. TmrStatus = 3
	dp_set_tmrStatus(PID_No_To_Trigger,3);

	//TRACE("enforcement timer fired: %llu, %d, %d.\n",litmus_clock(), PID_No_To_Trigger, dp_get_tmrStatus(PID_No_To_Trigger));	
	local_irq_restore(flags);

	return  HRTIMER_NORESTART;
}

/* assumes called with IRQs off */
static void cancel_enforcement_timer(struct enforcement_timer* et)
{
	int ret;

	TRACE("cancelling enforcement timer.\n");

	/* Since interrupts are disabled and et->armed is only
	 * modified locally, we do not need any locks.
	 */

	if (et->armed) {
		ret = hrtimer_try_to_cancel(&et->timer);
		/* Should never be inactive. */
		BUG_ON(ret == 0);
		/* Should never be running concurrently. */
		BUG_ON(ret == -1);

		et->armed = 0;
	}
}

/* assumes called with IRQs off */
static void arm_enforcement_timer(struct enforcement_timer* et,
				  struct task_struct* t)
{
	lt_t when_to_fire;

	int tmrStatus;
	

	WARN_ONCE(!hrtimer_is_hres_active(&et->timer),
		KERN_ERR "WARNING: no high resolution timers available!?\n");

	/* Calling this when there is no budget left for the task
	 * makes no sense, unless the task is non-preemptive. */
	//BUG_ON(budget_exhausted(t) && (!is_np(t)));

	/* __hrtimer_start_range_ns() cancels the timer
	 * anyway, so we don't have to check whether it is still armed */
	 tmrStatus = dp_get_tmrStatus((t)->pid);



	if (likely(!is_np(t)) && (tmrStatus==2)) {

		when_to_fire = (lt_t)dp_get_time_to_Fire((t)->pid);
		
		//when_to_fire = litmus_clock() + proPoint;

		//TRACE_TASK(t, "arming enforcement timer with ProPoint: %d, %llu\n",(t)->pid, when_to_fire);
		//when_to_fire = litmus_clock() + budget_remaining(t);
		__hrtimer_start_range_ns(&et->timer,
					 ns_to_ktime(when_to_fire),
					 0 /* delta */,
					 HRTIMER_MODE_ABS_PINNED,
					 0 /* no wakeup */);
		et->armed = 1;
	}
}


/* expects to be called with IRQs off */
void update_enforcement_timer(struct task_struct* t)
{

/*
	struct enforcement_timer* et = &__get_cpu_var(budget_timer);	
	if (t && budget_precisely_enforced(t)) {		
		// Make sure we call into the scheduler when this budget		 
		// expires. 		
		arm_enforcement_timer(et, t);	
	} else if (et->armed) {		
		// Make sure we don't cause unnecessary interrupts. 	
		cancel_enforcement_timer(et);
		
	}

*/

	struct enforcement_timer* et = &__get_cpu_var(budget_timer);

	//int Armed_PID;
	struct task_struct *dp_t;
	dp_t = NULL;
	

	if(dp_Size()>0)
	{
	
		if(!dp_is_TimerARM())
		{
			PID_No_To_Trigger = dp_get_PID_earliest_trigger_time();	
		}
		
		if(PID_No_To_Trigger>0)
		{
			dp_t = find_task_by_vpid(PID_No_To_Trigger);
		}

		
		
		if(dp_t && (!dp_is_TimerARM()) && (dp_get_tmrStatus(PID_No_To_Trigger)==1) && (dp_get_time_to_Fire(PID_No_To_Trigger) > litmus_clock()))
		{

			dp_set_tmrStatus(PID_No_To_Trigger, 2);
			arm_enforcement_timer(et, dp_t);
			//TRACE("Inside UE_timer,PID, Tmr Status: %d, %d\n",PID_No_To_Trigger, dp_get_tmrStatus(PID_No_To_Trigger));

		}	
		else
		{
			/*
			Armed_PID = dp_get_Armed_PID();
			if(Armed_PID>0 && dp_get_time_to_Fire(Armed_PID) < litmus_clock())
			{
				cancel_enforcement_timer(et);
				dp_set_tmrStatus(Armed_PID, 0);
				//TRACE("Inside update_enforcement_timer,Cancelled PID : %d\n",Armed_PID);
			}
			*/

			//TRACE("Inside UE_timer Error,PID:%d, Tmr Status: %d, ARMPID: %d, Time To Fire: %llu, Current Time: %llu, ProPoint: %llu\n",PID_No_To_Trigger, dp_get_tmrStatus(PID_No_To_Trigger), 
			//Armed_PID, dp_get_time_to_Fire(PID_No_To_Trigger),litmus_clock(),dp_get_PrPoint(PID_No_To_Trigger));

		}
	}


	

}

/* expects to be called with IRQs off */
////////////// DP needs to enable timer once task is released //////////////////////////


void dp_update_enforcement_timer(void)
{
	struct enforcement_timer* et = &__get_cpu_var(budget_timer);

	//int Armed_PID;
	struct task_struct *dp_t;
	dp_t = NULL;
	

	if(dp_Size()>0)
	{
	
		if(!dp_is_TimerARM())
		{
			PID_No_To_Trigger = dp_get_PID_earliest_trigger_time();	
		}
		
		if(PID_No_To_Trigger>0)
		{
			dp_t = find_task_by_vpid(PID_No_To_Trigger);
		}

		
		
		if(dp_t && (!dp_is_TimerARM()) && (dp_get_tmrStatus(PID_No_To_Trigger)==1) && (dp_get_time_to_Fire(PID_No_To_Trigger) > litmus_clock()))
		{

			dp_set_tmrStatus(PID_No_To_Trigger, 2);
			arm_enforcement_timer(et, dp_t);
			//TRACE("Inside UE_timer,PID, Tmr Status: %d, %d\n",PID_No_To_Trigger, dp_get_tmrStatus(PID_No_To_Trigger));

		}	
		else
		{
			/*
			Armed_PID = dp_get_Armed_PID();
			if(Armed_PID>0 && dp_get_time_to_Fire(Armed_PID) < litmus_clock())
			{
				cancel_enforcement_timer(et);
				dp_set_tmrStatus(Armed_PID, 0);
				//TRACE("Inside update_enforcement_timer,Cancelled PID : %d\n",Armed_PID);
			}
			*/

			//TRACE("Inside UE_timer Error,PID:%d, Tmr Status: %d, ARMPID: %d, Time To Fire: %llu, Current Time: %llu, ProPoint: %llu\n",PID_No_To_Trigger, dp_get_tmrStatus(PID_No_To_Trigger), 
			//Armed_PID, dp_get_time_to_Fire(PID_No_To_Trigger),litmus_clock(),dp_get_PrPoint(PID_No_To_Trigger));

		}
	}
	
	

}



static int __init init_budget_enforcement(void)
{
	int cpu;
	struct enforcement_timer* et;

	PID_No_To_Trigger = 0; //THIHA KYAW

	for (cpu = 0; cpu < NR_CPUS; cpu++)  {
		et = &per_cpu(budget_timer, cpu);
		hrtimer_init(&et->timer, CLOCK_MONOTONIC, HRTIMER_MODE_ABS);
		et->timer.function = on_enforcement_timeout;
	}
	return 0;
}

module_init(init_budget_enforcement);
