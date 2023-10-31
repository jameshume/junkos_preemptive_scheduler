/*
 * MIT License -- Copyright (c) 2023 James Edward Hume -- See LICENSE file in
 * repository root.
 */
#pragma once
#ifndef INC__JUNKOS_SCHEDULER
#define INC__JUNKOS_SCHEDULER

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

typedef uint8_t junkos_task_id_t;
typedef void (*junkos_task_func)(void);
typedef uint8_t junkos_task_priority_t;

typedef struct junkos_task_tag
{
	struct junkos_task_tag *next;
	junkos_task_func run;
	junkos_task_id_t id;
	junkos_task_priority_t priority; /* Low is higher prio */
	bool auto_run;
} junkos_task_t;



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
bool junkos_scheduler_init(junkos_task_t *const tasks, const size_t num_tasks);


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
bool junkos_scheduler_set_task_runnable(const junkos_task_id_t task_id);

/*
 * Runs the scheduler. Function does not return.
 *
 * The scheduler runs by continually looking for the highest priority task that is currently ready to run,
 * taking it off the runnable queue, putting it onto the paused queue and then running one iteration of
 * the task function to completion before looking for the next highest priority runnable task.
 */
void junkos_scheduler(void) __attribute__ ((noreturn));

#endif /* INC__JUNKOS_SCHEDULER */
