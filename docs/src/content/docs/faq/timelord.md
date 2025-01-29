---
title: Timelord FAQ
description: Frequently asked questions about MMX Timelord.
---
# Timelord

### _I have the fastest CPU in the world. How do I run the timelord and how would I check/know that I am winning timelord rewards?_
You can run the node with ./run_node.sh --timelord 1. Look for a line that says, "timelord = "true/false.

For a more permanent solution, create a file named `timelord` in your /config/local/ folder and put "true/false" in there and nothing else.

You should know if you're the fastest timelord in the network if your terminal/log says , "Broadcasting VDF for height XXXXX...."

Then you'll be rewarded 0.005 MMX for every block.

For more info, you can read about timelord here: https://github.com/voidxno/mmx-node-notes/wiki/Optimizations-for-TimeLord