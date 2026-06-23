// TODO: update README.md

# Massive unification
En este proyecto se implementa un sistema preliminar de unificación masivo para restricciones de igualdad sobre términos finitos. Este sistema se puede optimizar y acelerar mediante el uso de GPUs. En un futuro, se aspira a que una versión evolucionada de este sistema sirva de base para la implementación de un demostrador de teoremas automático para lógica de primer orden.

Trabajo desarrollado en colaboración por:  
- [LoRea EHU](https://www.ehu.eus/es/web/lorea/web-gunea)  
- [ISG EHU](http://www.sc.ehu.es/ccwbayes/)  
  
Contact: javier.alvez@ehu.eus (Javier Álvez), alejandro.perezc@ehu.eus (Alejandro Pérez), montserrat.hermo@ehu.eus (Montserrat Hermo), joseantonio.pascual@ehu.eus (Jose A. Pascual)  

------  
  
In this project, we develop a preliminary massive unification system for equality constraints over finite trees. This system can be optimized and accelerated using GPUs. As future work, we plan to use an evolved version of this system as the basis of a general automated theorem prover for first-order logic.  

Work developed in collaboration by:  
- LoRea EHU (https://www.ehu.eus/es/web/lorea/web-gunea)  
- ISG EHU (http://www.sc.ehu.es/ccwbayes/)  

Contact: javier.alvez@ehu.eus (Javier Álvez), alejandro.perezc@ehu.eus (Alejandro Pérez), montserrat.hermo@ehu.eus (Montserrat Hermo), joseantonio.pascual@ehu.eus (Jose A. Pascual)  

## Functionalites  
- Unification of pairs of equality constraints represented as matrices (M1 and M2) created from the domains COM (problem 123) and AGT (problem 007) of TPTP (www.tptp.org). 
- Checking the result (against M3).
  
  
## Compilation  
`gcc -o c core.c -Wall -Wextra -g -O0 structures.c perf_hash.c -D[COM|AGT] -lm`  
`gcc -o c core.c -Wall -Wextra -O3 structures.c perf_hash.c -D[COM|AGT] -lm`  

## Use
Note that currently there are two types of tests, from COM or AGT. To use the code in each type of test, the binary must be compiled with the correct flag declared (-DAGT or -DCOM). This is because in order to speed up the problem reading from files, a perfect hash is used, and that hash is built from COM or AGT symbol signature respectively.  
`./c /path/to/testXXM1.csv /path/to/testXXM2.csv /path/to/testXXM3.csv [verbose]`

## Extended use  
The `execute_triplets.sh` script provides and easy way to execute all tests composed by M1,M2 and M3 triplets within a folder. Results will be with no verbose by default (recommended), so times will show in a neat csv style file: 

`./execute_triplets.sh /path/to/folder`  
`nohup [time] ./execute_triplets.sh /path/COM123+1 > res_COM.csv 2>&1 &`  

Output example:  
```csv
/path/COM123+1/test0001, M1 blocks 15, M2 blocks 15, OK, 0/105, 14.126102932, 0.705182065, 0.260217489, 0.965399554, 15.149331875
/path/COM123+1/test0170, M1 blocks 264, M2 blocks 264, OK, 0/41418, 61.534173169, 6.006631036, 3.259907399, 9.266538435, 71.027235948
/path/COM123+1/test0003, M1 blocks 12, M2 blocks 12, OK, 0/84, 12.780420899, 0.795690496, 0.239054632, 1.034745128, 13.865813695

```  

## Output  
The expected output for each test triplet includes:  
- The base path to the test
- Number of matrix subsets in M1
- Number of matrix subsets in M2
- If all computed matrix subsets are OK or Not OK
- The number of incorrect resulting matrix subsets by the total number of resulting matrix subsets (if OK, should be 0/XXX)
- The time needed to parse input files (in seg.)
- Time needed to compute unifiers (in seg.)
- Time needed to apply unifiers (in seg.)
- Total combined time for unification (that is, total time minus the file parse and result checking)
- Total execution time (in seg.)

## For memory checking with Valgrind:  
```bash
valgrind --leak-check=full \
         --show-leak-kinds=all \ 
         --track-origins=yes --verbose --log-file=valgrind-out.txt \
         ./c /path/to/testXXM1.csv /path/to/testXXM2.csv \
         /path/to/testXXM3.csv [verbose]
```

## Files    
### core.c  
Functions for matrix computation, unifier handling, etc   
### structures.c  
Functions for managing this project’s data structures (L1, L2, L3, operand_block, result_block, main_term, mgu_schema, etc)
### dictionary.c  
Simple dictionary functionality for various operations of constant/variable element handling    
### perf_hash.c  
Perfect hash used to read symbols (constants) from AGT and COM test files more efficiently  