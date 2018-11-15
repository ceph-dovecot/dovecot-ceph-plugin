perl -n -e 'next if (/^doveadm/); if (/^(flags:\s*)(.*)/) {$_ = $1 . join(" ", sort split(" ",$2)) . "\n";}  print;' $1 | sed -e ':1;s/\f$/\f/;t 2;N;b 1;:2;y/\n/\t/' | sort -t  ' ' -k 1 > $2
