/*
 * MIT License -- Copyright (c) 2023 James Edward Hume -- See LICENSE file in
 * repository root.
 */
#include "junkos_scheduler.h"
#include "stm32f401xe.h"


/**********************************************************************************************************************
 * LOCAL TYPE DEFINITIONS
 *********************************************************************************************************************/
/*
 * Context for the scheduler
 *
 * Members:
 *    task_table        - pointer to the first element in an array of junkos_task_t structures.
 *    task_table_size   - number of elements in`task_table`.
 *    task_ready_head   - pointer to the first task that is ready to run in a list that is ordered by priority, highest
 *                        priority nearest the head of the list and lowest priority at the tail
 *                        The scheduler will just sit in a loop continually popping of the next task from the head of
 *                        this list until there is no ready task, in which case it will just wait for the list to
 *                        be populated again.
 *    task_blocked_head - pointer to the first task that is not scheduled to run n a list that is ordered by priority,
 *                        highest priority nearest the head of the list and lowest priority at the tail.
 */
typedef struct junkos_scheduler_context_tag
{
	junkos_task_t *task_table;
	size_t         task_table_size;
	junkos_task_t *task_ready_head;
	junkos_task_t *task_blocked_head;
} junkos_scheduler_context_t;


/**********************************************************************************************************************
 * LOCAL (STATIC )FUNCTION DEFINITIONS
 *********************************************************************************************************************/
static junkos_task_t* junkos_delete_task_from_list(junkos_task_t **head, const junkos_task_id_t task_id);
static void junkos_put_task_on_list_in_priority_order(junkos_task_t **head, junkos_task_t *const task);
static void junkos_put_task_on_list_in_priority_order(junkos_task_t **head, junkos_task_t *const task);
static junkos_task_t* junkos_pop_task_off_list_in_priority_order(junkos_task_t **head);


/**********************************************************************************************************************
 * LOCAL VARIABLES
 *********************************************************************************************************************/
static junkos_scheduler_context_t gbl_context = {
	.task_table        = NULL,
	.task_table_size   = 0,
	.task_ready_head   = NULL,
	.task_blocked_head = NULL,
};


/**********************************************************************************************************************
 * PUBLIC FUNCTIONS
 *********************************************************************************************************************/

/*
 * Returns the scheduler context object.
 *
 * This function should be viewed as private to this module. It is made public so that it can be mocked by
 * unit tests.
 */
junkos_scheduler_context_t* junkos_scheduler_get_context(void)
{
	return &gbl_context;
}


/*
 * Initialises the scheduler with a table of tasks that the application using the OS wishes to run.
 *
 * Params:
 *    tasks     - pointer to the first element of an array of junkos_task_t structures.
 *    num_tasks - the number of elements in the `tasks` array.
 *
 * Returns:
 *    true if successfully initialised, false if there is an error: either a NULL parameter or
 *    a task with a NULL run function.
 */
bool junkos_scheduler_init(junkos_task_t *const tasks, const size_t num_tasks)
{
	bool success = true;
	junkos_scheduler_context_t *const context = junkos_scheduler_get_context();

	if ((tasks == NULL) || (num_tasks == 0))
	{
		return false;
	}

	context->task_table        = NULL;
	context->task_table_size   = 0;
	context->task_ready_head   = NULL;
	context->task_blocked_head = NULL;

	for(junkos_task_id_t task_index = 0; task_index < num_tasks; ++task_index)
	{
		if (tasks[task_index].run == NULL)
		{
			success = false;
			break;
		}

		tasks[task_index].next = NULL;

		if(tasks[task_index].auto_run)
		{
			junkos_put_task_on_list_in_priority_order(&context->task_ready_head, &tasks[task_index]);
		}
		else
		{
			junkos_put_task_on_list_in_priority_order(&context->task_blocked_head, &tasks[task_index]);
		}
	}

	if (success)
	{
		context->task_table        = tasks;
		context->task_table_size   = num_tasks;
	}
	else
	{
		/* A task looked invalid so reset the task queues to undo additions made above */
		context->task_ready_head   = NULL;
		context->task_blocked_head = NULL;
	}

	return success;
}


