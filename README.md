CS502
=====

operation system project


This is my CS502 project phase2 (phase1 has already finish)

<I> the compress file include list:
1. source code: 
all functions is put in base.c, but every function has details instructions.
and it has a my header file, call "rli_global.h" define the PCB and other structure information. 
all code are work in linux enviorments, and test on wpi ccc machine (gcc version 4.1)
and my machine  (gcc version 4.6)
             
  it has a makefile in this folder, so use make file             
                       
                "make -f makefile"
      
  to complie the source code to generate execute file 


2. Architectural Documents:
there is 5 page architectural Documents, which include the design list,
high level design diagram , its's Justification and addition feature.



<II> What I finish:  
test2a
test2b
test2c
test2d
test2e (use FIFO)
test2f (use FIFO)



<III> Additional features: 
I use test2e and test2f build my extra test design.
they call test2e_LRU and test2f_LRU, the test code is same as test2e and 2f, 
but they  Approximate LRU algorithm, not FIFO.
so this extra design is compare faults number between the FIFO (test2e & test2f) the LRU (test2e_LRU & test2f_LRU).

For select the extra test, just change the argument to:

./OS test2e_LRU  or  ./OS test2f_LRU



<IV> Other tips:
for convenience to observe the result output and printer output.
The printer switch is added in memory printer and schedule printer.
You can found it in base.c:
Memory_SWITCH and schedule_SWITCH
If they set to SWITCH_ON, it will turn on the printer output.
If they set to SWITCH_OFF, it will turn off the printer output.
 

