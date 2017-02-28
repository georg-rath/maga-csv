GAWK CSV Parser
===============

This extension to gawk parses CSV files natively (libcsv) without using FPAT.
In our tests this has reduced runtime considerably (approx 20x).
Currently this extension requires FS to be set to "\31".

Internally it uses a queue of row buffers to convert the output of
libcsv to the gawk format.
