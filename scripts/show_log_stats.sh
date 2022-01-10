#!/bin/bash

cat $1 | grep -v 16384 | grep -oP "delay [+-]?([0-9]*[.])?[0-9]+" > block_delay

echo "Block Delay > 2 sec =" $(cat block_delay | awk '{ if ($2 > 2) sum += 1 } END { print sum / NR }') %
echo "Block Delay > 3 sec =" $(cat block_delay | awk '{ if ($2 > 3) sum += 1 } END { print sum / NR }') %
echo "Block Delay > 5 sec =" $(cat block_delay | awk '{ if ($2 > 5) sum += 1 } END { print sum / NR }') %
echo "Block Delay > 6 sec =" $(cat block_delay | awk '{ if ($2 > 6) sum += 1 } END { print sum / NR }') %
echo "Block Delay > 7 sec =" $(cat block_delay | awk '{ if ($2 > 7) sum += 1 } END { print sum / NR }') %
echo "Block Delay > 8 sec =" $(cat block_delay | awk '{ if ($2 > 8) sum += 1 } END { print sum / NR }') %

cat $1 | grep -oP "height [0-9]+ with score [0-9]+, forked at [0-9]+" > forked_at

echo "Fork Depth 1  =" $(cat forked_at | awk '{ if ($2 - $8 == 1) sum += 1 } END { print sum / NR }') %
echo "Fork Depth 2  =" $(cat forked_at | awk '{ if ($2 - $8 == 2) sum += 1 } END { print sum / NR }') %
echo "Fork Depth 3  =" $(cat forked_at | awk '{ if ($2 - $8 == 3) sum += 1 } END { print sum / NR }') %
echo "Fork Depth 4  =" $(cat forked_at | awk '{ if ($2 - $8 == 4) sum += 1 } END { print sum / NR }') %
echo "Fork Depth 5  =" $(cat forked_at | awk '{ if ($2 - $8 == 5) sum += 1 } END { print sum / NR }') %
echo "Fork Depth 6  =" $(cat forked_at | awk '{ if ($2 - $8 == 6) sum += 1 } END { print sum / NR }') %
echo "Fork Depth 7  =" $(cat forked_at | awk '{ if ($2 - $8 == 7) sum += 1 } END { print sum / NR }') %
echo "Fork Depth 8  =" $(cat forked_at | awk '{ if ($2 - $8 == 8) sum += 1 } END { print sum / NR }') %
echo "Fork Depth >8 =" $(cat forked_at | awk '{ if ($2 - $8 > 8) sum += 1 } END { print sum / NR }') %

rm forked_at
rm block_delay
