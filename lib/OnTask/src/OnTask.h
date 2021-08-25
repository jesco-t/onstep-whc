/*
 * Title       OnTask
 * by          Howard Dutton
 *
 * Copyright (C) 2021 Howard Dutton
 * www.stellarjourney.com
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * DESCRIPTION:
 *
 * Software timer based processes must use "tasks.yield();" (or the "Y;" macro, which is the same exact thing)
 * during any processing that requires a significant amount of time. This allows for the processing of other tasks
 * while it is working.  What a "significant amount of time" is depends on the program's requirements.  It might
 * be measured in micro-seconds, or milli-seconds, or seconds.  Yield should not be used for hardware timers based 
 * processes
*/

#pragma once
#include <Arduino.h>

// Default provision is for 8 tasks, up to 255 are allowed, to change use:
// #define TASKS_MAX 200 (for example)
#ifndef TASKS_MAX
  #define TASKS_MAX 16
#endif

// ESP32 override cli/sei and use muxes to block the h/w timer ISR's instead
#ifdef ESP32
  // on the ESP32 noInterrupts()/interrupts() are #defined to be cli()/sei()
  // cli()/sei() are empty and do nothing, below we use them for our purposes
  // to the block execution of all four timer interrupts
  extern portMUX_TYPE timerMux;
  #undef cli
  // disable hardware timer based tasks by FreeRTOS mux
  #define cli() portENTER_CRITICAL(&timerMux)
  #undef sei
  // enable hardware timer based tasks by FreeRTOS mux
  #define sei() portEXIT_CRITICAL(&timerMux)
  // disable hardware timer based tasks by clearing interrupt flag(s)
  extern void timerAlarmsDisable();
  // enable hardware timer based tasks by setting interrupt flag(s)
  extern void timerAlarmsEnable();
#endif

// short Y macro to embed yield()
#define Y tasks.yield()

// short macro to allow momentary postponement of the current task
extern unsigned char __task_postpone;
// postpone currently executing task (scheduler picks task up ASAP again on exit)
#define task_postpone() __task_postpone = true;

// mutex macros, do not run these in hardware timers (not ISR safe)!
extern unsigned char __task_mutex[];
#define tasks_mutex_enter(m) bitSet(__task_mutex[(m)/8],(m)%8);
#define tasks_mutex_exit(m)  bitClear(__task_mutex[(m)/8],(m)%8);
#define tasks_mutex_busy(m)  (bitRead(__task_mutex[(m)/8],(m)%8))

enum PeriodUnits {PU_NONE, PU_MILLIS, PU_MICROS, PU_SUB_MICROS};

class Task {
  public:
    Task(uint32_t period, uint32_t duration, bool repeat, uint8_t priority, void (*volatile callback)());
    ~Task();

    bool requestHardwareTimer(uint8_t num, uint8_t hwPriority);

    void setCallback(void (*volatile callback)());

    // run task at the prescribed interval
    // note: tasks are timed in such a way as to achieve an accurate average frequency, if
    //       the task occurs late the next call is scheduled earlier to make up the difference
    bool poll();

    void setPeriod(unsigned long period);
    void setPeriodMicros(unsigned long period);
    void setPeriodSubMicros(unsigned long period);
    void setFrequency(double freq);

    void setDuration(unsigned long duration);
    bool isDurationComplete();
    void setDurationComplete();

    void setRepeat(bool repeat);

    void setPriority(bool priority);
    uint8_t getPriority();

    void setNameStr(const char name[]);
    char *getNameStr();

    #ifdef TASKS_PROFILER_ENABLE
      double getArrivalAvg();
      double getArrivalMax();
      double getRuntimeTotal();
      long getRuntimeTotalCount();
      double getRuntimeMax();
    #endif

  private:

    void setHardwareTimerPeriod();

    char                   processName[8];
    unsigned long          period                     = 0;
    unsigned long          next_period                = 0;
    unsigned long          duration                   = 0;
    bool                   repeat                     = false;
    uint8_t                priority                   = 0;
    bool                   running                    = false;
    PeriodUnits            period_units               = PU_MILLIS;
    PeriodUnits            next_period_units          = PU_NONE;
    unsigned long          start_time                 = 0;
    unsigned long          next_task_time             = 0;
    uint8_t                hardwareTimer              = 0;
    void (*volatile callback)() = NULL;

    #ifdef TASKS_PROFILER_ENABLE
      volatile double        average_arrival_time       = 0;
      volatile unsigned long average_arrival_time_count = 0;
      volatile long          max_arrival_time           = 0;
      volatile double        total_runtime              = 0;
      volatile unsigned long total_runtime_count        = 0;
      volatile long          max_runtime                = 0;
    #endif
};

class Tasks {
  public:
    Tasks();
    ~Tasks();

    // add process task
    // period:   between process calls in milliseconds, use 0 to disable
    //           software timers are limited to <= 49 days, hardware timers are limited to <= 139 seconds.
    // duration: in milliseconds the task is valid for, use 0 for unlimited (deletes the task on completion)
    // repeat:   true if the task is allowed to repeat, false to run once (sets period to 0 on completion)
    // priority: software level highest 0 to 7 lowest.  hardware tasks are always higher priority than software tasks.
    //           for software timed tasks level 0 must be complete before level 1 are serviced, etc.  tasks are run
    //           round robin within their priority levels, the most recently serviced task will be last visited again
    // callback: function to handle this tasks processing
    // returns:  handle to the task on success, or 0 on failure
    uint8_t add(uint32_t period, uint32_t duration, bool repeat, uint8_t priority, void (*volatile callback)());

