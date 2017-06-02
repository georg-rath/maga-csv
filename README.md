GAWK CSV Parser
===============

This extension to gawk parses CSV files natively (libcsv) without using FPAT.
In our tests this has reduced runtime considerably (approx 20x).
Currently this extension requires FS to be set to "\31".

Internally it uses a queue of row buffers to convert the output of
libcsv to the gawk format.

This has been tested on GAWK 4.1.60 with extension support.

Example
=======


```
BEGIN {
  FS = "\31"
}

{
  print $1 "_" $2
}
```

Run with:

       AWKLIBPATH=$(pwd) gawk -lmaga-csv -f example.awk test_csvs/utf8.csv
