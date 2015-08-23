#! /bin/bash
echo "The kids wanted to send you this photo. We don't check this email addres, so no point in replying." | mutt -s "Berndt Photo Booth" $1 -a test.jpg
rm test.jpg
exit 1