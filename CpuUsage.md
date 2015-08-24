TurionPowerControl implements the most precise cpu usage counter available for AMD processors.

It is the most precise simply because it uses processor specific performance counters.
To use the performance usage meter, just issue:

**> TurionPowerControl ­-perf­-cpuusage**

and it will go into daemon mode telling you every second the actual cpu usage of every core. To exit the program while in daemon mode you have to press CTRL-C.

**Note:** if you have a processor with Turbo Core functionality, TurionPowerControl may tell you that some cores are loaded for more than 100%. This is normal and expected. It actually shows you that the Turbo Core functionality is really working on those cores. Also this applies if you overclock your processor.