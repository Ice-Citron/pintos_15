            +--------------------------+
            |      COMP50007/50012     |
            |  TASK 3: VIRTUAL MEMORY  |
            |      DESIGN DOCUMENT     |
            +--------------------------+

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

            PAGE TABLE MANAGEMENT AND LAZY LOADING
            ======================================

---- DATA STRUCTURES ----

>> A1: Copy here the declaration of each new or changed `struct' or `struct'
>> member, global or static variable, `typedef', or enumeration that relates to
>> your supplemental page table and table of shared files/pages. 
>> Identify the purpose of each in roughly 25 words.

---- ALGORITHMS ----

>> A2: Describe your code for finding the location of a given page that is not 
>> currently loaded into memory.

>> A3: How have you implemented sharing of read only pages?

>> A4: When a process P obtains a frame that was previously used by a process Q,
>> how do you adjust the page directory of process Q (and any other data 
>> structures) to reflect the frame Q no longer has?

---- SYNCHRONISATION ----

>> A5: Explain how you handle access to user pages that are not present when a
>> system call is made. In particular, how do you avoid deadlock or race
>> conditions when reading from or writing to a file during a system call.

---- RATIONALE ----

>> A6: Why did you choose the data structure(s) that you did for representing
>> the supplemental page table and table of shared files/pages?

               FRAME TABLE MANAGEMENT AND EVICTION
               ===================================

---- DATA STRUCTURES ----

>> B1: Copy here the declaration of each new or changed `struct' or `struct'
>> member, global or static variable, `typedef', or enumeration that relates to
>> your frame table. 
>> Identify the purpose of each in roughly 25 words.

---- ALGORITHMS ----

>> B2: When a frame is required but none is free, some frame must be evicted.
>> Describe your code for choosing a frame to evict.

---- SYNCHRONISATION ----

>> B3: When two user processes both need a new frame at the same time, how are 
>> races avoided? 
>> You should consider both when there are and are not free frames available in
>> memory.

>> B4: A page fault in process P can cause another process Q's frame to be
>> evicted. How do you ensure that Q cannot access or modify that page during
>> the eviction process and how do you avoid a race between P evicting Q's frame
>> and Q faulting the page back in?

>> B5: Explain how your synchronisation design prevents deadlock.
>> (You may want to refer to the necessary conditions for deadlock.)

---- RATIONALE ----

>> B6: There is an obvious trade-off between parallelism and the complexity of
>> your synchronisation methods. 
>> Explain where your design falls along this continuum and why you chose to 
>> design it this way.

             MEMORY MAPPED FILES
             ===================

---- DATA STRUCTURES ----

>> C1: Copy here the declaration of each new or changed `struct' or `struct'
>> member, global or static variable, `typedef', or enumeration that relates to
>> your memory mapped file table.
>> Identify the purpose of each in roughly 25 words.

---- ALGORITHMS ----

>> C2: Explain how you determine whether a new file memory mapping overlaps with
>> any existing segment and how you handle such a case.

---- RATIONALE ----

>> C3: File mappings created with "mmap" have similar semantics to those of data 
>> demand-paged from executables. 
>> How does your code-base take advantage of this?
