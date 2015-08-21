#! /bin/bash
echo "This is the body" | mutt -s "Testing mutt" lukekb@gmail.com -a test.jpg
rm test.jpg
exit 1