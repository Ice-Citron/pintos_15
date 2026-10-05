            +----------------------+
            |    COMP50007/50012   |
            |  TASK 1: SCHEDULING  |
            |    DESIGN DOCUMENT   |
            +----------------------+
                   
---- GROUP ----

>> Fill in the names and email addresses of your group members.

FirstName LastName <email@domain.example>
FirstName LastName <email@domain.example>
FirstName LastName <email@domain.example>

---- PRELIMINARIES ----

>> If you have any preliminary comments on your submission, or notes for the
>> markers, please give them here.

>> Please cite any offline or online sources you consulted while preparing your 
>> submission, other than the PintOS documentation, course text, lecture notes 
>> and course staff.

             PRIORITY SCHEDULING
             ===================

---- DATA STRUCTURES ----

>> A1: Copy here the declaration of each new or changed `struct' or `struct'
>> member, global or static variable, `typedef', or enumeration.  
>> Identify the purpose of each in roughly 25 words.

---- ALGORITHMS ----

>> A2: How do you ensure that the highest priority waiting thread wakes up first
>> for a (i) semaphore, (ii) lock, or (iii) condition variable?

>> A3: Describe the sequence of events when a call to lock_acquire() causes a 
>> priority donation. How is nested donation handled?

>> A4: Describe the sequence of events when lock_release() is called on a lock
>> that a higher-priority thread is waiting for.

---- SYNCHRONISATION ----

>> A5: How do you avoid a race condition in thread_set_priority() when a thread 
>> needs to recompute its effective priority, but the donated priorities 
>> potentially change during the computation?
>> Can you use a lock to avoid the race?

---- RATIONALE ----

>> A6: Why did you choose this design?  
>> In what ways is it superior to another design you considered?

              ADVANCED SCHEDULER
              ==================

---- DATA STRUCTURES ----

>> B1: Copy here the declaration of each new or changed `struct' or `struct'
>> member, global or static variable, `typedef', or enumeration. 
>> Identify the purpose of each in roughly 25 words.

---- ALGORITHMS ----

>> B2: What steps are taken to minimise the amount of time spent in the timer 
>> interrupt handler?

---- RATIONALE ----

>> B3: Briefly critique your design, pointing out advantages and disadvantages
>> in your design choices.
