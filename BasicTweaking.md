**Change frequency and voltage**

If you want to change voltage or frequency in a easy manner, you have to use the -set command switch. It is really easy to use and pretty user friendly.

Let's see a simple example. If you want to set the frequency of your pstate 0 to 2000 Mhz to all cores you just have to launch such a command:

**> TurionPowerControl -­set core all pstate 0 frequency 2000**

The program will warn you if there isn't a perfect combination that matches the frequency you requested, and will round the result.

If you wish to set core voltage too, let's say 1.200 volts, then the command will become like this:

**> TurionPowerControl -­set core all pstate 0 frequency 2000 vcore 1.200**

Again the program will warn you if you had choose a voltage that has no perfect match. Again it will round automatically.

You can also specify different pstates and different settings:

**> TurionPowerControl -­set core all pstate 0 frequency 2000 vcore 1.200 pstate 1 frequency 1000 vcore 1.100 pstate 2 frequency 500 vcore 1.000**

Such a command will set pstate 0 (to all cores) to frequency 2000 Mhz and voltage 1.200v, pstate 1 to frequency 1000 Mhz and voltage 1.100v and pstate 2 to frequency 500 and voltage 1.000v.

If you want to set voltage core to 1.125 to pstate 0 to just a core (let's say core 1), you can issue this command:

**> TurionPowerControl ­set core 1 pstate 0 vcore 1.125**

There's the possibility to use synomims also, to make the command line more compact. There are the following synonims:

– pstate and ps have the same meaning
– frequency, freq and f have the same meaning
– voltage, vcore and vc have the same meaning
– nbvoltage, nbvolt, nbv have the same meaning

so we can write more compactly the large example above this way:

**>TurionPowerControl -­set core all ps 0 f 2000 vc 1.2 ps 1 f 1000 vc 1.100 ps 2 f 500 vc 1.000**

that is pretty shorter (but also less easy to understand).

**Important Note:** if you have a family 10h processor (AMD Phenom, Phenom II or Athlon II) and it is installed in an old AM2 motherboard, you should manipulate **northbridge** voltages.

**Check paragraphs 3.3 and 3.4 of the included documentation if you want more detailed informations**