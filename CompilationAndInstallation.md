# Introduction #

TurionPowerControl has been reported to compile fine under a number of Linux distribution. These includes Ubuntu, Fedora, Slackware, OpenSuse, ArchLinux, Puppy Linux, etc...

There should be no problems compiling on i386 targets since if doesn't use any specific code, but you can check **Paragraph 1.3 - Linux Requirements** of the included documentation file for more details.

To compile the program for your specific Linux distribution, just use make command to allow automatic compilation.
To compile just put yourself in the source code folder and issue this command:

**> make**

The program now is compiled.


To install the program on Fedora, Suse and other non-Ubuntu based distribution you can write:

**> su**

insert your root password, then write:

**> make install**

And the process is completed. You can invoke TurionPowerControl (as root) from everywhere you wish.

To install the program if you are using Ubuntu based distributions run this command:

**> sudo make install**


**Troubleshooting**

If you find troubles or strange errors when you launch the software, keep in mind you have to be superuser to access and manipulate processor features.

You may also encounter errors about retrieving CPUID informations or access to cpu MSRs, so be sure that your system has the required cpuid and cpumsr modules loaded (if they are not yet compiled inside your kernel). This is most common on Ubuntu distributions. You can load them issuing (always as a superuser):

**> modprobe cpuid**

**> modprobe msr**

If you still can't load these modules, check that your distribution come with them, else I think you can download them using your distribution favourite package manager. Again, check **Paragraph 1.3 – Linux requirements** of the included documentation if you have troubles.