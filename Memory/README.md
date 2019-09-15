# How To Run
1. make
2. memory_coordinator 2

# The Algorithm Explained
The algorithm poll memory stats periodically and sort the memory stats by available memory a vm has in descending order. It then iterates through the memory stats. For those exceed ABUNDANCE_THRESHOLD, the algorithm collects MEMORY_CHANGE_DELTA amount from them, and for those fall below the ABUNDANCE_THRESHOLD, the algorithm allocates MEMORY_CHANGE_DELTA amount memory to them.
1. ABUNDANCE_THRESHOLD = 100 * 1024
2. HOST_MINIMUM = 50 * 1024
3. MEMORY_CHANGE_DELTA = 20 * 1024

##log files Explained

### log1
There are three stages in the log. \nFirstly, all vm's total memory is dropping. Since all of them exceed ABUNDANCE_THRESHOLD, the algorithm collects memory from them assign them back to hypervisor. But since vm1 is consuming memory, its available memory drops faster. \nSecond stage is when all vm memory drops to near 100 MB. You can see that the inactive three vm memory is ranging between 95~110MB. The reason is that the algorithm is making sure they at least has memory of size of ABUNDANCE_THRESHOLD. At the stage, vm1 still consumes memory. It's available memory drops further down below 100MB. As you can see, from the point on, the algorithm start allocating memory to vm1. \nStage three is when vm1 cleans its allocated memory. The available memory suddenly becomes large. The algorithm starts collecting memory from it till its available memory rangs between 95~110MB.

### log2
Similar to log1, except that all VMs' memory stat behave like the VM1 in log1 since they are all consuming memory.

### log3
There are four stages in the log. \n1. All VMs' consuming memory at the same pace. They all behave like VM1 in log1. \n 2. At some point, VM1 and VM2 clean their memory and stop consuming. You can see their available memory size suddenly becomes so large. Starting from here, the algorithm starts collecting memory from them. \n 3. Some times later, VM3 and VM4's available memory size fall below ABUNDANCE_THRESHOLD. The algorithm starts giving them memory while still collecting memory from VM1 and VM2. \n 4. Finally, VM3 and VM4 also release memory. The algorithm collects memory from them like it did to VM1 and VM2 earlier till they hover around ABUNDANCE_THRESHOLD.
