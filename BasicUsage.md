**Getting the list of switches**

To get the list of program available switches, just run the program from a command line with no switches at all, just like this:

**> TurionPowerControl**

**Listing the processor features**

Usually processors have lots of parameters that are useful for tweaking.
To list some of these interesting parameters there are different options. For example, to list the power states table and take a look to some interesting parameters you can just launch the program this way:

**> TurionPowerControl ­-l**

This will drop out a list of setting about your processor, including the power states table, maximum and minimum VIDs, maximum pstate available, and so on... The table that comes from this command is also very useful if you decide to tweak voltage and frequency because it can be taken as a starting point.
To list some other processor informations, specific for the different families, you can use this switch:

**> TurionPowerControl ­-spec**

Other interesting options are listing current Hardware Thermal Control (HTC) parameters. HTC parameters are enumerated as follows:

**> TurionPowerControl -­htc**

And you may wish to read the temperature reported from the internal diodes:

**> TurionPowerControl -­temp**

**Check the included documentation for more detailed informations, in particular paragraphs 3.1 and 3.2**