    // add process task
    // period:   between process calls in milliseconds, use 0 to disable
    //           software timers are limited to <= 49 days, hardware timers are limited to <= 139 seconds.
    // duration: in milliseconds the task is valid for, use 0 for unlimited (deletes the task on completion)
    // repeat:   true if the task is allowed to repeat, false to run once (sets period to 0 on completion)
    // priority: software level highest 0 to 7 lowest.  hardware tasks are always higher priority than software tasks.
    //           for software timed tasks level 0 must be complete before level 1 are serviced, etc.  tasks are run
    //           round robin within their priority levels, the most recently serviced task will be last visited again
    // callback: function to handle this tasks processing
    // name:     an optional short (max length 7) char str describing the task 
    // returns:  handle to the task on success, or 0 on failure
    uint8_t add(uint32_t period, uint32_t duration, bool repeat, uint8_t priority, void (*volatile callback)(), const char name[]);

    // allocates a hardware timer, if available, for this task
    // handle:       task handle
    // num:          the hardware timer number, there are up to four (1 to 4) depending on the platform
    // hwPriority:   hardware interrupt priority, default is 128
    // returns:      true if successful, or false (in which case the standard polling timer will still be available)
    // notes:
    //   "repeat"   must be true
    //   "priority" must be 0 (all are higher than task priority 0)
    //   generally it's a good idea to NOT poll tasks (P macro) from within the process associated with this task
    bool requestHardwareTimer(uint8_t handle, uint8_t num);
    bool requestHardwareTimer(uint8_t handle, uint8_t num, uint8_t hwPriority);

    bool setCallback(uint8_t handle, void (*volatile callback)());

    // remove process task
    // handle: task handle
    // note:   do not remove a task if the task process is running
    void remove(uint8_t handle);

    // set process period ms
    // handle: task handle
    // period: in milliseconds, use 0 for disabled
    // notes:
    //   the period/frequency change takes effect on the next task cycle
    //   if the period is > the hardware timers maximum period the task is disabled
    void setPeriod(uint8_t handle, unsigned long period);

    // set process period us
    // handle: task handle
    // period: in milliseconds, use 0 for disabled
    // notes:
    //   the period/frequency change takes effect on the next task cycle
    //   if the period is > the hardware timers maximum period the task is disabled
    void setPeriodMicros(uint8_t handle, unsigned long period);

    // set process period sub-us
    // handle: task handle
    // period: in sub-microseconds (1/16 microsecond units), use 0 for disabled
    // notes:
    //   the period/frequency change takes effect on the next task cycle
    //   if the period is > the hardware timers maximum period the task is disabled
    void setPeriodSubMicros(uint8_t handle, unsigned long period);

    // change process period Hz
    // handle: task handle
    // freq:  in Hertz, use 0 for disabled
    // notes:
    //   the period/frequency change takes effect on the next task cycle
    //   when setting a frequency the most appropriate setPeriod is used automatically
    //   if the period is > ~49 days (or > the hardware timers maximum period) the task is disabled
    void setFrequency(uint8_t handle, double freq);

    // change process duration (milliseconds,) use 0 for disabled
    void setDuration(uint8_t handle, unsigned long duration);

    // late removal of process task
    // handle: task handle
    // note:   removes a task process, even if it is running
    void setDurationComplete(uint8_t handle);

    // change process repeat (true/false)
    void setRepeat(uint8_t handle, bool repeat);

    // change process priority level (highest 0 to 7 lowest)
    void setPriority(uint8_t handle, uint8_t priority);

    // set the process name
    void setNameStr(uint8_t handle, const char name[]);

    // set the process name
    char *getNameStr(uint8_t handle);

    // allow search for tasks, returns 0 if no handles are found
    uint8_t getFirstHandle();

    uint8_t getNextHandle();

    #ifdef TASKS_PROFILER_ENABLE
      double getArrivalAvg(uint8_t handle);
      double getArrivalMax(uint8_t handle);
      double getRuntimeTotal(uint8_t handle);
      long getRuntimeTotalCount(uint8_t handle);
      double getRuntimeMax(uint8_t handle);
    #endif

    // runs tasks at their prescribed interval, each call can trigger at most a single process
    // processes that are already running are ignored so it's ok to poll() within a process
    void yield();
    void yield(unsigned long milliseconds);

  private:
    // keep track of the range of priorities so we don't waste cycles looking at empty ones
    void updatePriorityRange();
    // keep track of the range of tasks so we don't waste cycles looking at empty ones
    void updateEventRange();

    uint8_t highest_task     = 0;
    uint8_t highest_priority = 0;
    uint8_t count            = 0;
    uint8_t num_tasks        = 0;
    uint8_t number[8]        = {255, 255, 255, 255, 255, 255, 255, 255};
    bool    allocated[TASKS_MAX];
    uint8_t handleSearch     = 255;
    Task    *task[TASKS_MAX];
};
