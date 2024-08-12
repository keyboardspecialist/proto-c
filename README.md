Very early C compilers and language
Several years ago, Paul Vixie and Keith Bostic found a DECtape drive, attached it to a VAX, and offered to read old DECtapes. Even at the time, this was an antiquarian pursuit, and it presented an opportunity to mine beneath the raised floor of the computer room and unearth some of the DECtapes we'd stored since the early 1970s. Gradually, I've been curating some of this, and here offer some of the artifacts. Unfortunately existing tapes lack interesting things like earliest Unix OS source, but some indicative fossils have been prepared for exhibition.
 information: Warren Toomey, now at Bond University, has managed to make one of the compilers (last1120c, see below) compile itself using a First/Second edition Unix emulator for the PDP-11; see his ftp-available directory. More generally, it's worth looking into the PDP-11 Unix Preservation Society pages for sources and simulators.

As described in the C History paper, 1972-73 were the truly formative years in the development of the C language: this is when the transition from typeless B to weakly typed C took place, mediated by the (Neanderthal?) NB language, of which no source seems to survive. It was also the period in which Unix was rewritten in C.

In looking over this material, I have mixed emotions; so much of this stuff is immature and not well-done, and there is an element of embarrassment about displaying it. But at the same time it does capture two moments in a period of creativeness and may have some historical interest.

Two tapes are present here; the first is labeled "last1120c", the second "prestruct-c". I know from distant memory what these names mean: the first is a saved copy of the compiler preserved just as we were abandoning the PDP-11/20, which did not have multiply or divide instructions, but instead a separate, optional unit that did these operations (and also shifts) by storing the operands into memory locations. (A story about using this hardware is told elsewhere.)

"prestruct-c" is a copy of the compiler just before I started changing it to use structures itself.

It's a bit hard to get really accurate dates for these compilers, except that they are certainly 1972-73. There are date bits on the tape image, but they suffer from a possible off-by-a-year error because we changed epochs more than once during this era, and also because the files may have been copied or fiddled after they were the source for the compiler in contemporaneous use.

The earlier compiler does not know about structures at all: the string "struct" does not appear anywhere. The second tape has a compiler that does implement structures in a way that begins to approach their current meaning. Their declaration syntax seems to use () instead of {}, but . and -> for specifying members of a structure itself and members of a pointed-to structure are both there.

Neither compiler yet handled the general declaration syntax of today or even K&R I, with its compound declarators like the one in int **ipp; . The compilers have not yet evolved the notion of compounding of type constructors ("array of pointers to functions", for example). These would appear, though, by 5th or 6th edition Unix (say 1975), as described (in Postscript) in the C manual a couple of years after these versions.

Instead, pointer declarations were written in the style int ip[];. A fossil from this era survives even in modern C, where the notation can be used in declarations of arguments. On the other hand, the later of the two does accept the * notation, even though it doesn't use it. (Evolving compilers written in their own language are careful not to take advantage of their own latest features.)

It's interesting to note that the earlier compiler has a commented-out preparation for a "long" keyword; the later one takes over its slot for "struct." Implementation of long was a few years away.

Aside from their small size, perhaps the most striking thing about these programs is their primitive construction, particularly the many constants strewn throughout; they are used for names of tokens, for example. This is because the preprocessor didn't exist at the time.

A second, less noticeable, but astonishing peculiarity is the space allocation: temporary storage is allocated that deliberately overwrites the beginning of the program, smashing its initialization code to save space. The two compilers differ in the details in how they cope with this. In the earlier one, the start is found by naming a function; in the later, the start is simply taken to be 0. This indicates that the first compiler was written before we had a machine with memory mapping, so the origin of the program was not at location 0, whereas by the time of the second, we had a PDP-11 that did provide mapping. (See the Unix History paper). In one of the files (prestruct-c/c10.c) the kludgery is especially evident.

Links to the source of the compilers are listed below. The files named c0?.c are the first passes, which parse source and writes syntax trees intermingled with some text on an intermediate file. The c1?.c files are the code generators, which read the trees and generate code. The format is straight text (with just NL characters separating lines; the browsers I've tried cope with this).

The code generation technique uses tables of instruction prototypes; a parse tree is recursively matched against the part of the table corresponding to its root operator. Restrictions on the types and complexity of the operands can be expressed, and the table is searched sequentially for the earliest matching fragment. Following each restriction specification is the expansion specification; lower case letters are literal, upper case things are replaced by things from the operands in the tree. This is described in more detail in the paper A Tour through the PDP-11 Compiler. (This reference is troff source; it can also be found in Postscript or PDF forms, though bundled with other papers, under the 7th Edition Manual's home page). But do note that this Tour describes the state of things after several years had passed.

There are four tables specifying how to compile an expression to a register, to compile only for side effects, to compile only to test condition codes, and to compile to push on the stack (used for function arguments, or for temporaries). They were saved only with the "last1120c" compiler; the tables for the later one would have been similar.

The source for the last1120c compiler also has a subsidiary table for each pass with a bit of stuff that was not in the library, and some encoding of facts about various operators as .s (assembler language) files.

Finally, there is the cvopt program, used to convert the nonce-language expression template tables into assembler. With a lot of handwork, there is probably enough material to construct a working version of the last1120c compiler, where "works" means "turns source into PDP-11 assembler." (See the top of the page for one who succeeded.)

The links for the files are:

last1120c
c00.c
c01.c
c02.c
c03.c
c0t.s
c10.c
c11.c
c1t.s
regtab.s
cctab.s
sptab.s
efftab.s
cvopt.c
prestruct-c
c00.c
c01.c
c02.c
c03.c
c10.c
c11.c