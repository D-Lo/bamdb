# bamdb

    Program: bamdb (Software for indexing and querying 10x BAMs)
    Version: 0.9 (using htslib 1.5)

    Usage:   bamdb index <required-flag>

    Required flags:
       WGS            index QNAME and BX tags for 10x WGS BAM 
       single-cell    index QNAME, CB, and UB tags for single-cell BAM


e.g. create bamdb index for WGS on BX and QNAME field

bamdb index WGS example.bam

e.g. create bamdb index for WGS on BX and QNAME field, and also MD and 

bamdb index WGS example.bam  --tag=MD --tag=BZ
