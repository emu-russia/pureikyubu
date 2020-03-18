echo "Script to debug OSInitAudioSystem DSPInit stub. Run after loading DOL."

dstop
dreset

echo "Run until Welcome message"

dstep 100

echo "Load DSPInit stub"

ramload data\dsp_stub.bin 0x1300000

echo "Execute mailbox sequence to load DSPInit stub and run it"

dspmbox

dstep 100

cpumbox 0x80F3A001
dstep 100
cpumbox 0x1300000
dstep 100

cpumbox 0x80F3A002
dstep 100
cpumbox 128
dstep 100

cpumbox 0x80F3C002
dstep 100
cpumbox 0
dstep 100

cpumbox 0x80F3B001
dstep 100
cpumbox 0
dstep 100

cpumbox 0x80F3B002
dstep 100
cpumbox 0
dstep 100

cpumbox 0x80F3C001
dstep 100
cpumbox 0
dstep 100

cpumbox 0x80F3D001
dstep 100
cpumbox 0

echo "Set breakpoint and go"

dbrkclr
dbrk 0x10

drun
run