/*
 * Make a task runnable so that on some future scheduler iteration it will be run.
 *
 * Takes the task off the paused queue if it is on it, and puts it on the runnable queue. It is inserted into the
 * runnable queue such that it will be run after any tasks of the same priority that are already ready to run.
 *
 * Params:
 *     task_id - the unique task identifier that was set for the task's entry in the task table passed to the
 *               scheduler initialisation function.
 *
 * Returns:
 *     true if the task has been made runnable, false if the task with either already runnable or the id is
 *     unrecognised.
 */
bool junkos_scheduler_set_task_runnable(const junkos_task_id_t task_id)
{
	bool success = false;
	junkos_scheduler_context_t *const context = junkos_scheduler_get_context();

	__disable_irq();
	{

		junkos_task_t *const task = junkos_delete_task_from_list(&context->task_blocked_head, task_id);
		if (task)
		{
			success = true;
			junkos_put_task_on_list_in_priority_order(&context->task_ready_head, task);
		}
	}
	__enable_irq();

	return success;
}


/*
 * Runs the scheduler. Function does not return.
 *
 * The scheduler runs by continually looking for the highest priority task that is currently ready to run,
 * taking it off the runnable queue, putting it onto the paused queue and then running one iteration of
 * the task function to completion before looking for the next highest priority runnable task.
 */
void junkos_scheduler(void)
{
	junkos_scheduler_context_t *const context = junkos_scheduler_get_context();

	 while(true)
	 {
		 junkos_task_t *task;

		__disable_irq();
		{
			task = junkos_pop_task_off_list_in_priority_order(&context->task_ready_head);
			if (task)
			{
				junkos_put_task_on_list_in_priority_order(&context->task_blocked_head, task);
			}
		}
		__enable_irq();

	    if (task)
	    {
	    	task->run();
	    }
	    else
	    {
	       __asm volatile ("wfi");
	    }
	 }
}


/**********************************************************************************************************************
 * LOCAL FUNCTIONS
 *********************************************************************************************************************/
static junkos_task_t* junkos_delete_task_from_list(junkos_task_t **head, const junkos_task_id_t task_id)
{
	junkos_task_t* task = NULL;
	junkos_task_t* prev = NULL;
	junkos_task_t* current = *head;

	while(current)
	{
		if (current->id == task_id)
		{
			if (current == *head)
			{
				*head = current->next;
			}
			else
			{
				prev->next = current->next;
			}
			task = current;
			task->next = NULL;
			break;
		}

		prev = current;
		current = current->next;
	}

	return task;
}

// Precondition interrupts disabled
static void junkos_put_task_on_list_in_priority_order(junkos_task_t **head, junkos_task_t *const task)
{
	if (*head)
	{
		/* Put on list in priority order behind any of the same priority so that a thread
		 * cannot starve others of same priority. */
		junkos_task_t *current = *head;
		junkos_task_t *prev    = NULL;

		while(current)
		{
			if (current->priority > task->priority)
			{
				/* Have found a less important task. Insert it before that task */
				if (current == *head)
				{
					task->next = *head;
					*head = task;
				}
				else
				{
					prev->next = current;
					task->next = current;
				}
				break;
			}

			prev = current;
			current = current->next;
		}

		if (!current)
		{
			/* The while loop completed without inserting the task. Put it on the end
			 * of the list. */
			prev->next = task;
			task->next = NULL;
		}
	}
	else
	{
		*head = task;
	}
}

// Precondition interrupts disabled
static junkos_task_t* junkos_pop_task_off_list_in_priority_order(junkos_task_t **head)
{
	junkos_task_t* task = NULL;

	if (*head)
	{
		task = *head;
		*head = task->next;
		task->next = NULL;
	}

	return task;
}
