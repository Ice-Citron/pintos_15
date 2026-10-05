             +-------------------------+
             |     COMP50007/50012     |
             |  TASK 2: USER PROGRAMS  |
             |     DESIGN DOCUMENT     |
             +-------------------------+

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

               ARGUMENT PASSING
               ================

---- DATA STRUCTURES ----

>> A1: Copy here the declaration of each new or changed `struct' or `struct'
>> member, global or static variable, `typedef', or enumeration. 
>> Identify the purpose of each in roughly 25 words.

---- ALGORITHMS ----

>> A2: How does your argument parsing code avoid overflowing the user's stack
>> page? What are the efficiency considerations of your approach?

                 SYSTEM CALLS
                 ============

---- DATA STRUCTURES ----

>> B1: Copy here the declaration of each new or changed `struct' or `struct'
>> member, global or static variable, `typedef', or enumeration. 
>> Identify the purpose of each in roughly 25 words.

---- ALGORITHMS ----

>> B2: Describe how your code ensures safe memory access of user provided data
>> from within the kernel. 

>> B3: When an error is detected during a system call handler, how do you ensure 
>> that all temporarily allocated resources (locks, buffers, etc.) are freed?

>> B4: Describe your implementation of the "wait" system call and how it
>> interacts with process termination for both the parent and child.

---- SYNCHRONISATION ----

>> B5: The "exec" system call returns -1 if loading the new executable fails, so
>> it cannot return before the new executable has completed loading. 
>> How does your code ensure this? 
>> How is the load success/failure status passed back to the thread that calls 
>> "exec"?

>> B6: Consider parent process P with child process C. 
>> How do you ensure proper synchronisation and avoid race conditions when: 
>>   i) P calls wait(C) before C exits?
>>  ii) P calls wait(C) after C exits?
>> iii) P terminates, without waiting, before C exits?
>>  iv) P terminates, without waiting, after C exits?
>> Additionally, how do you ensure that all resources are freed regardless of 
>> the above case? 

---- RATIONALE ----

>> B7: Why did you choose to implement safe access of user memory from the
>> kernel in the way that you did?

>> B8: What advantages and disadvantages can you see to your design for file 
>> descriptors?
